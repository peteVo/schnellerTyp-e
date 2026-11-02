# schnellerTyp-e 0.2.2 — Omarchy / Fcitx 5 fixes

This release hardens the native Linux input method and adds an installable
Arch/Omarchy x86_64 package. It fixes text being committed twice when leaving a
field, places the cursor correctly after Vietnamese/German preedit text, and
commits text before clearing preedit. Client-disabled, password and sensitive
fields bypass processing when the client reports those capabilities.

Fcitx now exposes built-in language modes, Telex/VNI, German rules and literal
words in its normal configuration dialog. The native addon needs Fcitx 5 and
Qt Core, without libuiohook or the legacy desktop app. The package includes
licenses and can be upgraded or removed through pacman.

## Install on Omarchy / Arch x86_64

Download `schnellertype-e-fcitx5-0.2.2-1-x86_64.pkg.tar.zst` and `SHA256SUMS`.
The debug package is optional and only needed for troubleshooting.

```sh
sha256sum --ignore-missing -c SHA256SUMS
sudo pacman -U ./schnellertype-e-fcitx5-0.2.2-1-x86_64.pkg.tar.zst
```

Install the `fcitx5-gtk` and `fcitx5-qt` toolkit integration packages. Finish the
current word, restart Omarchy's existing `omarchy-fcitx5.service`, then add and
select **schnellerTyp-e** in `fcitx5-configtool`. Keep existing sources available
for switching back. No second Fcitx autostart or global keyboard hook is needed.

The supported launch configuration uses `QT_IM_MODULE=fcitx`,
`GTK_IM_MODULE=fcitx`, and Chromium `--gtk-version=4`. Do not combine the
Chromium GTK route with `--enable-wayland-ime`. Restart applications to pick up
environment/flag changes. The package does not overwrite user configuration.
See [setup, upgrades and rollback](https://github.com/peteVo/schnellerTyp-e/blob/v0.2.2/packaging/linux/fcitx5/README.md).

## Verification and limits

- Six local CTest suites pass, including 42 checks that load the shared Fcitx
  addon and exercise composition, lifecycle, sensitive fields and settings.
- The v0.2.2 private desktop matrix passed 70 checks across Qt, GTK and
  Chromium on Wayland/XWayland at 20 ms per generated character. The preceding
  implementation validation also passed 14 checks against the installed package
  and four checks on the actual Hyprland session.
- This release includes repeatable private-desktop and opt-in live Omarchy
  probes. Final passing synthetic reports are attached. The live host probe was
  not repeated during release preparation because the user is actively typing.
- Native GTK text-input-v3 without the Fcitx GTK module remains experimental:
  rapid synthetic runs lost words. A 5 ms Chromium password-input run also lost
  characters. These failing paths/rates are not part of the supported setup.
- Password bypass depends on correct client sensitivity reporting. Masking a
  GTK entry without declaring its password purpose is insufficient.
- Logout/login startup and universal application/keyboard compatibility have
  not been established. The local service restart and existing startup wiring
  were checked. Full details: [verification](https://github.com/peteVo/schnellerTyp-e/blob/v0.2.2/VERIFICATION.md).

The binary downloads in this release target Arch/Omarchy x86_64. No new Windows
or macOS binaries were built on this Linux machine; existing Windows downloads
remain available in v0.2.1. GitHub's cross-platform source checks are separate
from desktop runtime compatibility.
