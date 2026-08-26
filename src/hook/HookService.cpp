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
    resetEngineState();
}

void HookService::setEngine(EnginePtr engine)
{
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
        resetEngineState();
        return;

    case EVENT_KEY_PRESSED:
        handleKeyPressed(rawEvent);
        return;

    case EVENT_KEY_RELEASED:
        // Releases are otherwise of no interest: the engines are driven by
        // presses, and the injectors never synthesise a modifier, so nothing
        // here can be an event of our own.
        trackChordRelease(static_cast<std::uint16_t>(event->data.keyboard.keycode),
                          static_cast<std::uint16_t>(event->mask));
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
    if (syntheticPending_.load(std::memory_order_acquire) > 0) {
        syntheticPending_.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

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

    if (!result.handled)
        return;

    if (platform::canSuppressEvents()) {
        // Windows / macOS: consume the keystroke, then perform the edit. Note
        // that a "swallow" result (an escape key) has no edit at all, which is
        // exactly what the flag below achieves on its own.
        event->reserved |= 0x01;
        if (result.isNoop())
            return;
        enqueue(Edit{result.backspaces, std::move(result.text)});
    } else {
        // X11: XRecord observes, it cannot consume, so the character the user
        // typed has already reached the application. Erase it along with
        // whatever else this edit replaces.
        if (result.isNoop())
            enqueue(Edit{1, {}});  // swallow-equivalent: rub out the stray char
        else
            enqueue(Edit{result.backspaces + 1, std::move(result.text)});
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

void HookService::enqueue(Edit edit)
{
    // Charge the counter before the events exist, so the hook can never see one
    // of them while the counter is still zero. Only presses are counted: the
    // hook only inspects EVENT_KEY_PRESSED.
    const int presses = TextInjector::keyEventCount(edit.backspaces, edit.text) / 2;
    if (gTrace)
        std::fprintf(stderr, "[st] edit bs=%d text=%s presses=%d\n", edit.backspaces,
                     unicode::toUtf8(edit.text).c_str(), presses);
    syntheticPending_.fetch_add(presses, std::memory_order_acq_rel);
    {
        std::scoped_lock lock(queueMutex_);
        queue_.push_back(std::move(edit));
    }
    queueCv_.notify_one();
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

        if (injector_)
            injector_->applyEdit(edit.backspaces, edit.text);

        if (more)
            continue;  // a burst is still draining; the counter covers it

        // Safety valve. The counter is exact as long as every injected press
        // comes back through the hook, which it does on all three platforms —
        // but an event lost anywhere would otherwise leave the counter
        // permanently too high and silently eat one real keystroke after every
        // edit from then on. Once the queue is empty and the platform has had
        // time to deliver, any residue is drift, so clear it.
        std::this_thread::sleep_for(kSelfEventGrace);
        syntheticPending_.store(0, std::memory_order_release);
    }
}

} // namespace st
