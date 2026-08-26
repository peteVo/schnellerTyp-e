# schnellerTyp-e

A cross-platform input method editor and text transformation tool, in the spirit
of UniKey. It runs in the system tray, watches the keyboard globally, and
rewrites what you typed in whichever application has focus.

- **German** — `ae → ä`, `oe → ö`, `ue → ü`, `ss → ß`, with a dictionary that
  knows `Dauer`, `Duell`, `Wasser` and `aussehen` are not umlauts.
- **Vietnamese** — Telex and VNI, with free tone marking, correct tone
  placement, spelling check and auto-restore.
- **Anything else** — a new language or a set of text expansions is a JSON file
  in the config folder, or one `registerEngine()` call in C++.
- **One-chord switching** — tap **Ctrl + Shift** and let go to step German →
  Vietnamese → … → Off → German, anywhere, in any application.

Windows, macOS and Linux/X11. Modern C++20, CMake, Qt 6 with a QML dark-mode
settings window, libuiohook for capture and a native backend per platform for
injection.

**[BUILD.md](BUILD.md) has the build instructions, the per-OS permission notes
and the user guide.** [RELEASE.md](RELEASE.md) covers producing a portable
Windows zip to hand to someone who does not build software. Windows users following the Qt Creator route can use
**[WINDOWS-BUILD.md](WINDOWS-BUILD.md)**, which is the same thing condensed to
one path.

---

## Layout

```
src/core/     the rule engines. No Qt, no OS headers, no I/O.
src/hook/     libuiohook capture, key mapping, per-platform injection.
src/app/      the Qt bridge: AppController, QSettings, QSystemTrayIcon.
qml/          the settings window and its components.
tests/        engine tests — no display server required.
packaging/    Info.plist, entitlements, .desktop, PKGBUILD.
config/       annotated examples of the user-editable rule files.
```

The split at `src/core` is the load-bearing one. Every rule the program applies
lives behind one small interface:

```cpp
class LanguageRuleEngine {
    virtual void         reset() noexcept = 0;
    virtual EngineResult processKey(const KeyEvent&) = 0;
};

struct EngineResult {
    bool           handled;      // swallow the original keystroke
    int            backspaces;   // delete this many characters
    std::u32string text;         // then type this
};
```

"Delete N, then type T" is the only primitive. Composing a Vietnamese syllable,
reverting a German digraph, expanding a macro and restoring a mistyped English
word are all expressed as that.

Both engines work the same way: keep the plain characters the user actually
typed, re-derive the whole word after every keystroke, and emit the difference
against what is already on screen. That is what makes free tone marking, tone
migration (`toa` → `toán` moves the mark) and self-correcting German exceptions
fall out without special cases — and it is why the engines are pure functions of
the keystroke sequence, with no timers to lose a race against.

## Threads

| Thread | Does |
|---|---|
| GUI | Qt event loop, `AppController`. Never blocks on the hook. |
| hook | `hook_run()`. Classifies each key, runs the engine, suppresses where the OS allows. |
| injector | Drains a FIFO of edits and performs the actual `SendInput` / `CGEventPost` / `XTestFakeKeyEvent`. |

Injection is off the hook thread on purpose: injecting from inside a Windows
low-level hook callback can deadlock against that same hook. Events the program
generates itself are recognised by an exact counter of pending synthetic key
presses, charged before the events exist.

## Platform differences that shape the design

| | Windows | macOS | Linux/X11 |
|---|---|---|---|
| Capture | `WH_KEYBOARD_LL` | `CGEventTap` | `XRecord` |
| Suppress the original key | yes | yes | **no** |
| Injection | `SendInput` + `KEYEVENTF_UNICODE` | `CGEventKeyboardSetUnicodeString` | `XTest` + borrowed keycode |
| Permission | none | Accessibility | none (X11 session required) |

X11's inability to suppress is the one difference visible in behaviour: the
keystroke has already reached the application, so every edit sends one extra
backspace to erase it, and the edit races the next keystroke. Measured, that
holds up to about 200 words per minute. BUILD.md explains it in full, along with
why Wayland is not supported.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

About a hundred keystroke sequences through the German, Vietnamese and
JSON-driven engines, checking the text that would end up on screen rather than
the struct the engine returned. The hook and the injectors are covered by
running the real thing against a real X server and reading back what a text
field received; `SCHNELLERTYPE_DEBUG=1` traces every keystroke and every edit.

## Relationship to UniKey

UniKey 3.62's source was used as a **behavioural reference only** — what a
Vietnamese typist expects `nuwowcs`, `toasn` or `ass` to do. No code and no
tables from it are present here: the composition tables are generated from
Unicode normalisation, the syllable model and the tone-placement rules were
written from Vietnamese orthography, and the engine is a different design
(decompose-and-re-render rather than UniKey's packed bit-table state machine).

UniKey is GPL-2.0 and copyright Phạm Kim Long; nothing in this repository is
derived from it. `NOTICE.md` says the same in the form a lawyer would want.

## Status

The engines, the hook, all three injection backends, the tray and the settings
window are implemented and the German and Vietnamese paths are verified end to
end on X11. The Windows and macOS backends are written against their documented
APIs but have not been run on those platforms — that is the first thing to do
with this repository, and BUILD.md tells you what to expect when you do.
