# Verification

See [the 3 November readiness audit](docs/PRODUCTION-READINESS.md) for the
plain-Fcitx reproduction, validated Chromium XWayland workaround, physical
keyboard check and published-package lifecycle tests. It narrows the earlier
Chromium Wayland support claim below. The 13 November logout/login check is
recorded in the 0.4.0 acceptance section.

## Development candidate 0.4.0 — 6 November 2026

This changes the shared typing logic: weak short English matches can yield to
Vietnamese, and a word that has visibly become Vietnamese keeps valid tone
corrections even when the new spelling is absent from the frequency table.
Ctrl+Alt+V provides a dictionary-independent word override; Ctrl+Alt+Z restores
the prior current-word state. Neither writes preferences or typing history.
Automatic's native composition session also supports caret editing before
commit. Core behavior remains free of Qt/OS dependencies for Windows reuse.

- Seven local CTest suites passed, including new live conversion, correction,
  undo, override, VNI, caret, boundary and AltGr cases. A deterministic 2,000-key
  edit sequence also checked text and cursor bounds.
- The 30,000-word English audit preserved all 7,080 high-frequency words and
  all longer rare words in English context. Vietnamese won 76 ambiguous short
  rare-English context cases; this is the deliberate short-word tradeoff.
- The private four-client matrix passed 140 checks: GTK Wayland 32, Qt Wayland
  35, Qt XWayland 35 and Chromium XWayland 38. The new cases assert live preedit
  text, tone replacement/removal, current-word undo, temporary override and
  caret editing before Space, in addition to the existing regression gates.
- Windows hook sample coverage now includes the reported short words and
  live tone corrections. This machine cannot execute that interactive probe;
  Windows application/runtime compatibility is not claimed by these results.

At the time of this 6 November candidate validation, the installed 0.3.0
package remained unchanged. The subsequent installed-package acceptance check
is recorded below.

## Installed package acceptance — 13 November 2026

The Omarchy host has `schnellertype-e-fcitx5 0.4.0-1` installed (built and
installed 10 November). Its Fcitx module SHA-256 matches the retained package
archive exactly. The checkout is `codex/linux` at
`bc9af9078e62997eb16f5148536140fde0451eed`.

- Rebuilt the current checkout with warnings treated as errors. All seven
  CTest suites passed.
- The installed-package private desktop matrix passed **140/140 checks** at
  20 ms per generated character: GTK Wayland 32, Qt Wayland 35, Qt XWayland
  35, and Chromium XWayland 38. Coverage includes live conversion, tone
  correction, caret edits, current-word override and undo, focus boundaries,
  sensitive-field bypass, paste, AltGr, and pause/resume. The isolated Fcitx
  session reported the installed addon loaded.
- A separate run of the optional Chromium native Wayland route reproduced
  dropped input: `hoom nuawx` appeared as `hom nuawx` in the password field.
  This route remains experimental; Chromium XWayland is the supported path.
- Read-only checks on the actual Hyprland session found the existing Fcitx
  service enabled and active, `keyboard-us` selected, and all attached
  keyboards using English (US).
- A coordinated physical-keyboard check in the actual Hyprland Qt scratch
  window passed on the installed package: mixed English/German/Vietnamese
  text, an independent second field, and literal synthetic password input.
- After a logout/login, the Fcitx service remained enabled and active, the
  user's systemd environment retained the GTK/Qt Fcitx modules, Chromium's
  supported XWayland flags remained configured, and the Fcitx profile retained
  schnellerTyp-e as its default input method. No app settings file exists, so
  the default Automatic mode remains in effect. Every keyboard remained on
  English (US), and Hyprland reported no configuration errors.
- A second physical-keyboard check after login passed in a fresh Chromium
  XWayland profile on the installed package, including mixed-language input,
  an independent field, and literal synthetic password input. The probe
  restored the previous active source (`keyboard-us`) and Fcitx state.

A reboot cycle and longer ordinary-use session remain pending.

## Development candidate 0.3.0 — 4 November 2026

This candidate adds native Fcitx status/menu controls, Automatic-mode literal
learning, exact app-ID preferences, explicit unsupported-profile passthrough,
and a Qt Status & Setup companion with backed-up repairs and a restricted
diagnostic export. It has not been published as a release.

