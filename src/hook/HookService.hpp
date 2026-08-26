// SPDX-License-Identifier: MIT
//
// The background half of the application.
//
// Threading model — three threads, no locks on the hot path except one short
// engine mutex:
//
//   GUI thread       Qt event loop. Owns AppController. Never blocks on the
//                    hook: every call below is either atomic or takes a mutex
//                    held for a few instructions.
//   hook thread      Runs libuiohook's hook_run(). Every keystroke is
//                    classified, fed to the rule engine and — when the platform
//                    allows it — suppressed, all inside the OS hook callback.
//                    On Windows that callback has a hard time budget, so it
//                    does no I/O and no allocation beyond the engine's own.
//   injector thread  Drains a FIFO of edits and performs the actual SendInput /
//                    CGEventPost / XTestFakeKeyEvent calls. Kept off the hook
//                    thread because injecting from inside a low-level Windows
//                    hook can deadlock against the same hook.
//
// Because the injector is a single consumer of a FIFO fed by a single producer,
// the order of edits is preserved, which is all the engines require.
//
// Ignoring our own keystrokes: before queueing an edit the service adds the
// number of key events it is about to generate to `syntheticPending_`. The hook
// callback decrements that counter and drops the event instead of processing
// it. No timers, no heuristics.

#pragma once

#include "core/LanguageRuleEngine.hpp"
#include "hook/KeyMapper.hpp"
#include "hook/PlatformSupport.hpp"
#include "hook/TextInjector.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace st {

class HookService {
public:
    enum class State { Stopped, Starting, Running, Failed };

    struct Status {
        State                     state = State::Stopped;
        std::string               message;
        std::string               injectorBackend;
        bool                      canSuppress = false;
        platform::PermissionInfo  permission;
    };

    /// Called from the hook thread whenever the status changes. The Qt layer
    /// must marshal it onto the GUI thread (AppController does).
    using StatusCallback = std::function<void(Status)>;

    HookService();
    ~HookService();

    HookService(const HookService&)            = delete;
    HookService& operator=(const HookService&) = delete;

    /// Starts the hook thread. Returns false when permissions are missing; the
    /// status callback carries the reason.
    bool start();
    void stop();

    [[nodiscard]] bool   isRunning() const noexcept { return running_.load(); }
    [[nodiscard]] Status status() const;

    /// Master switch. When disabled the hook stays installed (so re-enabling is
    /// instant and needs no permission round-trip) but every event is passed
    /// straight through.
    void               setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const noexcept { return enabled_.load(); }

    /// Swap the active rule engine. Safe to call at any time; the previous
    /// engine's word state is dropped.
    void setEngine(EnginePtr engine);

    /// The engine's word state is only valid while the caret stays put. Called
    /// internally on mouse clicks, and by the Qt layer on focus changes.
    void resetEngineState();

    void setStatusCallback(StatusCallback callback);

    /// Entry point for libuiohook's C dispatch callback. Not part of the public
    /// API in spirit — it is public only so the C shim can reach it.
    static void dispatchTrampoline(void* uiohookEvent);

    /// Access to the key mapper for layout overrides.
    KeyMapper&       keyMapper() noexcept { return mapper_; }
    const KeyMapper& keyMapper() const noexcept { return mapper_; }

private:
    struct Edit {
        int            backspaces = 0;
        std::u32string text;
    };

    void handleEvent(void* event);
    void handleKeyPressed(void* event);

    void enqueue(Edit edit);
    void injectorLoop();
    void hookLoop();
    void publish(State state, std::string message);

    KeyMapper                     mapper_;
    std::unique_ptr<TextInjector> injector_;

    mutable std::mutex engineMutex_;
    EnginePtr          engine_;

    std::atomic_bool enabled_{false};
    std::atomic_bool running_{false};
    std::atomic_bool stopRequested_{false};

    // --- "is this event one of mine?" ---------------------------------------
    //
    // `syntheticPending_` is the exact number of key presses the queued edits
    // will generate, decremented as they come back through the hook.
    //
    // It is exact — the injectors generate exactly the events the count
    // predicts — but "exact" is a property of code that can regress, and a
    // counter left one too high would silently eat one real keystroke after
    // every edit from then on. So the injector thread also clears it once the
    // queue has drained and the platform has had time to deliver, which turns
    // any drift into at most one lost keystroke instead of a permanent fault.
    //
    // Note what is deliberately *not* done here: blanket-ignoring every event
    // while an injection is in flight. That looks safer and is much worse —
    // during Vietnamese typing nearly every keystroke produces an edit, so the
    // ignore window would swallow a large fraction of real input and the
    // engine's idea of the screen would drift apart from the screen itself.
    std::atomic_int syntheticPending_{0};

    std::thread      hookThread_;
    std::thread      injectorThread_;
    std::atomic_bool injectorQuit_{false};

    std::mutex              queueMutex_;
    std::condition_variable queueCv_;
    std::deque<Edit>        queue_;

    mutable std::mutex statusMutex_;
    Status             status_;
    StatusCallback     statusCallback_;
};

} // namespace st
