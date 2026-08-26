// SPDX-License-Identifier: MIT
//
// X11 injection backend.
//
// XTestFakeKeyEvent can only press keycodes that exist in the current keymap,
// and no layout contains "ế". The standard workaround — used by xdotool and by
// most Linux IMEs that bypass XIM — is to borrow keycodes the layout does not
// use, bind them to Unicode keysyms (0x01000000 | codepoint), press them, and
// unbind them again.
//
// Three details separate "usually works" from "always works". All three were
// found by injecting into a real X server with the XRecord hook running, which
// is the configuration that actually matters here:
//
//  1. Pacing. Back-to-back XTestFakeKeyEvent requests, all stamped CurrentTime,
//     are not reliably delivered while another client is recording the input
//     stream. One XSync and a short pause per event fixes it; xdotool's --delay
//     exists for the same reason.
//
//  2. Never mix delivery paths inside one string. A press of a *real* keycode
//     issued right after a keymap change is regularly swallowed — the server
//     has just broadcast MappingNotify and that one event goes missing. "ä"
//     followed by "p" reproducibly lost the "p". So a string is sent either
//     entirely with real keys or entirely with borrowed ones.
//
//  3. Remap the whole string in one go. Borrowing one keycode per character and
//     mapping them all in a single batch turns an N-character injection from N
//     settle windows into one, which keeps the whole edit short enough that a
//     fast typist's next keystroke cannot land in the middle of it. That matters
//     on X11 specifically, because XRecord cannot suppress the original
//     keystroke and the edit is therefore racing the user.
//
//  4. Make the injection immune to modifiers instead of fighting them. A
//     synthetic keypress is interpreted against whatever modifiers are
//     physically held at that instant, so an injected "n" arrives as "N" if the
//     user is already reaching for the Shift of the next word. xdotool solves
//     this by releasing the held modifiers first (--clearmodifiers), but that
//     is wrong for an IME: by the time the edit finishes the user may have let
//     Shift go, and pressing it again leaves it stuck down — everything after
//     comes out in capitals. Binding *both* shift levels of the borrowed
//     keycode to the same keysym makes Shift and Caps Lock irrelevant to the
//     character produced, with nothing to restore afterwards. Backspace is left
//     as a real key: the engine never emits an edit while Ctrl or Alt is held,
//     because a chord belongs to the focused application.
//
// A dedicated Display connection is used so injection never races with the
// XRecord connection libuiohook owns.

#include "hook/TextInjector.hpp"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>

#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace st {
namespace {

/// How long to let a keymap change settle before pressing anything.
constexpr auto kMappingSettle = std::chrono::milliseconds(2);

/// Pause between synthetic key events. See note 1 above.
constexpr auto kKeyPace = std::chrono::milliseconds(1);

/// Pause before the first event of an edit. See tapKey().
constexpr auto kLeadIn = std::chrono::milliseconds(2);

/// A gap longer than this means a new edit is starting rather than the next
/// character of the current one.
constexpr auto kEditGap = std::chrono::milliseconds(40);

/// Upper bound on borrowed keycodes. Edits are one to three characters in
/// practice; eight leaves room for a macro expansion without chunking.
constexpr std::size_t kMaxScratchKeys = 8;

class X11Injector final : public TextInjector {
public:
    X11Injector()
    {
        display_ = XOpenDisplay(nullptr);
        if (display_ == nullptr) {
            error_ = "Cannot open the X display (is DISPLAY set?).";
            return;
        }
        int eventBase = 0, errorBase = 0, major = 0, minor = 0;
        if (XTestQueryExtension(display_, &eventBase, &errorBase, &major, &minor) == 0) {
            error_ = "The XTEST extension is not available on this display server.";
            XCloseDisplay(display_);
            display_ = nullptr;
            return;
        }
        scratch_ = findScratchKeycodes();
        if (scratch_.empty())
            error_ = "No free keycode available for Unicode injection.";
    }

    ~X11Injector() override
    {
        if (display_ != nullptr) {
            releaseScratch(scratch_.size());
            XCloseDisplay(display_);
        }
    }

    [[nodiscard]] bool available() const override
    {
        return display_ != nullptr && !scratch_.empty();
    }
    [[nodiscard]] std::string backendName() const override { return "XTest"; }
    [[nodiscard]] std::string lastError() const override { return error_; }

    void sendBackspaces(int count) override
    {
        if (!available() || count <= 0)
            return;
        const KeyCode backspace = XKeysymToKeycode(display_, XK_BackSpace);
        if (backspace == 0)
            return;

        for (int i = 0; i < count; ++i)
            tapKey(backspace);
        XFlush(display_);
    }