- All seven local CTest suites passed with warnings treated as errors:
  65 Fcitx adapter checks, 17 shared-settings checks and 14 setup/privacy
  checks, plus the existing engine/corpus/regression suites.
- The expanded private desktop matrix passed 96 checks: GTK Wayland 21,
  Qt Wayland 24, Qt XWayland 24 and Chromium XWayland 27. New cases cover
  selection replacement, deletion undo/redo in Qt/Chromium, Unicode paste,
  typing after paste, actual pointer placement during preedit, held keys,
  Caps Lock, German-layout Unicode/AltGr, the real Ctrl+Alt+L learning
  shortcut, and diagnostic export excluding a newly learned word.
- Rapid Chromium XWayland password comparisons passed 15/15 transitions
  each with the addon and plain Fcitx at 5 ms per character.
- The normal and minimum-size setup windows were inspected offscreen. Native
  Qt controls follow the system palette; no custom theme dependency was added.
- The Arch candidate package built and ran all seven suites. Setup checks
  were also read against the live session without altering it.
- Isolated pacman install/upgrade/reinstall/rollback/upgrade/removal from
  v0.2.2 to this candidate passed all six stages. Package ownership checks
  reported 27 intact files and the synthetic preference sentinel survived.
  As before, dependencies/scripts were skipped only in this empty test root.

These checks use synthetic text in isolated sessions. Undo granularity is
owned by the client; the undo/redo gate specifically verifies selection
deletion of already composed text. The earlier physical-keyboard results
below describe v0.2.2 and have not been repeated for this candidate. Login,
reboot and longer ordinary-use checks remain pending. The Chromium native
Wayland limitation remains unchanged.

## Release v0.2.2 — 2 November 2026

The v0.2.2 Arch package (`schnellertype-e-fcitx5-0.2.2-1-x86_64.pkg.tar.zst`)
was rebuilt from the release source with warnings treated as errors. Its
makepkg build ran all six CTest suites successfully, including 42 dynamically
loaded Fcitx adapter checks. The private five-client desktop matrix was rerun on v0.2.2:
70/70 checks passed at 20 ms per generated character. The local installed
package and live-session
checks below were performed on the preceding 0.2.1-2 package with the same
adapter implementation; release preparation changes its version metadata.
No further input was sent to the active desktop during release preparation.
Final synthetic reports and portable SHA-256 checksums accompany the release.
Hosted cross-platform checks are reported separately on GitHub.

## Omarchy / Linux — implementation validation, 2 November 2026

The locally verified configuration uses Fcitx toolkit modules on Wayland and
XWayland: `QT_IM_MODULE=fcitx`, `GTK_IM_MODULE=fcitx`, and Chromium
`--gtk-version=4`. The legacy keyboard hook is not used.

- Release build with warnings treated as errors: passed on Omarchy,
  GCC 16.2.1, CMake 4.4.4, Qt 6.12.0 and Fcitx 5.1.23.
- All six CTest suites passed. The adapter suite dynamically loads the built
  shared addon through Fcitx's real addon loader and runs 42 checks. Coverage
  includes composition, UTF-8 cursor placement, commit order, one-time
  focus-out commits, editing, shortcuts, field isolation, password/sensitive/
  disabled capability transitions, saved preferences, native configuration,
  invalid mode validation, and immediate preference changes in existing fields.
- Fixed duplicate focus-out commits (`tôi` became `tôitôi`), missing preedit
  cursor placement, and commit-before-clear ordering. Password, sensitive and
  client-disabled capabilities bypass processing. Native Fcitx configuration
  now exposes built-in preferences without libuiohook or Qt Quick.
- **70/70 desktop checks passed**, at 20 ms per generated character, on a
  private headless Sway 1.12 desktop with separate D-Bus, settings and scratch
  documents. Each client passed 14 checks including mixed-language typing,
  literal recovery, editing, focus changes, password bypass, pause/resume and
  20 repeated mixed-language sentences.

| Client | Display | Input path | Result |
|---|---|---|---|
| Qt 6.12.0 scratch editor | Wayland | Fcitx Qt module | 14/14 |
| GTK 3.24.52 scratch editor | Wayland | Fcitx GTK module | 14/14 |
| Qt 6.12.0 scratch editor | XWayland | Fcitx Qt module | 14/14 |
| Chromium 155.0.8059.84 | Wayland | GTK 4 / Fcitx module | 14/14 |
| Chromium 155.0.8059.84 | XWayland | GTK 4 / Fcitx module | 14/14 |

