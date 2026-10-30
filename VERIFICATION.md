# Verification — 30 October 2026

## Windows

- Full Windows x64 Release build: passed using Qt 6.11.2, MinGW 13.1.0,
  CMake/Ninja, and warnings treated as errors.
- Core engine/composition suite: 133 checks passed.
- Native preference loader: 6 checks passed with an isolated temporary INI
  profile. The test refuses to run outside that profile.
- Real Windows hook/injection probe: 7 checks passed in its private Win32 edit
  control and a separate hook process. Samples cover Vietnamese at 20ms and 5ms
  per key, German at 5ms, English literal words, visible-character Backspace,
  Escape restoration and Enter ordering.
- UI inspected on Windows: Typing with the requested New York example,
  Preferences and Setup. The normal GPU-backed app started successfully with
  keyboard capture disabled for visual inspection.
- QML compiled. The linter still warns about the dynamically registered backend
  singleton's missing static import metadata and some legacy unused components;
  the application loaded and the live bindings worked during inspection.
- Git whitespace validation passed.

The scratch probe aborts if it does not own foreground/focus. Earlier skipped
runs and failed runs were used to find and fix recapture/reordering; a skip is
not counted as a pass. The final successful report is 7 checks, 0 failures.

The installed MSVC toolchain has a local PDB manager mismatch, so the delivered
portable binary uses the matching Qt/MinGW kit. MSVC-specific packaging remains
available but was not exercised for this update.

## Linux and macOS

Shared core tests run on Windows. Fcitx 5 and InputMethodKit native adapters,
opt-in build targets, installation instructions and hosted build workflows are
included. Their actual platform status is **unverified**:

- The available WSL Arch environment lacks build dependencies. Disposable Linux
  container/dependency downloads were blocked by network/mirror failures. No
  host/WSL packages were installed, and the Fcitx addon was not compiled here.
- No macOS SDK/runtime was available, so InputMethodKit was not compiled or run.
- Hosted workflows have been added to the source; no hosted run is claimed.

## Limits

Automatic selection uses offline word frequencies and one committed word of
language context. No general language-detection accuracy percentage has been
measured. The samples above do not establish compatibility with every editor,
browser, keyboard layout, elevated window, game or remote desktop client.
Ambiguous words can still require Escape before Space or a personal literal
entry. Native adapters currently support saved built-in modes; custom JSON mode
selection falls back to Automatic there.