    void sendText(const std::u32string& text) override
    {
        if (!available() || text.empty())
            return;

        // Every character goes through a borrowed keycode, even the ones the
        // layout already has. See note 2 in the file header for why the two
        // paths must not be mixed, and note 4 for why borrowed keycodes are the
        // safe choice: both shift levels of a borrowed key carry the same
        // keysym, so the character produced cannot be changed by Shift, Caps
        // Lock or an AltGr level the user happens to be holding.
        for (std::size_t offset = 0; offset < text.size(); offset += scratch_.size()) {
            const std::size_t count = std::min(scratch_.size(), text.size() - offset);
            bindScratch(text, offset, count);
            for (std::size_t i = 0; i < count; ++i)
                tapKey(scratch_[i]);
            releaseScratch(count);
        }
        XFlush(display_);
    }

private:
    /// Latin-1 maps 1:1 onto the classic keysym range; everything else uses the
    /// Unicode keysym encoding.
    [[nodiscard]] static KeySym keysymFor(char32_t cp) noexcept
    {
        return (cp >= 0x20 && cp <= 0xFF) ? static_cast<KeySym>(cp)
                                          : static_cast<KeySym>(0x01000000u | cp);
    }

    /// Bind `count` characters starting at `offset` onto the borrowed keycodes,
    /// as one batch with a single settle window.
    void bindScratch(const std::u32string& text, std::size_t offset, std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i) {
            const KeySym keysym = keysymFor(text[offset + i]);
            KeySym       mapping[2] = {keysym, keysym};
            XChangeKeyboardMapping(display_, scratch_[i], 2, mapping, 1);
        }
        settleMapping();
    }

    /// Unbind the borrowed keycodes again, so a crash mid-word cannot leave the
    /// user's keymap altered.
    void releaseScratch(std::size_t count)
    {
        if (scratch_.empty())
            return;
        for (std::size_t i = 0; i < count && i < scratch_.size(); ++i) {
            KeySym none[2] = {NoSymbol, NoSymbol};
            XChangeKeyboardMapping(display_, scratch_[i], 2, none, 1);
        }
        settleMapping();
    }

    /// Flush the keymap change, absorb our own MappingNotify so this client's
    /// cached keymap is coherent again, and give the other clients a moment to
    /// do the same before anything is pressed.
    void settleMapping()
    {
        XSync(display_, False);
        XEvent event;
        while (XCheckTypedEvent(display_, MappingNotify, &event) == True)
            XRefreshKeyboardMapping(&event.xmapping);
        std::this_thread::sleep_for(kMappingSettle);
    }

    /// One press/release pair, paced and synced.
    ///
    /// The first tap of an edit also waits out `kLeadIn`. The edit is a reaction
    /// to a keystroke the server has only just finished delivering — on X11 that
    /// keystroke was not suppressed, so it really did reach the focused window —
    /// and a synthetic event issued immediately behind it is unreliable,
    /// especially when it presses the very same key the user just released
    /// (which is exactly what auto-restore does: it retypes the space that ended
    /// the word). Waiting a few milliseconds first makes it deterministic.
    void tapKey(KeyCode keycode)
    {
        if (keycode == 0)
            return;

        const auto now = std::chrono::steady_clock::now();
        if (now - lastTap_ > kEditGap)
            std::this_thread::sleep_for(kLeadIn);
        lastTap_ = now;

        XTestFakeKeyEvent(display_, keycode, True, CurrentTime);
        XSync(display_, False);
        std::this_thread::sleep_for(kKeyPace);
        XTestFakeKeyEvent(display_, keycode, False, CurrentTime);
        XSync(display_, False);
        std::this_thread::sleep_for(kKeyPace);
    }

    /// Keycodes that map to NoSymbol in every group, searched from the top of
    /// the range because that is where layouts leave gaps.
    [[nodiscard]] std::vector<KeyCode> findScratchKeycodes()
    {
        int minKeycode = 0, maxKeycode = 0;
        XDisplayKeycodes(display_, &minKeycode, &maxKeycode);

        int     symsPerCode = 0;
        KeySym* syms        = XGetKeyboardMapping(display_, static_cast<KeyCode>(minKeycode),
                                                  maxKeycode - minKeycode + 1, &symsPerCode);
        if (syms == nullptr)
            return {};

        std::vector<KeyCode> found;
        for (int code = maxKeycode; code >= minKeycode && found.size() < kMaxScratchKeys; --code) {
            bool free = true;
            for (int s = 0; s < symsPerCode; ++s)
                if (syms[(code - minKeycode) * symsPerCode + s] != NoSymbol) {
                    free = false;
                    break;
                }
            if (free)
                found.push_back(static_cast<KeyCode>(code));
        }
        XFree(syms);
        return found;
    }

    Display*             display_ = nullptr;
    std::vector<KeyCode> scratch_;
    std::string          error_;
    std::chrono::steady_clock::time_point lastTap_{};
};

} // namespace

std::unique_ptr<TextInjector> createX11Injector()
{
    return std::make_unique<X11Injector>();
}

} // namespace st