- The pacman-installed, stripped module additionally passed the 14 Qt Wayland
  checks on the private desktop; it was not substituted by the build artifact.
- The installed addon passed **4/4 checks on the actual Hyprland 0.56.2
  session**: mixed typing, one-time focus commit, independent fields and
  password bypass. The bounded Qt scratch test targeted its own window,
  checked focus, temporarily shared input state, then restored the original
  global settings, source group and activation state.
- Earlier live-window runs were invalidated by unreliable synthetic input and
  concurrent user typing (confirmed by the user). They are not counted as
  addon failures or passing compatibility evidence. The final host probe uses
  Omarchy's explicit-modifier, window-targeted Hyprland dispatcher.
- Built `schnellertype-e-fcitx5-0.2.1-2-x86_64.pkg.tar.zst` with makepkg;
  its package build ran all six suites. Install/reinstall/removal and file
  ownership records were exercised in a separate pacman root. That unprivileged
  test root skips dependency resolution and emits ownership warnings; the
  actual host install used normal pacman dependency checks and privileges.
- Host package installation passed; `pacman -Qk` reports 23 files, none missing.
  The installed module's SHA-256 matches the package payload.
  Fcitx discovers the addon after restarting the existing
  `omarchy-fcitx5.service`; its configuration schema was retrieved through the
  actual service. There is no second startup service.
- Saved supported GTK/Chromium launch settings with backups, and added
  schnellerTyp-e beside the existing sources. The existing Unikey default
  and current activation choice were preserved. Existing applications were
  not restarted; new browser processes pick up the saved flags.
- Installable package, debug symbols, SHA-256 checksums and synthetic test
  reports are retained in `dist/omarchy/` (ignored build artifacts).

### Compatibility limits and failed diagnostic runs

The native GTK text-input-v3 path (without the Fcitx GTK module) lost words in
5 ms synthetic bursts, even after commit-order correction. It is experimental,
not the supported launch configuration. The harness exposes it explicitly as
`--native-wayland-protocol`; a failing run exits nonzero. The GTK-module path
passed the final 20 ms matrix. A 5 ms Chromium toolkit-module password run also
lost characters; no reliability claim is made for that extreme synthetic rate.
Failed diagnostic runs are retained separately from the final passing reports.

Password bypass requires client reporting. A GTK 3 entry with only visibility
turned off advertised free-form content, so the addon could not identify it as
a password. The password fixture sets an explicit password purpose. The Qt and
HTML password fixtures use their real toolkit/browser password controls.
Unreported sensitive fields require pausing input assistance.

These results cover the listed clients/configuration and synthetic input, not
every application, physical keyboard layout or typing rate. A logout/login
startup cycle and hosted CI have not been rerun here. Service restart and
existing login-service wiring were checked. The addon uses saved built-in modes;
custom JSON profiles remain outside native-adapter support. Use the
[package/setup guide](packaging/linux/fcitx5/README.md) for installation,
activation, upgrades, rollback and the repeatable probes.

## Windows — 30 October 2026

- Full Windows x64 Release build: passed using Qt 6.11.2, MinGW 13.1.0,
  CMake/Ninja, and warnings treated as errors.
- Core engine/composition suite: 133 checks passed.
- The v0.2.1 patch adds 116 unique Vietnamese regression cases: 56 manual
  Telex, 46 Automatic and 14 VNI cases; all five CTest tests passed. Coverage
  includes standalone `ww` undo versus `uww`, delayed `nuawx`/`nuwax` → `nữa`,
  literal undo surviving Space, and Automatic preserving unknown raw literal
  undo while still recognizing known English and German words. A literal `w`
  does not establish Vietnamese context: `ww cars hats moon ` stays
  `w cars hats moon `.
- Expanded Automatic corpus: 181 typing/editing cases and 5 option cases passed.
  Includes Strasse/Strassen, Grüße, Fuß, Größe, German compounds, SS/ẞ casing,
  literal ss exceptions, English false positives, Telex/VNI, mixed sentences,
  personal literals, Escape, Backspace and command boundaries.
