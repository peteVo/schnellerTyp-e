# Native input methods

Windows is the primary release. Its tray application uses tagged Windows input
events and checks the target window/input field before applying queued edits.
Automatic uses offline English, Vietnamese and German word frequencies and one
word of language context. It is a conservative statistical heuristic, not a
guarantee of detecting the intention behind every ambiguous key sequence.

The optional native adapters reuse the same engine through `CompositionSession`:
edits affect an owned preedit buffer. In Automatic mode, plain Left/Right/Home/End
move within that buffer and Backspace/Delete edit at the caret. Space and
punctuation commit it; Enter, Tab, broader navigation and ordinary command
shortcuts commit pending text before being forwarded. Ctrl+Alt+V temporarily
forces Vietnamese for the current word; Ctrl+Alt+Z undoes a current-word edit.
Native hosts
reload saved built-in mode and language preferences when activated. A saved
custom JSON mode passes through literally in the native adapters.
Disable the desktop keyboard hook when using a native input source so there
is only one processor of your keystrokes.

## Shared behavior and Windows adaptation

Language selection, word overrides, bounded current-word undo and native caret
editing remain C++20 code under `src/core/`, with no Qt or OS dependencies.
The core test suite runs on MSVC, Linux and macOS. Adapters translate keys and
modifiers; Right Alt/AltGr is excluded from composition shortcuts.

The Windows hook uses the shared selection/override/undo behavior through its
existing end-of-word edit contract. Navigation still ends its tracked word.
Native caret editing is exposed by `CompositionSession::Result::cursor` in
Unicode codepoints. A future Windows TSF adapter can use this same session and
translate the cursor to UTF-16, as the macOS adapter does. It should own a native
composition buffer rather than backspace across application-owned text.
Windows interactive hook tests remain opt-in; compilation and shared-core tests
do not establish compatibility with every Windows application.

## Omarchy / Linux (Fcitx 5)

Prerequisites: a C++20 compiler, CMake 3.21+, Fcitx 5 development files and Qt 6.4+
Core, Widgets and DBus. The addon itself links Qt Core; Status & Setup uses
Widgets and DBus. The native package does not need libuiohook, XTest or Qt Quick.
The optional
legacy settings panel uses Qt Quick and can be launched with `--no-hook`.
The addon also exposes built-in preferences through Fcitx's standard
configuration dialog, requiring no legacy app. The
[native Arch package guide](packaging/linux/fcitx5/README.md) is the preferred
installation/upgrade path on Omarchy.

```sh
cmake -S . -B build-native -DSCHNELLERTYPE_BUILD_APP=OFF \
  -DSCHNELLERTYPE_BUILD_FCITX5=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
sudo cmake --install build-native
```

If Fcitx is already running, restart it through the desktop's existing service.
If it is not running, start `fcitx5 -d` once for the current desktop session.
Open `fcitx5-configtool`, disable the
"Only show current language" filter if needed, and add **schnellerTyp-e**. Select
that input method once; Automatic interprets words within that one source. On
Omarchy reuse its existing Fcitx service/autostart when present.
The addon uses per-field composition and bypasses password/sensitive fields
when the client reports those capabilities. Toolkit support must be checked in
your actual browser/editor applications on Wayland and XWayland.

The native build includes `fcitx5-composition`: an in-memory client with an
isolated settings directory and all unrelated desktop addons disabled. It loads
the built shared addon through Fcitx's actual loader and exercises its callbacks,
including UTF-8 cursor placement, one-time focus-out commits, editing,
shortcuts, field isolation, sensitive-field
transitions, and saved settings. It does not establish Wayland protocol or
browser/editor compatibility. See [VERIFICATION.md](VERIFICATION.md) for the
local desktop results and compatibility limits.

Use the existing X11 tray app as a separate fallback; its erase-and-retype
behavior is not native Wayland support. IBus integration is not included.

## macOS (InputMethodKit)

Build on a Mac with Xcode command-line tools, CMake and Qt 6 Core:

```sh
cmake -S . -B build-native -DSCHNELLERTYPE_BUILD_APP=OFF \
  -DSCHNELLERTYPE_BUILD_MACOS_INPUT_METHOD=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
```

The output is `schnellerTyp-e-input-method.app`. Follow
[the local installation guide](packaging/macos/input-method/README.md) to add it
to Input Sources. Distributing it to another Mac also requires bundling Qt Core
(for example with `macdeployqt`), Developer ID signing and notarization.
This is an experimental native host until compiled and exercised on a Mac;
its secure-input bypass is not a guarantee of universal password-field support.

## Verification

The workflow in `.github/workflows/build.yml` compiles core tests on all three
operating systems and builds the optional Linux/macOS input methods. Merely
adding this workflow does not mean a hosted run has passed. Native runtime
validation still requires each operating system and representative apps.

For a dependency-free core check:

```sh
cmake -S . -B build-core -DSCHNELLERTYPE_BUILD_APP=OFF -DSCHNELLERTYPE_WERROR=ON
cmake --build build-core --config Release --parallel
ctest --test-dir build-core -C Release --output-on-failure
```

On Windows, `SCHNELLERTYPE_BUILD_INTERACTIVE_TESTS=ON` additionally builds
`schnellertype_hook_probe`. Run it explicitly on an unlocked desktop with Caps
Lock off. It creates its own scratch editor and a separate hook process, then
checks real keyboard capture, event suppression and replacement delivery. It
never opens or edits user documents; it aborts if its scratch window loses focus.
