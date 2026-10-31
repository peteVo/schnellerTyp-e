// SPDX-License-Identifier: MIT
#include "hook/HookService.hpp"

#include "core/Unicode.hpp"

#include <uiohook.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <thread>

#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
#include <windows.h>
#include "hook/WindowsInputTag.hpp"
#endif

namespace st {
namespace {

/// libuiohook's dispatch callback is a C function pointer, and in 1.2 it does
/// not carry a user pointer at all. Exactly one service owns the hook at a
/// time, so a file-static pointer is the simplest correct bridge.
std::atomic<HookService*> gActiveService{nullptr};

/// True when the named environment variable is set to something non-empty.
///
/// MSVC deprecates std::getenv (C4996) in favour of _dupenv_s, which allocates
/// rather than returning a pointer into the environment block. Wrapping both
/// keeps the call site honest instead of silencing the warning wholesale.
[[nodiscard]] bool environmentFlag(const char* name)
{
#if defined(_MSC_VER)
    char*       value  = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
        return false;
    const bool set = value[0] != '\0';
    std::free(value);
    return set;
#else
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
#endif
}

/// SCHNELLERTYPE_DEBUG=1 traces every keystroke and every edit to stderr. The
/// hook is the one part of the program that cannot be unit-tested, so it keeps
/// a way to see what it is doing on a real desktop.
const bool gTrace = environmentFlag("SCHNELLERTYPE_DEBUG");

/// How long after an injection the hook keeps ignoring key events. Long enough
/// for the platform to deliver the events we just generated, short enough that
/// it cannot swallow a keystroke from anyone typing at a human speed
/// (240 words per minute is still one keystroke every 50 ms).
constexpr auto kSelfEventGrace = std::chrono::milliseconds(15);

#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
HHOOK gMetadataHook = nullptr;

// Keyboard processing owns a local result. libuiohook's shared event object is
// unsuitable for reentrant Windows callbacks; it remains the mouse/lifecycle host.
LRESULT CALLBACK metadataHook(int code, WPARAM message, LPARAM data)
{
    if (code == HC_ACTION
        && reinterpret_cast<const KBDLLHOOKSTRUCT*>(data)->dwExtraInfo == kWindowsInputTag) {
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
            if (auto* service = gActiveService.load())
                service->acknowledgeSyntheticPress();
        return 0;
    }
    if (code == HC_ACTION)
        if (auto* service = gActiveService.load())
            if (service->dispatchWindowsKey(message, static_cast<std::uintptr_t>(data)))
                return 1;
    return CallNextHookEx(nullptr, code, message, data);
}

struct FocusTarget {
    std::uintptr_t foreground;
    std::uintptr_t focus;
};

[[nodiscard]] FocusTarget foregroundFocus()
{
    const HWND foreground = GetForegroundWindow();
    const DWORD thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
    GUITHREADINFO info{};
    info.cbSize = static_cast<DWORD>(sizeof(info));
    const HWND focus = thread && GetGUIThreadInfo(thread, &info) && info.hwndFocus
                           ? info.hwndFocus : foreground;
    return {reinterpret_cast<std::uintptr_t>(foreground),
            reinterpret_cast<std::uintptr_t>(focus)};
}

[[nodiscard]] bool isCurrentProcessWindow(std::uintptr_t window)
{
    DWORD process = 0;
    if (window)
        GetWindowThreadProcessId(reinterpret_cast<HWND>(window), &process);
    return process == GetCurrentProcessId();
}
#endif

#if defined(SCHNELLERTYPE_UIOHOOK_DISPATCH_USERDATA)
void dispatchProc(uiohook_event* const event, void* /*userData*/)
{
    HookService::dispatchTrampoline(event);
}
#else
void dispatchProc(uiohook_event* const event)
{
    HookService::dispatchTrampoline(event);
}
#endif

[[nodiscard]] const char* describeStatus(int code)
{
    switch (code) {
    case UIOHOOK_SUCCESS:             return "";
    case UIOHOOK_ERROR_OUT_OF_MEMORY: return "Out of memory while installing the hook.";
    case UIOHOOK_ERROR_X_OPEN_DISPLAY: return "Cannot open the X display.";
    case UIOHOOK_ERROR_X_RECORD_NOT_FOUND:
        return "The XRecord extension is missing. Enable it on the X server.";
    case UIOHOOK_ERROR_X_RECORD_ALLOC_RANGE:
    case UIOHOOK_ERROR_X_RECORD_CREATE_CONTEXT:
    case UIOHOOK_ERROR_X_RECORD_ENABLE_CONTEXT:
    case UIOHOOK_ERROR_X_RECORD_GET_CONTEXT:
        return "XRecord could not be initialised.";
    case UIOHOOK_ERROR_SET_WINDOWS_HOOK_EX:
        return "SetWindowsHookEx failed. Another program may already own a "
               "low-level keyboard hook.";
    case UIOHOOK_ERROR_AXAPI_DISABLED:
        return "Accessibility access is disabled. Enable schnellerTyp-e in "
               "System Settings > Privacy & Security > Accessibility.";
    case UIOHOOK_ERROR_CREATE_EVENT_PORT:
    case UIOHOOK_ERROR_CREATE_RUN_LOOP_SOURCE:
    case UIOHOOK_ERROR_GET_RUNLOOP:
    case UIOHOOK_ERROR_CREATE_OBSERVER:
        return "The macOS event tap could not be created.";
    default:
        return "The keyboard hook failed to start.";
    }
}

void subtractPending(std::atomic_int& pending, int count)
{
    int current = pending.load(std::memory_order_acquire);
    while (count > 0 && !pending.compare_exchange_weak(
               current, current > count ? current - count : 0,
               std::memory_order_acq_rel, std::memory_order_acquire)) {}
}

} // namespace

// ---------------------------------------------------------------------------

HookService::HookService() : injector_(TextInjector::create())
{
    std::scoped_lock lock(statusMutex_);
    status_.injectorBackend = injector_ ? injector_->backendName() : "none";
    status_.canSuppress     = platform::canSuppressEvents();
    status_.permission      = platform::checkPermissions();
}

HookService::~HookService() { stop(); }

void HookService::acknowledgeSyntheticPress()
{
    subtractPending(syntheticPending_, 1);
}

bool HookService::dispatchWindowsKey(std::uintptr_t message, std::uintptr_t data)
{
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
    const auto& native = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool released = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (!pressed && !released)
        return false;
    const auto vk = native.vkCode;
    auto down = [&](int code) {
        return native.vkCode == static_cast<DWORD>(code) ? pressed
                                                       : (GetAsyncKeyState(code) & 0x8000) != 0;
    };
    const bool shift = down(VK_LSHIFT) || down(VK_RSHIFT);
    const bool control = down(VK_LCONTROL) || down(VK_RCONTROL);
    const bool alt = down(VK_LMENU) || down(VK_RMENU);
    const bool meta = down(VK_LWIN) || down(VK_RWIN);
    std::uint16_t mask = 0;
    if (shift) mask |= MASK_SHIFT_L;
    if (control) mask |= MASK_CTRL_L;
    if (alt) mask |= MASK_ALT_L;
    if (meta) mask |= MASK_META_L;
    std::uint16_t modifier = 0;
    switch (vk) {
    case VK_LSHIFT: case VK_RSHIFT: modifier = VC_SHIFT_L; break;
    case VK_LCONTROL: case VK_RCONTROL: modifier = VC_CONTROL_L; break;
    case VK_LMENU: case VK_RMENU: modifier = VC_ALT_L; break;
    case VK_LWIN: case VK_RWIN: modifier = VC_META_L; break;
    }
    if (released) {
        trackChordRelease(modifier, mask);
        return false;
    }

    const auto target = foregroundFocus();
    if (!target.foreground || isCurrentProcessWindow(target.foreground)) {
        resetEngineState();
        return false;
    }
    if (target.foreground != foregroundWindow_ || target.focus != focusWindow_) {
        discardQueuedEdits();
        foregroundWindow_ = target.foreground;
        focusWindow_ = target.focus;
        resetEngineState();
    }
    trackChordPress(modifier, mask);
    if (!enabled_.load())
        return false;
    if (vk == VK_CAPITAL)
        mapper_.setCapsLock(!mapper_.capsLock());
    KeyEvent key;
    key.ctrl = control;
    key.alt = alt;
    key.shift = shift;
    key.meta = meta;
    key.rawCode = static_cast<std::uint16_t>(vk);
    if (modifier || vk == VK_CAPITAL || vk == VK_NUMLOCK || vk == VK_SCROLL) {
        key.kind = KeyKind::Modifier;
    } else {
        switch (vk) {
        case VK_BACK: key.kind = KeyKind::Backspace; break;
        case VK_RETURN: key.kind = KeyKind::Enter; break;
        case VK_TAB: key.kind = KeyKind::Tab; break;
        case VK_ESCAPE: key.kind = KeyKind::Escape; break;
        case VK_SPACE: key.kind = KeyKind::Space; key.character = U' '; break;
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_HOME:
        case VK_END: case VK_PRIOR: case VK_NEXT: case VK_DELETE: case VK_INSERT:
            key.kind = KeyKind::Navigation; break;
        default: {
            if (const auto override = mapper_.overrideFor(static_cast<std::uint16_t>(native.scanCode))) {
                const bool upper = shift ^ (mapper_.capsLock() && unicode::isAsciiAlpha(override->plain));
                key.kind = KeyKind::Character;
                key.character = upper ? override->shift : override->plain;
                break;
            }
            BYTE state[256]{};
            GetKeyboardState(state);
            state[VK_SHIFT] = shift ? 0x80 : 0;
            state[VK_CONTROL] = control ? 0x80 : 0;
            state[VK_MENU] = alt ? 0x80 : 0;
            state[VK_CAPITAL] = mapper_.capsLock() ? 1 : 0;
            state[vk & 0xff] |= 0x80;
            WCHAR text[4]{};
            const DWORD thread = GetWindowThreadProcessId(reinterpret_cast<HWND>(target.foreground), nullptr);
            const int count = ToUnicodeEx(vk, native.scanCode, state, text, 4, 4,
                                          GetKeyboardLayout(thread));
            if (count == 1 && text[0] >= 0x20) {
                key.kind = KeyKind::Character;
                key.character = text[0];
            }
            break;
        }
        }
    }

    const auto generation = editGeneration_.load(std::memory_order_acquire);
    EngineResult result;
    try {
        std::scoped_lock lock(engineMutex_);
        if (!engine_)
            return false;
        result = engine_->processKey(key);
    } catch (const std::exception& error) {
        enabled_.store(false);
        discardQueuedEdits();
        resetEngineState();
        publish(State::Failed, error.what());
        return false;
    }
    if (result.handled) {
        if (!result.isNoop())
            enqueue(Edit{result.backspaces, std::move(result.text)}, generation);
        return true;
    }
    if (!key.isChord() && syntheticPending_.load(std::memory_order_acquire) > 0) {
        if (key.kind == KeyKind::Character || key.kind == KeyKind::Space) {
            enqueue(Edit{0, std::u32string(1, key.character)}, generation);
            return true;
        }
        if (key.kind == KeyKind::Backspace) {
            enqueue(Edit{1, {}}, generation);
            return true;
        }
        if (key.kind == KeyKind::Enter || key.kind == KeyKind::Tab || key.kind == KeyKind::Navigation) {
            Edit edit;
            edit.virtualKey = static_cast<std::uint16_t>(vk);
            enqueue(std::move(edit), generation);
            return true;
        }
    }
#else
    (void)message;
    (void)data;
#endif
    return false;
}

// ---------------------------------------------------------------------------

void HookService::setStatusCallback(StatusCallback callback)
{
    std::scoped_lock lock(statusMutex_);
    statusCallback_ = std::move(callback);
}

HookService::Status HookService::status() const
{
    std::scoped_lock lock(statusMutex_);
    return status_;
}

void HookService::publish(State state, std::string message)
{
    Status         copy;
    StatusCallback callback;
    {
        std::scoped_lock lock(statusMutex_);
        status_.state           = state;
        status_.message         = std::move(message);
        status_.injectorBackend = injector_ ? injector_->backendName() : "none";
        status_.canSuppress     = platform::canSuppressEvents();
        status_.permission      = platform::checkPermissions();
        copy                    = status_;
        callback                = statusCallback_;
    }
    if (callback)
        callback(std::move(copy));
}

void HookService::setEnabled(bool enabled)
{
    enabled_.store(enabled);
    discardQueuedEdits();
    resetEngineState();
}

void HookService::setEngine(EnginePtr engine)
{
    discardQueuedEdits();
    std::scoped_lock lock(engineMutex_);
    engine_ = std::move(engine);
    if (engine_)
        engine_->reset();
}

void HookService::resetEngineState()
{
    std::scoped_lock lock(engineMutex_);
    if (engine_)
        engine_->reset();
}

// ---------------------------------------------------------------------------

bool HookService::start()
{
    if (running_.load())
        return true;

    const platform::PermissionInfo permission = platform::checkPermissions();
    if (permission.state == platform::PermissionState::Denied) {
        publish(State::Failed, permission.detail);
        return false;
    }

    HookService* expected = nullptr;
    if (!gActiveService.compare_exchange_strong(expected, this)) {
        publish(State::Failed, "Another hook service is already running in this process.");
        return false;
    }

    stopRequested_.store(false);
    publish(State::Starting, "Installing the keyboard hook…");

    injectorQuit_.store(false);
    injectorThread_ = std::thread([this] { injectorLoop(); });
    hookThread_     = std::thread([this] { hookLoop(); });
    return true;
}

void HookService::stop()
{
    if (hookThread_.joinable()) {
        stopRequested_.store(true);
        hook_stop();
        hookThread_.join();
    }
    if (injectorThread_.joinable()) {
        injectorQuit_.store(true);
        queueCv_.notify_all();
        injectorThread_.join();
    }
    gActiveService.store(nullptr);
    running_.store(false);
}

void HookService::hookLoop()
{
#if defined(SCHNELLERTYPE_UIOHOOK_DISPATCH_USERDATA)
    hook_set_dispatch_proc(&dispatchProc, this);
#else
    hook_set_dispatch_proc(&dispatchProc);
#endif

    const int code = hook_run();

#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
    if (gMetadataHook) {
        UnhookWindowsHookEx(gMetadataHook);
        gMetadataHook = nullptr;
    }
#endif

    running_.store(false);
    if (code == UIOHOOK_SUCCESS || stopRequested_.load())
        publish(State::Stopped, "Keyboard hook stopped.");
    else
        publish(State::Failed, describeStatus(code));
}

// ---------------------------------------------------------------------------

void HookService::dispatchTrampoline(void* uiohookEvent)
{
    if (HookService* service = gActiveService.load(); service != nullptr)
        service->handleEvent(uiohookEvent);
}

void HookService::handleEvent(void* rawEvent)
{
    auto* event = static_cast<uiohook_event*>(rawEvent);

    switch (event->type) {
    case EVENT_HOOK_ENABLED:
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
        gMetadataHook = SetWindowsHookExW(WH_KEYBOARD_LL, metadataHook, GetModuleHandleW(nullptr), 0);
        if (!gMetadataHook) {
            enabled_.store(false);
            publish(State::Failed, "Windows could not initialise safe keyboard capture.");
            return;
        }
        mapper_.setCapsLock((GetKeyState(VK_CAPITAL) & 1) != 0);
#endif
        running_.store(true);
        publish(State::Running, "Keyboard hook active.");
        return;

    case EVENT_HOOK_DISABLED:
        running_.store(false);
        return;

    case EVENT_MOUSE_PRESSED:
    case EVENT_MOUSE_WHEEL:
        // The caret probably moved, so the word buffer no longer describes
        // what is on screen. A click also ends any chord in progress: a
        // Ctrl+Shift+click is a selection gesture, not a language switch.
        cancelChord();
        discardQueuedEdits();
        resetEngineState();
        return;

    case EVENT_KEY_PRESSED:
#if !defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
        handleKeyPressed(rawEvent);
#endif
        return;

    case EVENT_KEY_RELEASED:
        // Releases are otherwise of no interest: the engines are driven by
        // presses, and the injectors never synthesise a modifier, so nothing
        // here can be an event of our own.
#if !defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
        trackChordRelease(static_cast<std::uint16_t>(event->data.keyboard.keycode),
                          static_cast<std::uint16_t>(event->mask));
#endif
        return;

    default:
        return;
    }
}

void HookService::handleKeyPressed(void* rawEvent)
{
    auto* event = static_cast<uiohook_event*>(rawEvent);

    if (gTrace)
        std::fprintf(stderr, "[st] key vc=%u pending=%d\n",
                     unsigned(event->data.keyboard.keycode), syntheticPending_.load());

    // Drop the key events we generated ourselves.
#if !defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
    if (syntheticPending_.load(std::memory_order_acquire) > 0) {
        syntheticPending_.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }
#endif

    const std::uint64_t generation = editGeneration_.load(std::memory_order_acquire);

    // Before the master switch, not after: switching the IME back on is one of
    // the things the chord exists to do, and it cannot do that from a branch
    // that only runs while it is already on.
    trackChordPress(static_cast<std::uint16_t>(event->data.keyboard.keycode),
                    static_cast<std::uint16_t>(event->mask));

    if (!enabled_.load())
        return;

    if (event->data.keyboard.keycode == VC_CAPS_LOCK)
        mapper_.setCapsLock(!mapper_.capsLock());

    const KeyEvent key = mapper_.fromPressed(event->data.keyboard.keycode,
                                             static_cast<std::uint16_t>(event->mask));

    EngineResult result;
    {
        std::scoped_lock lock(engineMutex_);
        if (!engine_)
            return;
        result = engine_->processKey(key);
    }

    if (!result.handled) {
        return;
    }

    if (platform::canSuppressEvents()) {
        // Legacy macOS: consume the keystroke, then perform the edit. Note
        // that a "swallow" result (an escape key) has no edit at all, which is
        // exactly what the flag below achieves on its own.
        event->reserved |= 0x01;
        if (result.isNoop())
            return;
        enqueue(Edit{result.backspaces, std::move(result.text)}, generation);
    } else {
        // X11: XRecord observes, it cannot consume, so the character the user
        // typed has already reached the application. Erase it along with
        // whatever else this edit replaces.
        if (result.isNoop())
            enqueue(Edit{1, {}}, generation);  // swallow-equivalent: rub out the stray char
        else
            enqueue(Edit{result.backspaces + 1, std::move(result.text)}, generation);
    }
}

// --- modifier-only shortcut ------------------------------------------------
//
// Deliberately driven by libuiohook's modifier mask rather than by tracking
// which physical keys are held. libuiohook updates the mask before dispatching
// a modifier event — set on press, cleared on release — so the mask on the
// event is already the state *after* that key changed. That makes left and
// right Ctrl interchangeable for free, and it self-heals: if a release is
// missed because the window lost focus mid-chord, the next event carries the
// truth instead of a counter that has drifted.

namespace {

enum ModifierBit : unsigned {
    ModCtrl  = 1u << 0,
    ModShift = 1u << 1,
    ModAlt   = 1u << 2,
};

[[nodiscard]] unsigned modifiersFromMask(std::uint16_t mask)
{
    // The _L/_R constants, spelled out and parenthesised. uiohook's combined
    // MASK_CTRL is `MASK_CTRL_L | MASK_CTRL_R` with no parentheses of its own,
    // so `mask & MASK_CTRL` parses as `(mask & MASK_CTRL_L) | MASK_CTRL_R` and
    // is never zero — the chord would have fired on any single modifier.
    // KeyMapper::fromPressed() already avoids the macro for the same reason.
    unsigned bits = 0;
    if ((mask & (MASK_CTRL_L | MASK_CTRL_R)) != 0)
        bits |= ModCtrl;
    if ((mask & (MASK_SHIFT_L | MASK_SHIFT_R)) != 0)
        bits |= ModShift;
    if ((mask & (MASK_ALT_L | MASK_ALT_R)) != 0)
        bits |= ModAlt;
    return bits;
}

/// The modifiers a chord requires, or 0 for Chord::None.
[[nodiscard]] unsigned modifiersFor(HookService::Chord chord)
{
    switch (chord) {
    case HookService::Chord::CtrlShift: return ModCtrl | ModShift;
    case HookService::Chord::CtrlAlt:   return ModCtrl | ModAlt;
    case HookService::Chord::AltShift:  return ModAlt | ModShift;
    case HookService::Chord::None:      break;
    }
    return 0;
}

/// The modifier a key *is*, or 0 when it is an ordinary key.
[[nodiscard]] unsigned modifierForKeycode(std::uint16_t keycode)
{
    switch (keycode) {
    case VC_CONTROL_L:
    case VC_CONTROL_R: return ModCtrl;
    case VC_SHIFT_L:
    case VC_SHIFT_R:   return ModShift;
    case VC_ALT_L:
    case VC_ALT_R:     return ModAlt;
    default:           return 0;
    }
}

} // namespace

void HookService::trackChordPress(std::uint16_t keycode, std::uint16_t mask)
{
    const unsigned wanted = modifiersFor(chord_.load());
    if (wanted == 0)
        return;

    if (modifierForKeycode(keycode) == 0) {
        // An ordinary key went down while the modifiers were held, so this is
        // a normal shortcut or normal typing. Stand down until every modifier
        // has been released and pressed again.
        chordArmed_ = false;
        return;
    }

    // Arm only on the press that completes the set, so Ctrl alone never arms.
    if ((modifiersFromMask(mask) & wanted) == wanted)
        chordArmed_ = true;
}

void HookService::trackChordRelease(std::uint16_t keycode, std::uint16_t mask)
{
    const unsigned wanted = modifiersFor(chord_.load());
    if (wanted == 0 || !chordArmed_)
        return;

    const unsigned released = modifierForKeycode(keycode);
    if ((released & wanted) == 0)
        return;

    // Fire on the first of the chord's modifiers to come back up, then stand
    // down so letting go of the second one does not fire again.
    chordArmed_ = false;
    if (gTrace)
        std::fprintf(stderr, "[st] chord fired (mask now 0x%04X)\n", unsigned(mask));
    if (shortcutCallback_)
        shortcutCallback_();
}

HookService::Chord HookService::chordFromString(std::string_view text)
{
    if (text == "ctrl+shift")
        return Chord::CtrlShift;
    if (text == "ctrl+alt")
        return Chord::CtrlAlt;
    if (text == "alt+shift")
        return Chord::AltShift;
    return Chord::None;
}

const char* HookService::chordToString(Chord chord)
{
    switch (chord) {
    case Chord::CtrlShift: return "ctrl+shift";
    case Chord::CtrlAlt:   return "ctrl+alt";
    case Chord::AltShift:  return "alt+shift";
    case Chord::None:      break;
    }
    return "none";
}

const char* HookService::chordDisplayName(Chord chord)
{
    switch (chord) {
#if defined(SCHNELLERTYPE_PLATFORM_MACOS)
    case Chord::CtrlShift: return "Control + Shift";
    case Chord::CtrlAlt:   return "Control + Option";
    case Chord::AltShift:  return "Option + Shift";
#else
    case Chord::CtrlShift: return "Ctrl + Shift";
    case Chord::CtrlAlt:   return "Ctrl + Alt";
    case Chord::AltShift:  return "Alt + Shift";
#endif
    case Chord::None:      break;
    }
    return "None";
}

void HookService::setShortcutCallback(ShortcutCallback callback)
{
    shortcutCallback_ = std::move(callback);
}

// ---------------------------------------------------------------------------

void HookService::enqueue(Edit edit, std::uint64_t generation)
{
    // Charge the counter before the events exist, so the hook can never see one
    // of them while the counter is still zero. Only presses are counted: the
    // hook only inspects EVENT_KEY_PRESSED.
    const int presses = TextInjector::keyEventCount(edit.backspaces, edit.text) / 2 + (edit.virtualKey ? 1 : 0);
    edit.generation = generation;
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
    edit.foreground = foregroundWindow_;
    edit.focus = focusWindow_;
#endif
    if (gTrace)
        std::fprintf(stderr, "[st] edit bs=%d text=%s presses=%d\n", edit.backspaces,
                     unicode::toUtf8(edit.text).c_str(), presses);
    {
        std::scoped_lock lock(queueMutex_);
        if (generation != editGeneration_.load(std::memory_order_acquire))
            return;
        syntheticPending_.fetch_add(presses, std::memory_order_acq_rel);
        queue_.push_back(std::move(edit));
    }
    queueCv_.notify_one();
}

void HookService::discardQueuedEdits()
{
    editGeneration_.fetch_add(1, std::memory_order_acq_rel);
    {
        std::scoped_lock lock(queueMutex_);
        int canceledPresses = 0;
        for (const Edit& edit : queue_)
            canceledPresses += TextInjector::keyEventCount(edit.backspaces, edit.text) / 2 + (edit.virtualKey ? 1 : 0);
        queue_.clear();
        if (canceledPresses > 0)
            subtractPending(syntheticPending_, canceledPresses);
    }
}

void HookService::injectorLoop()
{
    for (;;) {
        Edit edit;
        bool more = false;
        {
            std::unique_lock lock(queueMutex_);
            queueCv_.wait(lock, [this] { return injectorQuit_.load() || !queue_.empty(); });
            if (injectorQuit_.load() && queue_.empty())
                return;
            edit = std::move(queue_.front());
            queue_.pop_front();
            more = !queue_.empty();
        }

        const int presses = TextInjector::keyEventCount(edit.backspaces, edit.text) / 2 + (edit.virtualKey ? 1 : 0);
        if (edit.generation != editGeneration_.load(std::memory_order_acquire)) {
            if (presses > 0)
                subtractPending(syntheticPending_, presses);
            continue;
        }
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
        const FocusTarget target = foregroundFocus();
        if (target.foreground != edit.foreground || target.focus != edit.focus
            || isCurrentProcessWindow(target.foreground)) {
            resetEngineState();
            if (presses > 0)
                subtractPending(syntheticPending_, presses);
            continue;
        }
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) || (GetAsyncKeyState(VK_MENU) & 0x8000)
            || (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000)) {
            // A delayed Backspace must not become Ctrl+Backspace when the user
            // begins a shortcut while replacement text is still queued.
            discardQueuedEdits();
            resetEngineState();
            subtractPending(syntheticPending_, presses);
            continue;
        }
#endif
        try {
#if defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
            if (edit.virtualKey) {
                INPUT inputs[2]{};
                for (auto& input : inputs) {
                    input.type = INPUT_KEYBOARD;
                    input.ki.wVk = edit.virtualKey;
                    input.ki.dwExtraInfo = kWindowsInputTag;
                }
                inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
                if (SendInput(2, inputs, sizeof(INPUT)) != 2)
                    throw std::runtime_error("Windows could not deliver a queued key. Typing assistance was paused.");
            } else
#endif
            if (injector_)
                injector_->applyEdit(edit.backspaces, edit.text);
        } catch (const std::exception& error) {
            enabled_.store(false);
            discardQueuedEdits();
            syntheticPending_.store(0);
            resetEngineState();
            publish(State::Failed, error.what());
            continue;
        }

        if (more)
            continue;  // a burst is still draining; the counter covers it

        // Safety valve. The counter is exact as long as every injected press
        // comes back through the hook, which it does on all three platforms —
        // but an event lost anywhere would otherwise leave the counter
        // permanently too high and silently eat one real keystroke after every
        // edit from then on. Once the queue is empty and the platform has had
        // time to deliver, any residue is drift, so clear it.
#if !defined(SCHNELLERTYPE_PLATFORM_WINDOWS)
        std::this_thread::sleep_for(kSelfEventGrace);
        syntheticPending_.store(0, std::memory_order_release);
#endif
    }
}

} // namespace st