- English dictionary audit: all 30,000 entries preserved after `This is `;
  all 7,080 high-frequency entries also preserved in isolation. Per-key
  backspace bounds, committed-prefix preservation and reset determinism passed.
  The remaining 22,920 lower-frequency entries were also observed in isolation:
  16 showed changed isolated output, including ambiguous German forms (fuss,
  sass, weiss) and the explicit `ww` → `w` Telex shortcut. The audit found zero
  unintended English regressions; these observations are not counted as correct
  English detections.
- Native preference loader: 6 checks passed with an isolated temporary INI
  profile. The test refuses to run outside that profile.
- v0.2.0 baseline real Windows hook/injection probe: 30 checks passed in its private Win32 edit
  control and a separate hook process. Samples cover German sharp S/umlauts and
  literal ss exceptions, Vietnamese, English regressions, three-language input,
  email/URL/path protection, Backspace, Escape and Enter ordering. Delays range
  from 0 to 20ms per key; the final check types 20 mixed-language lines at 0ms.
- v0.2.0 baseline separate VNI real-input run: 10 checks passed, including tiếng Việt, người,
  giữa, the New York sentence, English/German in VNI, digit composition,
  Backspace, Escape and Enter. One earlier run lost focus before typing and
  was recorded as a skip; the test now waits for 250ms of stable focus.
- UI inspected on Windows: Typing with the requested New York example,
  Preferences and Setup. The normal GPU-backed app started successfully with
  keyboard capture disabled for visual inspection.
- QML compiled. The linter still warns about the dynamically registered backend
  singleton's missing static import metadata and some legacy unused components;
  the application loaded and the live bindings worked during inspection.
- Git whitespace validation passed.

The scratch probe aborts if it does not own foreground/focus. Earlier skipped
runs and failed runs were used to find and fix recapture/reordering; a skip is
not counted as a pass. Previously verified v0.2.0 reports: 30 Telex and 10 VNI
checks, 0 failures. The v0.2.1 real-input run passed all 67 checks: 44 Automatic
Telex checks including `ww`, `nuawx` and the 20-line stress case; 13 Automatic
VNI checks; and 10 manual Vietnamese checks. Final rerun reports: 0 failures.

Early patch runs exposed interference from an enabled older copy and a test
reset mistake: Escape restores raw keys instead of clearing the current word.
The final scratch helper ends each word with navigation before clearing its
editor. The older copy was paused during isolated checks, then its enabled
setting was restored. Failed and skipped runs are not counted as passes.

The installed MSVC toolchain has a local PDB manager mismatch, so the delivered
portable binary uses the matching Qt/MinGW kit. MSVC-specific packaging remains
available but was not exercised for this update.

## Linux and macOS — previous verification, 30 October 2026

Fcitx 5 and InputMethodKit native adapters, opt-in build targets, installation
instructions and hosted build workflows are included. At that time local native
builds and desktop runtime behavior were **unverified**; the Omarchy section
above supersedes the Linux local build/callback status:

- The available WSL Arch environment lacks build dependencies. Disposable Linux
  container/dependency downloads were blocked by network/mirror failures. No
  host/WSL packages were installed, and the Fcitx addon was not compiled here.
- No macOS SDK/runtime was available, so InputMethodKit was not compiled or run.
- Hosted portability baseline: core builds/tests passed on Windows/MSVC,
  Ubuntu and macOS, and native Fcitx 5 (Ubuntu 24.04) and InputMethodKit (macOS)
  builds/tests passed on v0.2.0 release commit
  `83b1f3fe0d40acb0eb37b30b378d83baee7f0fd7` in [run 41117319424](https://github.com/peteVo/schnellerTyp-e/actions/runs/41117319424).
  See the v0.2.1 release notes for the patch's hosted run. Hosted builds/tests
  do not establish desktop runtime compatibility.

## Limits

Automatic selection uses offline word frequencies and one committed word of
language context. No general language-detection accuracy percentage has been
measured. The samples above do not establish compatibility with every editor,
browser, keyboard layout, elevated window, game or remote desktop client.
Common English words are protected in a fresh or English context; strong
German homographs such as `fuss`/`weiss` can still convert at a fresh start.
A pinned CC0 spelling list checks sharp-S candidates independently of folded
frequencies; the list is incomplete and is not a universal spellchecker.
Ambiguous words can still require context, Escape before Space or a personal
literal entry. Native adapters currently support saved built-in modes; custom JSON mode
selection falls back to Automatic there.
