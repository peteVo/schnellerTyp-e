# Omarchy production checks — 3 November 2026

## Supported launch configuration

Use Fcitx toolkit modules (`QT_IM_MODULE=fcitx`, `GTK_IM_MODULE=fcitx`).
Native Wayland Qt/GTK module clients and XWayland Qt clients remain supported.
For Chromium use `--ozone-platform=x11 --gtk-version=4`; this runs a browser
window through XWayland on the Wayland desktop. Native Chromium Wayland is
experimental until its rapid password-input problem is resolved. Do not mix
`--enable-wayland-ime` with the GTK module route. New browser processes are
needed for saved launch flags to take effect.

This is a narrower contract than the initial v0.2.2 setup guide. The machine's
Chromium flags were updated with a timestamped backup; running windows were not
restarted. No global GTK synchronous-processing flag was installed.

## Dropped-input comparison

Environment: Chromium 155.0.8059.84, Fcitx 5.1.23, fcitx5-gtk 5.1.7,
private Sway 1.12, GTK 4 Fcitx module, standard US-layout virtual keyboard.
All passwords below are generated test strings, never real credentials.

| Route | Engine | Rate | Password transitions | Result |
|---|---|---|---|---|
| Chromium Wayland | schnellerTyp-e | 5 ms/character | 100 | 1 failure, first transition |
| Chromium Wayland | plain Fcitx keyboard | 5 ms/character | 100 | 1 failure, first transition |
| Chromium Wayland, synchronous GTK | schnellerTyp-e | 5 ms/character | 100 | 100 passed |
| Chromium Wayland, synchronous GTK | plain Fcitx keyboard | 5 ms/character | 100 | 1 failure, first transition |
| Chromium XWayland | schnellerTyp-e | 5 ms/character | 100 | 100 passed |
| Chromium XWayland | plain Fcitx keyboard | 5 ms/character | 100 | 100 passed |

The plain-keyboard profile excludes the addon. `context.json` confirmed
`addon_loaded=false` in the failing baseline, and true in the addon run. Both
asynchronous native-Wayland runs changed the expected
`hoom nuawx repeatedoo 12345` into `hom nuawx repeatedoo 12345`. The original
short-password scenario separately reproduced `hom nuawx` with the addon,
while one baseline original-scenario run passed. This is intermittent and
sensitive to initial focus/setup; a single passing run is not sufficient.

The loss therefore reproduces without our addon. This narrows the fault to
the shared browser/toolkit/input-method path; it does not identify the exact
upstream component. Synchronous GTK processing is not a complete fix. No
addon-engine change is claimed to fix native Chromium Wayland. The supported
workaround is the separately validated XWayland route.

Reproduction from a probe-enabled build:

```sh
python tools/verify-linux-desktop.py --clients chromium-wayland \
  --key-delay-ms 5 --password-repeats 100
python tools/verify-linux-desktop.py --clients chromium-wayland \
  --engine keyboard --key-delay-ms 5 --password-repeats 100
```

Repeat with `--clients chromium-xwayland` for the supported workaround.
`FCITX_ENABLE_SYNC_MODE=1` enables the separate synchronous diagnostic.
Results and launch context are retained per run; any mismatched string returns
nonzero. The baseline must report the addon unloaded. New reports are under
`build-native/production-evidence/`; failed reports are preserved too.

## Additional verification

- All six local CTest suites passed.
- The five-client matrix passed at 5 ms with synchronous GTK processing. This
  is diagnostic evidence, not a supported native-Chromium claim, since the
  longer plain-Fcitx comparison above still failed on that route.
- A coordinated physical-keyboard test passed in the actual Hyprland Qt
  scratch editor: mixed languages, Tab to an independent field, then literal
  password input. The operator typed all keys. Fcitx settings were restored.
  Physical typing in Chromium and a longer ordinary-use session remain pending.
- Published v0.2.2 packages were downloaded from GitHub and matched SHA256SUMS.
  Real privileged pacman transactions in an isolated filesystem root passed:
  install 0.2.1-2, upgrade to 0.2.2-1, reinstall, rollback, upgrade again, remove.
  `pacman -Qkk` reported no altered files after each install; a synthetic user
  preference file survived all transactions. Dependency checks/scripts were
  deliberately skipped in this isolated root, which lacks a full OS.
- The actual host was upgraded to the published 0.2.2-1 package with normal
  dependency checks. All 23 package files are intact. Existing Fcitx login
  service is enabled and active, with toolkit environment settings present.

## Release gates

The Linux CI job now executes the four supported desktop clients and both
rapid XWayland password comparisons, using the runner's Google Chrome. It
uploads synthetic results, launch context and logs, including on failure.
Hosted runs still need to pass; configuring a workflow is not execution proof.

Do not label native Chromium Wayland supported based on a slower passing run.
The native GTK text-input-v3 path also remains experimental. Sensitive-field
bypass requires correct client reporting.

Logout/login and reboot were deferred at the user's request. After an agreed
restart, check `systemctl --user is-enabled omarchy-fcitx5.service`,
`systemctl --user is-active omarchy-fcitx5.service`, the GTK/Qt environment in
new app processes, and selection/settings persistence in Fcitx. Perform the
physical-keyboard test again and test Chromium with the saved XWayland flags.
No reboot or logout has been performed, and no startup-cycle pass is claimed.
