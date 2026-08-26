// SPDX-License-Identifier: MIT
#include "hook/HookService.hpp"

#include "core/Unicode.hpp"

#include <uiohook.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace st {
namespace {

/// libuiohook's dispatch callback is a C function pointer, and in 1.2 it does
/// not carry a user pointer at all. Exactly one service owns the hook at a
/// time, so a file-static pointer is the simplest correct bridge.
std::atomic<HookService*> gActiveService{nullptr};

/// SCHNELLERTYPE_DEBUG=1 traces every keystroke and every edit to stderr. The
/// hook is the one part of the program that cannot be unit-tested, so it keeps
/// a way to see what it is doing on a real desktop.
const bool gTrace = std::getenv("SCHNELLERTYPE_DEBUG") != nullptr;

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
        // what is on screen.
        resetEngineState();
        return;

    case EVENT_KEY_PRESSED:
        handleKeyPressed(rawEvent);
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
