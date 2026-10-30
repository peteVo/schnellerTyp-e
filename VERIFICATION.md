# Verification — 30 October 2026

## Windows

- Full Windows x64 Release build: passed using Qt 6.11.2, MinGW 13.1.0,
  CMake/Ninja, and warnings treated as errors.
- Core engine/composition suite: 133 checks passed.
- Expanded Automatic corpus: 181 typing/editing cases and 5 option cases passed.
  Includes Strasse/Strassen, Grüße, Fuß, Größe, German compounds, SS/ẞ casing,
  literal ss exceptions, English false positives, Telex/VNI, mixed sentences,
  personal literals, Escape, Backspace and command boundaries.
- English dictionary audit: all 30,000 entries preserved after `This is `;
  all 7,080 high-frequency entries also preserved in isolation. Per-key
  backspace bounds, committed-prefix preservation and reset determinism passed.
  The remaining 22,920 lower-frequency entries were also observed in isolation:
  15 changed, including ambiguous German forms (fuss, sass, weiss) and short
  typing sequences (oo, ee, ue). Those observations are not counted as correct
  English detections or hidden as passing accuracy tests.
- Native preference loader: 6 checks passed with an isolated temporary INI
  profile. The test refuses to run outside that profile.
- Real Windows hook/injection probe: 30 checks passed in its private Win32 edit
  control and a separate hook process. Samples cover German sharp S/umlauts and
  literal ss exceptions, Vietnamese, English regressions, three-language input,
  email/URL/path protection, Backspace, Escape and Enter ordering. Delays range
  from 0 to 20ms per key; the final check types 20 mixed-language lines at 0ms.
- Separate VNI real-input run: 10 checks passed, including tiếng Việt, người,
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
not counted as a pass. Expanded successful reports: 30 Telex and 10 VNI checks,
0 failures.

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
  builds/tests passed in [run 41115058356](https://github.com/peteVo/schnellerTyp-e/actions/runs/41115058356)
  at commit `18f122e`. This resolved actual compiler/API failures in the first
  hosted run. The subsequent Automatic expansion is covered by the local
  checks above; consult the release/Actions for its final hosted run.

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
