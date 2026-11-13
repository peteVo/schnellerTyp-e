# schnellerTyp-e Linux 0.4.0 — Automatic editing and Omarchy support

This Linux release updates the Fcitx 5 typing assistant for mixing English,
Vietnamese and German within one word stream. It runs locally and keeps the
existing physical keyboard layout independent from text conversion.

## Changes

- Automatic mode can correct live Vietnamese tones, edit unfinished words at
  the caret, and restore the last 128 current-word edits with **Ctrl+Alt+Z**.
- **Ctrl+Alt+V** temporarily composes the current word as Vietnamese, including
  when spelling or personal literal rules would otherwise keep it unchanged.
- **Ctrl+Alt+L** commits the unfinished word literally and remembers that
  spelling. Learned words can be reviewed in Fcitx settings.
- Fcitx provides native mode controls, Telex/VNI and German options, exact
  application-specific mode preferences, and the Status & Setup diagnostics
  tool.
- The package uses the native Fcitx adapter and does not install the legacy
  keyboard hook or a second startup service.

## Install on Omarchy / Arch x86_64

Download `schnellertype-e-fcitx5-0.4.0-1-x86_64.pkg.tar.zst` and `SHA256SUMS`.
The debug-symbol package is optional.

```sh
sha256sum --ignore-missing -c SHA256SUMS
sudo pacman -U ./schnellertype-e-fcitx5-0.4.0-1-x86_64.pkg.tar.zst
```

Install `fcitx5-gtk` and `fcitx5-qt` for the toolkits you use. Add
**schnellerTyp-e** in `fcitx5-configtool`, select Automatic, and keep your
English (US) physical layout. For Chromium use the Fcitx GTK module with
`--ozone-platform=x11 --gtk-version=4`; do not combine it with
`--enable-wayland-ime`. See the [Omarchy setup and rollback guide](packaging/linux/fcitx5/README.md).

## Verification and limits

- All seven native CTest suites passed with compiler warnings treated as
  errors.
- The installed package passed 140 checks at 20 ms per generated character
  across GTK Wayland, Qt Wayland, Qt XWayland and Chromium XWayland.
- Physical-keyboard checks passed in the actual Hyprland session using Qt
  Wayland and Chromium XWayland, including mixed-language text, field changes
  and literal input in a synthetic password field. The post-login service,
  input environment, Chromium flags and English (US) keyboard layout persisted.
- A separate Chromium native Wayland run dropped a character in the synthetic
  password sample (`hoom nuawx` appeared as `hom nuawx`). That route remains
  experimental; Chromium XWayland is the supported path.
- Automatic interpretation uses offline frequency data and context. Ambiguous
  short words can still need Escape or a personal literal entry. Password
  bypass depends on the application reporting sensitive fields correctly.

The package is Linux x86_64. The Windows `windows-v0.4.0` release is a separate
build and does not establish Linux compatibility.
