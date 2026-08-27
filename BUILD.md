# Building and using schnellerTyp-e

A cross-platform input method editor for German umlauts and Vietnamese
(Telex / VNI). It sits in the system tray, watches the keyboard globally, and
rewrites what you typed in whatever application has focus.

- [What you need everywhere](#what-you-need-everywhere)
- [Windows](#windows)
- [macOS](#macos)
- [Linux](#linux)
  - [Arch Linux and CachyOS](#arch-linux-and-cachyos)
  - [Debian and Ubuntu](#debian-and-ubuntu)
  - [Fedora](#fedora)
  - [Wayland](#wayland)
- [Using it](#using-it)
- [Extending it without a compiler](#extending-it-without-a-compiler)
- [Troubleshooting](#troubleshooting)

---

## What you need everywhere

| Dependency | Minimum | Notes |
|---|---|---|
| A C++20 compiler | MSVC 19.30 / Clang 14 / GCC 11 | |
| CMake | 3.21 | Ninja recommended |
| Qt | 6.4 | `Core Gui Qml Quick QuickControls2 Widgets` |
| libuiohook | 1.2 | Either the 1.2 or the 1.3 dispatch API; CMake detects which. **Not available in vcpkg — build from source.** |

The generic build, once the dependencies are in place, is the same on all three
platforms:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure     # runs the engine tests
```

`ctest` exercises the German and Vietnamese engines with about a hundred
keystroke sequences. It needs no display server and no keyboard hook, because
the rule engines are deliberately free of Qt and of OS calls — which makes them
the part of the program you can actually test.

Useful options:

| Option | Default | Meaning |
|---|---|---|
| `-DSCHNELLERTYPE_BUILD_TESTS=OFF` | `ON` | Skip the test target |
| `-DSCHNELLERTYPE_WERROR=ON` | `OFF` | Warnings become errors |
| `-DUIOHOOK_ROOT=<prefix>` | — | Where to find libuiohook, if it is not on the default search path |

---

## Windows

### 1. Prerequisites

Install **Visual Studio 2022 or newer** with the "Desktop development with C++"
workload, and Qt 6 from the official online installer
(<https://www.qt.io/download-qt-installer>) — pick **Qt 6.5 or newer → MSVC
64-bit** plus the *Qt Quick* modules.

**libuiohook is not in vcpkg.** There is no `uiohook` port, so build it from
source once. It needs nothing but the compiler on Windows (it links only
`Advapi32`):

```powershell
git clone https://github.com/kwhat/libuiohook.git C:\src\libuiohook
cmake -S C:\src\libuiohook -B C:\src\libuiohook\build-x64 -A x64 ^
      -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=C:\libs\uiohook
cmake --build C:\src\libuiohook\build-x64 --config Release
cmake --install C:\src\libuiohook\build-x64 --config Release
```

That leaves `C:\libs\uiohook\include\uiohook.h`, `lib\uiohook.lib` and
`bin\uiohook.dll`.

**Pass `-A x64` explicitly.** Otherwise the architecture depends on which
developer prompt is open and which generator CMake chose, and a 32-bit
libuiohook against a 64-bit app fails as four unresolved `hook_*` symbols with
one easily-missed `LNK4272` warning above them. Verify with
`dumpbin /headers C:\libs\uiohook\lib\uiohook.lib | findstr machine`; a
build tree cannot change platform in place, so use a fresh directory if you
need to redo it.

### 2. Configure and build

```powershell
cmake -B build -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=C:/Qt/6.8.2/msvc2022_64 ^
      -DUIOHOOK_ROOT=C:/libs/uiohook
cmake --build build --parallel
```

Run this from a **Developer PowerShell for VS** so the MSVC toolchain is on the
path. Ninja avoids pinning a Visual Studio generator version; if you prefer the
VS generator, note that `Visual Studio 18 2026` needs CMake 4.2 or newer.

The build copies `uiohook.dll` next to the executable automatically.

### 3. Make it runnable outside the build tree

```powershell
C:\Qt\6.8.2\msvc2022_64\bin\windeployqt.exe --qmldir qml build\schnellerTyp-e.exe
```

`build\schnellerTyp-e.exe` is now self-contained.

### 4. Permissions and platform notes

- **No permission prompt.** The low-level keyboard hook (`SetWindowsHookEx`
  with `WH_KEYBOARD_LL`) and `SendInput` both work for a normal user account.
- **Elevated windows are off limits.** User Interface Privilege Isolation stops
  a normal-integrity process from sending input to an elevated one. If you want
  schnellerTyp-e to work inside an elevated Command Prompt or an installer,
  run schnellerTyp-e elevated too. The status pane says so as well.
- **Games and remote desktop clients** that read raw input (`WM_INPUT`,
  DirectInput) ignore synthesised keystrokes by design. Nothing can be done
  about that from user space.
- **Toolset mixing is fine.** MSVC v140 through v145 (VS 2015 → VS 2026) are
  binary compatible, so a Qt built for `msvc2022_64` links happily against a
  VS 2026 build — the linker just has to be the newer of the two, which it is.
- **Antivirus.** A program that installs a global keyboard hook looks exactly
  like a keylogger to heuristic scanners. Expect to whitelist the binary, and
  sign it if you distribute it.
- **Start with Windows:** put a shortcut in
  `shell:startup` (`Win+R` → `shell:startup`).

---

## macOS

### 1. Prerequisites

```bash
brew install cmake ninja qt
```

**libuiohook is not in Homebrew** — there is no `brew install libuiohook`, so it
has to be built. It takes about ten seconds:

```bash
git clone --depth 1 https://github.com/kwhat/libuiohook.git ~/src/libuiohook
cmake -S ~/src/libuiohook -B ~/src/libuiohook/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build ~/src/libuiohook/build --parallel
cmake --install ~/src/libuiohook/build
```

Installing into `$HOME/.local` rather than `/usr/local` keeps it out of
Homebrew's way and needs no `sudo`. Any prefix works; pass whichever you used as
`-DUIOHOOK_ROOT` below.

### 2. Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" \
      -DUIOHOOK_ROOT="$HOME/.local"
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

`$(brew --prefix qt)` resolves to `/opt/homebrew/opt/qt` on Apple Silicon and
`/usr/local/opt/qt` on Intel, so the same line works on both. The Homebrew
formula is `qt` (currently Qt 6); older instructions elsewhere say `qt@6`, which
is at best an alias and at worst a package that no longer resolves.

The build produces `build/schnellerTyp-e.app` and ad-hoc signs it, because
macOS grants Accessibility to a *signed identity*, not to a path.

### 3. Grant Accessibility

The first launch pops the system dialog. If you miss it, or the button in the
app does nothing:

> System Settings → Privacy & Security → **Accessibility** → enable
> **schnellerTyp-e**

Then quit and relaunch the app — the entitlement is read at process start.

**The rebuild trap:** an ad-hoc signature (`codesign -s -`) produces a *new*
identity every time you rebuild, so macOS treats the rebuilt app as a different
program and the Accessibility switch silently stops applying. Symptoms: the
switch is on, the app says access is denied. The fix is to remove the entry with
the "−" button and add the app again. For anything beyond local development,
sign with a real Developer ID:

```bash
codesign --force --deep --options runtime \
  --entitlements packaging/macos/schnellerTyp-e.entitlements \
  --sign "Developer ID Application: Your Name (TEAMID)" build/schnellerTyp-e.app
xcrun notarytool submit ... && xcrun stapler staple build/schnellerTyp-e.app
```

### 4. Platform notes

- **No Dock icon.** `LSUIElement` is set in `Info.plist`, so the app lives in
  the menu bar only. Remove that key if you would rather have a Dock tile.
- **Not sandboxable.** A global keyboard hook is incompatible with the App
  Sandbox, so schnellerTyp-e cannot ship through the Mac App Store. Direct
  distribution with Developer ID + notarisation is the supported route.
- **Secure input.** While a password field has focus, macOS enables Secure Event
  Input and no event tap sees the keyboard — including this one. That is correct
  and desirable. Some apps (Terminal with "Secure Keyboard Entry", some password
  managers) leave it on longer than you expect; if the app seems dead
  everywhere, that is usually why.
- **Start at login:** System Settings → General → Login Items → add the app.

---

## Linux

schnellerTyp-e needs an **X11 session**. See [Wayland](#wayland) below.

### Arch Linux and CachyOS

libuiohook is in the AUR, everything else is in the official repositories.

```bash
sudo pacman -S --needed base-devel cmake ninja git \
                        qt6-base qt6-declarative \
                        libx11 libxtst libxi libxkbcommon libxkbcommon-x11

# libuiohook comes from the AUR:
paru -S libuiohook          # or: yay -S libuiohook
```

If that package has gone missing or is broken — it is user-submitted, so it can
— build the library from upstream instead. This route depends on nobody and
works on every distribution:

```bash
git clone --depth 1 https://github.com/kwhat/libuiohook.git ~/src/libuiohook
cmake -S ~/src/libuiohook -B ~/src/libuiohook/build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build ~/src/libuiohook/build --parallel
cmake --install ~/src/libuiohook/build
```

Then build directly:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
# ...or, if you built libuiohook into your own prefix above:
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DUIOHOOK_ROOT="$HOME/.local"

cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/schnellerTyp-e
```

`-DUIOHOOK_ROOT` is searched before pkg-config and before any system copy, so a
prefix build wins over a stale `/usr/lib` one rather than silently losing to it.
The configure output names the library it settled on — check that line if a link
error mentions `hook_run`.

or build a package, which also installs the desktop entry and an autostart file:

```bash
cd packaging/linux
makepkg -si
```

**CachyOS specifics.** CachyOS is Arch underneath, so the commands above are
unchanged. Two things are worth knowing:

- CachyOS ships several desktop flavours. The KDE Plasma and Xfce X11 sessions
  work as documented. If you installed the **Wayland** session (the default for
  the Plasma and GNOME editions), pick the *X11* session at the login screen —
  the session type is shown in the app's status pane, so you can check without
  guessing.
- If you use one of the CachyOS optimised repositories, `qt6-base` may come from
  `cachyos-v3` or `cachyos-v4`. That is fine; the CMake configuration does not
  care which repository Qt came from.

### Debian and Ubuntu

```bash
sudo apt install build-essential cmake ninja-build git \
                 qt6-base-dev qt6-declarative-dev \
                 qml6-module-qtquick qml6-module-qtquick-controls \
                 qml6-module-qtquick-layouts qml6-module-qtquick-templates \
                 qml6-module-qtqml-workerscript \
                 libx11-dev libxtst-dev libxi-dev \
                 libxkbcommon-dev libxkbcommon-x11-dev libx11-xcb-dev \
                 libxkbfile-dev libxt-dev
```

libuiohook is not packaged, so build it once:

```bash
git clone https://github.com/kwhat/libuiohook
cmake -S libuiohook -B libuiohook/build -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build libuiohook/build --parallel
sudo cmake --install libuiohook/build && sudo ldconfig
```

Then the generic build. Ubuntu 24.04 ships Qt 6.4, which is exactly why the
project's minimum is 6.4 rather than 6.5.

### Fedora

```bash
sudo dnf install gcc-c++ cmake ninja-build git \
                 qt6-qtbase-devel qt6-qtdeclarative-devel \
                 libX11-devel libXtst-devel libXi-devel \
                 libxkbcommon-devel libxkbcommon-x11-devel libxkbfile-devel
```

libuiohook is not in the Fedora repositories either; build it as shown above.

### Autostart

```bash
mkdir -p ~/.config/autostart
cp packaging/linux/schnellerTyp-e.desktop ~/.config/autostart/
```

### Wayland

Wayland has no global keyboard hook and no XTest equivalent. A compositor
deliberately does not let one client read or synthesise another client's input,
which is a security improvement and a hard wall for this design. schnellerTyp-e
detects the session type, says so in the status pane, and refuses to pretend it
is working.

The options are: log into an X11 session, or use a Wayland-native input method
(ibus-bamboo and fcitx5-unikey both do Vietnamese well). A Wayland port of
schnellerTyp-e would mean implementing the `input-method-unstable-v2` protocol,
which is a different program rather than a different backend.

### The X11 limitation worth knowing about

On Windows and macOS the hook **suppresses** the keystroke: the application
never sees the `e` of `ue`, it only ever receives `ü`.

X11 cannot do that. `XRecord` is an observer; by the time schnellerTyp-e is told
about a key, the focused window already has it. So on X11 the app lets the key
through and then erases it: every edit sends one extra backspace. The visible
result is identical, but the edit is now *racing* the next keystroke.

Measured on this codebase, the X11 path is reliable up to about one keystroke
every 60 ms — roughly 200 words per minute, well above sustained human typing —
and starts dropping characters at around 50 ms. Windows and macOS have no such
ceiling because nothing is racing. If you type Vietnamese at 240 WPM, use one of
those two, or an X11 session with a native IME.

---

## Using it

**The tray icon** shows the active profile: `DE`, `VN`, `FR`… or `OFF`.

| Action | Result |
|---|---|
| Left click | Pause / resume |
| Middle click | Next language |
| Ctrl + Shift | Next language, or Off (see below) |
| Double click | Open settings |
| Right click | Menu: settings, enable, language, reload rules, config folder, quit |

Closing the settings window does not quit the app; use the tray menu or the
Quit button.

### The keyboard shortcut

**Tap Ctrl and Shift together and let go** — without pressing anything else —
to step through the profiles and Off:

```
German  →  Vietnamese  →  French accents  →  Off  →  German  →  …
```

Everything you have loaded is in the rotation, so a custom JSON rule set joins
it with no extra configuration. Coming back from Off always lands on the first
profile.

It fires on **release**, and any other key pressed while the modifiers are down
cancels it. That is what lets it coexist with the shortcuts you already use:
Ctrl+Shift+T reopens your browser tab exactly as before, because the `T`
stands the chord down on its way through. Left and right modifiers are
interchangeable.

Settings → Language offers **Ctrl + Shift**, **Ctrl + Alt**, **Alt + Shift** or
**No shortcut**. On macOS these read Control, Option and Shift. Pick a different
one if something else on your system already claims a bare modifier tap — some
Windows installations use Ctrl+Shift to switch keyboard layouts, under
*Settings → Time & language → Typing → Advanced keyboard settings → Input
language hot keys*.

If the chord seems dead, the log says whether it fired at all: look for
`shortcut: cycled to …` in `schnellerTyp-e.log` (the settings window shows the
folder). A line there with nothing happening on screen is a different problem
from no line at all.

### German

| Type | Get |
|---|---|
| `ae` `oe` `ue` | `ä` `ö` `ü` (`Ae` → `Ä`, `AE` → `Ä`) |
| `ss` | `ß` |

The interesting part is the words where those letter pairs are *not* an umlaut.
Three mechanisms, all optional, all independent of typing speed:

1. **Exception dictionary.** `dauer`, `duell`, `wasser`, `essen`, `aussehen`
   and friends stay literal. It is not a fixed decision: the transformation is
   held back only while what you have typed still matches a dictionary entry, so
   `duell` stays `duell` while `duester` becomes `düster`, and `aussehen` stays
   intact while `aussen` becomes `außen`.
2. **Double-key undo.** `ue` gives `ü`; press `e` once more and you get `ue`
   back, pinned. So `Dauer` can be typed `d a u e e r` even with the dictionary
   off. This is UniKey's convention, generalised.
3. **Escape key** (`\` by default). The next keystroke is literal and the
   backslash is never typed: `mu\esli` → `muesli`.

`ss → ß` is on by default because you asked for it, but it is the rule that
misfires most often in modern orthography (`Wasser`, `dass`, `Fluss` all keep
`ss`). It has its own switch in the settings window.

### Vietnamese

**Telex**

| Type | Get |
|---|---|
| `aa` `ee` `oo` | `â` `ê` `ô` |
| `aw` `ow` `uw` | `ă` `ơ` `ư` (`w` alone → `ư`) |
| `dd` | `đ` |
| `s f r x j` | sắc, huyền, hỏi, ngã, nặng |
| `z` | remove the tone |

**VNI**

| Type | Get |
|---|---|
| `6` `7` `8` `9` | circumflex, horn, breve, `đ` |
| `1 2 3 4 5` | the five tones |
| `0` | remove the tone |

Both methods support:

- **free tone marking** — `toans` and `toasn` both give `toán`;
- **correct tone placement**, including `qu` and `gi` as onset digraphs
  (`quá`, `giữ` — never `qùa`), and a switch between the traditional `hòa` and
  the modern `hoà`;
- **double-key undo** — `as` → `á`, `ass` → `as`; `aa` → `â`, `aaa` → `aa`;
- **spelling check** — a diacritic that would produce an impossible syllable is
  refused and the key is typed literally (`batf` stays `batf`);
- **auto-restore** — a finished word that turns out not to be Vietnamese is
  rewritten with your original keystrokes, so typing `away` in Telex leaves
  `away` and not `ăay`. This fires on a printable word separator (space,
  punctuation); Enter and Tab end the word without restoring it.

---

## Extending it without a compiler

Settings → Custom rules → **Open config folder**. You get:

```
rules/       *.json  — extra languages and text expansions
layouts/     *.json  — keyboard layout overrides
german-exceptions.txt
```

A rule file becomes a new entry in the language selector, with its own tray
badge, as soon as you press **Reload**:

```json
{
  "id": "es",
  "displayName": "Spanish accents",
  "badge": "ES",
  "rules": [
    { "trigger": "n~", "replacement": "ñ" },
    { "trigger": "e'", "replacement": "é" },
    { "trigger": "btw", "replacement": "by the way", "smartCase": true }
  ]
}
```

`config/custom-rules.example.json` documents every field.

**Layout overrides.** The key mapper resolves characters from the libuiohook
virtual key code, and its built-in table is US QWERTY. On QWERTZ the physical Y
and Z keys are swapped, and on AZERTY most of the alphabet moves — which matters
because Telex's `z` and `w` are input-method keys. Drop a file in `layouts/`:

```json
{ "keys": {
    "VC_Y": { "plain": "z", "shift": "Z" },
    "VC_Z": { "plain": "y", "shift": "Y" }
} }
```

Files named `*.example.json` are treated as documentation and skipped.

**A new language in C++** means subclassing `LanguageRuleEngine`
(`src/core/LanguageRuleEngine.hpp`) and one `registerEngine()` call. Nothing in
the tray, the settings window or the persistence layer needs to know about it:
they all work from the engine descriptor and its string id.

---

## Troubleshooting

**"Keyboard hook failed to start" on Windows.** Another program already owns a
low-level keyboard hook and refuses to chain. Common culprits: other IMEs,
macro tools, some gaming overlays.

**Nothing happens on macOS even though Accessibility is on.** Either you
rebuilt the app after granting it (see the rebuild trap above) or the focused
field has Secure Event Input on.

**Nothing happens on Linux.** Check the status pane: if it says Wayland, that is
the answer. If it says X11 but the hook failed, the X server is missing the
`XRecord` extension (`xdpyinfo | grep RECORD`).

**Characters come out in the wrong case or the wrong letter** on a non-US
physical keyboard: add a layout override, as above.

**`ss` keeps turning into `ß` in words where it should not.** Add the word to
`german-exceptions.txt` under `ss:`, or turn the rule off. The list ships with
the common cases but no list is complete.

**It ate a keystroke.** Set `SCHNELLERTYPE_DEBUG=1` and run from a terminal:
every keystroke, and every edit the engine emits, is traced to stderr. That is
the tool that found the two hardest bugs in this codebase.
