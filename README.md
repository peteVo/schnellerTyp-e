# schnellerTyp-e

A Windows-first typing assistant for mixing English, Vietnamese and German
without switching language modes between words. C++20, Qt 6 and QML.

## Automatic typing

Select **Automatic** once. Type Vietnamese using Telex or VNI and German using
ae, oe, ue and ss. The current word can adjust as you type; Space settles it.
For example, with Telex:

    Keys:   hoom nay tooi ddeens New York
    Result: hôm nay tôi đến New York

    Keys:   tooi ddeens Berlin und moechte coffee
    Result: tôi đến Berlin und möchte coffee

In Vietnamese Telex mode, `w` makes `ư`, `ww` types a literal `w`, and `uww`
types `uw`. Automatic also resolves delayed Telex input `nuawx` and `nuwax`
to `nữa`; literal undo remains literal after Space.

Automatic compares literal input with Vietnamese and German candidates using
80,403 offline word-frequency entries and a small amount of language context.
Known English words, unknown names and ambiguous words favour literal input.
This is a conservative statistical heuristic; identical keys can express
different intentions, so it cannot promise perfect automatic interpretation.

- Press **Escape before Space** to restore the original keys of the current word.
- Add names or technical terms under **Preferences → Keep these words literal**.
- Addresses, paths, identifiers and contractions are kept literal once their
  identifying punctuation is typed.
- German and Vietnamese manual modes and custom JSON profiles remain available.
- New installations default to Automatic; existing saved mode choices are kept.

Everything needed for typing runs locally. No account, network service or
typing-history collection is required. Optional debug tracing can include
keystrokes; leave SCHNELLERTYPE_DEBUG unset for normal use.

## Desktop interface

**Typing** contains the mode selector and a live practice field using the real
configured engine. **Preferences** contains Telex/VNI, German rules, personal
literal words and shortcuts. **Setup** contains availability, permissions and
custom-rule recovery. The interface follows the system light/dark appearance
and supports keyboard navigation.

The app stays in the tray when its settings window closes. Left-click the tray
to pause/resume, double-click for settings, and right-click for its menu.
Ctrl+Shift cycles Automatic → German → Vietnamese → custom profiles → Off.
Preferences can change or disable that shortcut. Select Automatic to mix all
three languages within one mode.

## Windows

Use the portable Windows x64 zip: unzip the whole folder, then run
schnellerTyp-e.exe. A local build is unsigned. Higher-privilege windows and
applications that reject simulated input require separate compatibility testing.
The settings/practice window intentionally accepts literal keys; the global
hook does not transform its own process.

[BUILD.md](BUILD.md) describes the full build prerequisites. For a matching
MinGW Qt/toolchain build, tools/package-windows-mingw.ps1 assembles a portable
zip and its dependencies. tools/package-windows.ps1 is the MSVC packaging path.

## Linux and macOS

Optional native adapters use Fcitx 5 on Linux and InputMethodKit on macOS. They
share the language engine and saved built-in preferences with the desktop app.
See [PLATFORMS.md](PLATFORMS.md) for build flags, installation and verification
limits. The legacy Linux/X11 and macOS event-injection backends remain available.
The X11 backend is not a native Wayland solution. Avoid running a native input
method and the legacy hook against the same typing session.

## Build and tests

The engine tests require only a C++20 compiler and CMake:

    cmake -S . -B build-core -DSCHNELLERTYPE_BUILD_APP=OFF
    cmake --build build-core --config Release --parallel
    ctest --test-dir build-core -C Release --output-on-failure

The full desktop app additionally requires Qt 6.4+ and libuiohook. Tests simulate
the final visible text, including mixed languages, editing, reset boundaries,
literal words and native composition. The optional Windows scratch-window probe
checks the real hook/injection path without editing user documents.

    src/core/       platform-independent rules, Automatic and composition session
    src/hook/       keyboard capture and legacy native injectors
    src/app/        shared preferences, Qt bridge and tray
    src/platform/   optional Fcitx 5 and InputMethodKit hosts
    qml/            settings and practice interface
    tests/          core, native-preference and opt-in Windows integration checks
    config/         custom rules and exception examples

## Data and attribution

Application code is MIT. The filtered word-frequency data is CC BY-SA 4.0,
derived from wordfreq 3.1.1 by Robyn Speer. Its full attribution and license are
in src/core/auto/data/; tools/generate-frequency-data.py reproduces the
checked-in tables. Python/wordfreq is development tooling, not an app dependency.
German sharp-S candidate data is derived from
[enz/german-wordlist](https://github.com/enz/german-wordlist) and licensed under
CC0 1.0; source details and scope are in
[NOTICE-german-spelling.md](src/core/auto/data/NOTICE-german-spelling.md).
See [NOTICE.md](NOTICE.md) for all attributions and the relationship to UniKey.
