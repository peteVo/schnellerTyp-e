# Fcitx 5 adapter on Omarchy

The native addon composes English, Vietnamese and German text through Fcitx 5.
It needs Fcitx 5 and Qt Core at runtime; libuiohook, XTest, Qt Quick and the
legacy tray app are not required. The normal settings interface is Fcitx's
configuration tool.

## Build and install the Arch package

From the repository root:

```sh
omarchy pkg add gcc cmake ninja qt6-base fcitx5
cd packaging/linux/fcitx5
makepkg -f
sudo pacman -U ./schnellertype-e-fcitx5-0.2.2-1-x86_64.pkg.tar.zst
```

This is a local working-tree package: it includes your current source changes
and runs all six test suites before packaging. Use the matching architecture
filename on aarch64. Install optional `fcitx5-gtk` and `fcitx5-qt` integration
packages for the toolkits you use. Do not install the separate legacy X11
package for native Wayland typing.

Finish the current word before restarting the existing input-method service:

```sh
systemctl --user restart omarchy-fcitx5.service
fcitx5-configtool
```

In the input-method list, add **schnellerTyp-e** (clear the current-language
filter if needed). Keep the existing keyboard and other input sources so you
can switch back. Select schnellerTyp-e, then use its configuration button to
choose Automatic/German/Vietnamese, Telex/VNI, German rules and literal words.
Fcitx's configured activation shortcut pauses/resumes the source. For the
supported app launch configuration, set `GTK_IM_MODULE=fcitx` in the user
environment (for example `~/.config/environment.d/20-schnellertype.conf`) and
set `--ozone-platform=x11 --gtk-version=4` in `~/.config/chromium-flags.conf`. Do not combine it
with `--enable-wayland-ime`; the native protocol route remains experimental.
Back up existing files, preserve unrelated flags, and start new app processes
to apply the change. The current Omarchy installation was configured this way.
No second Fcitx autostart or global keyboard hook is needed.

The addon settings share `~/.config/schnellerTyp-e/schnellerTyp-e.conf` with the
legacy application. Applying settings commits pending ordinary composition
before replacing the engine and takes effect in existing fields. Sensitive
composition is discarded. Settings are also read on activation. Custom JSON
profiles and the legacy app's mode-cycling shortcut are not native-addon
features; native mode choices are the three built-in modes.

## Upgrade, rollback and remove

Rebuild with `makepkg -f`, install the new package with `pacman -U`, then restart
the existing Fcitx service. Pacman owns the addon, descriptors and licenses.
The package does not create startup services or overwrite user preferences.

To roll back, switch to your previous input source and install your previous
package with `sudo pacman -U /path/to/previous.pkg.tar.zst`, then restart Fcitx.
To remove this addon, select another input source, remove schnellerTyp-e from
Fcitx's list, run `sudo pacman -R schnellertype-e-fcitx5`, and restart Fcitx.
User preferences remain available for a later reinstall.

## Repeatable desktop verification

The optional helpers are development tools, not installed by the package:

```sh
omarchy pkg add sway python-websocket-client chromium gtk3 wayland libxkbcommon
cmake -S . -B build-native -G Ninja -DCMAKE_INSTALL_PREFIX=/usr \
  -DSCHNELLERTYPE_BUILD_APP=OFF -DSCHNELLERTYPE_BUILD_FCITX5=ON \
  -DSCHNELLERTYPE_WERROR=ON -DSCHNELLERTYPE_BUILD_LINUX_DESKTOP_PROBES=ON
cmake --build build-native --parallel
ctest --test-dir build-native --output-on-failure
python tools/verify-linux-desktop.py --build-dir build-native
```

Run these commands from the repository root. The harness uses a separate
headless Sway compositor, private D-Bus session, temporary settings, standard
US-layout virtual keyboard and scratch documents. It tests Qt Wayland, GTK
Wayland, Qt XWayland, and Chromium Wayland/XWayland at 20 ms per character. It does not send input to
the active desktop. Logs and observed synthetic text are retained under
`build-native/desktop-verification-*`; failures exit nonzero. See
[VERIFICATION.md](../../../VERIFICATION.md) for actual results and limits.

Password bypass depends on client reporting. In GTK 3, hiding a `GtkEntry` by
itself can still advertise a free-form field; applications must set the
password/PIN purpose or sensitivity hints. A field that hides text without
reporting sensitivity cannot be identified reliably by this addon. Pause
Fcitx for such applications. The supported Chromium route uses `GTK_IM_MODULE=fcitx` with
`--ozone-platform=x11 --gtk-version=4` on the Wayland desktop. Native Chromium
Wayland reproduced password-input loss even with plain Fcitx and is experimental. The optional `--native-wayland-protocol` test mode
exercises native text-input-v3, which failed rapid typing diagnostics and is
not part of the supported configuration. Application/toolkit versions remain part of the
compatibility contract.

The test-only virtual keyboard protocol XML is vendored from
[swaywm/wlroots](https://github.com/swaywm/wlroots/blob/master/protocol/virtual-keyboard-unstable-v1.xml),
with its MIT license retained in the file. The adapter follows the
[Fcitx input-method API](https://www.fcitx-im.org/wiki/Develop_an_simple_input_method).

To check the installed package rather than the staged build, add `--installed`
to the desktop harness command. To run a subset, use e.g.
`--clients chromium-wayland chromium-xwayland`.

The separate `python tools/verify-omarchy-installed.py build-native` probe opens
a visible scratch window on the active Hyprland 0.56 Lua desktop. It uses
window-targeted key dispatch, checks focus, and restores Fcitx configuration
afterward. It requires Python GObject bindings and an idle keyboard while the
scratch window is open. Unlike the private desktop matrix, this is an explicit
interactive check; do not run it during normal typing.

[Readiness audit and comparison results](../../../docs/PRODUCTION-READINESS.md)
describe the current support boundary and pending login/reboot checks.

For physical-keyboard verification, `python tools/verify-omarchy-installed.py
build-native --manual` waits for the operator to type the displayed examples
and sends no synthetic events. Add `--browser` to use a fresh Chromium
XWayland scratch profile. Both modes restore Fcitx configuration afterward;
coordinate an idle window before running them.

`python tools/verify-arch-lifecycle.py --old OLD.pkg.tar.zst --new NEW.pkg.tar.zst`
tests install, upgrade, reinstall, rollback and removal in a separate pacman
root under the build directory. The tool requests privilege only for those
isolated transactions; it does not change host packages.
