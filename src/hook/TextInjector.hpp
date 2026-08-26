// SPDX-License-Identifier: MIT
//
// Keystroke injection.
//
// libuiohook is used for *capture* only. Its hook_post_event() cannot reliably
// produce characters that are not on the user's current keyboard layout, which
// is precisely what this application has to do — no layout has "ế" on it. Each
// platform therefore gets a native backend:
//
//   Windows  SendInput with KEYEVENTF_UNICODE. Arbitrary UTF-16, no layout
//            involvement at all.
//   macOS    CGEventCreateKeyboardEvent + CGEventKeyboardSetUnicodeString,
//            posted to the HID event tap.
//   X11      XTestFakeKeyEvent. Characters outside the layout are typed by
//            temporarily binding a spare keycode to the Unicode keysym
//            (0x01000000 | codepoint), pressing it, and unbinding it again.
//
// The uiohook backend remains as a last-resort fallback (and for Wayland, where
// nothing better is available without a compositor-specific protocol).

#pragma once

#include <memory>
#include <string>

namespace st {

class TextInjector {
public:
    virtual ~TextInjector() = default;

    TextInjector(const TextInjector&)            = delete;
    TextInjector& operator=(const TextInjector&) = delete;

    /// False when the backend could not attach (no X display, no event tap
    /// permission, …). The caller should surface this in the status pane.
    [[nodiscard]] virtual bool available() const = 0;

    /// "SendInput", "CGEvent", "XTest", "uiohook" — shown in the settings UI.
    [[nodiscard]] virtual std::string backendName() const = 0;

    /// Reason the backend is unavailable, empty when it is fine.
    [[nodiscard]] virtual std::string lastError() const { return {}; }

    virtual void sendBackspaces(int count) = 0;
    virtual void sendText(const std::u32string& text) = 0;

    /// Convenience: the whole edit in one go, in the right order.
    void applyEdit(int backspaces, const std::u32string& text)
    {
        if (backspaces > 0)
            sendBackspaces(backspaces);
        if (!text.empty())
            sendText(text);
    }

    /// Number of individual key events `applyEdit` will generate, so the hook
    /// can pre-charge its "ignore my own events" counter.
    [[nodiscard]] static int keyEventCount(int backspaces, const std::u32string& text);

    /// Best backend for this platform; never returns nullptr (falls back to the
    /// uiohook backend, which may itself report available() == false).
    [[nodiscard]] static std::unique_ptr<TextInjector> create();

protected:
    TextInjector() = default;
};

/// Platform backends. Each is compiled only on its own platform; the others are
/// not even added to the target (see CMakeLists.txt).
[[nodiscard]] std::unique_ptr<TextInjector> createWindowsInjector();
[[nodiscard]] std::unique_ptr<TextInjector> createMacInjector();
[[nodiscard]] std::unique_ptr<TextInjector> createX11Injector();
[[nodiscard]] std::unique_ptr<TextInjector> createUiohookInjector();

} // namespace st
