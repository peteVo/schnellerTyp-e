# Verification — 30 October 2026

## Windows

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

## Linux and macOS

Fcitx 5 and InputMethodKit native adapters, opt-in build targets, installation
instructions and hosted build workflows are included. Native desktop runtime
behavior is **unverified**:

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

## 0.3.0 desktop interface

- Final Qt/MinGW Windows build completed successfully; all five CTest suites
  passed.
- Reviewed Typing, Preferences and Setup at the default 820 × 740 px and minimum
  640 × 560 px window sizes. Tab navigation reveals the editor and actions.
- The live preview transformed `Strasse nuawx ww New York` to
  `Straße nữa w New York` using the configured typing engine.
- Runtime review produced no warnings. Dark appearance received the visual
  review. Light-mode text contrast was checked against the source palette and
  measured 5.19:1–14.83:1 for the checked text/surface pairs. The selected dropdown
  item's source colors measure 14.01:1 in dark mode and 13.76:1 in light mode.
  The compact language-menu review confirmed the selected item remains legible.
- The existing 67 real-input engine checks are inherited verification and were
  not rerun for this interface update. The typing engine was unchanged.
- This desktop review covers Windows only; Linux and macOS desktop runtime remain
  unverified as described above.
