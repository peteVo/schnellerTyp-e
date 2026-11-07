# Windows x64 — schnellerTyp-e 0.4.0

Portable Windows build using Qt 6.11.2 and MinGW 13.1.0. Download the ZIP,
verify it against `SHA256SUMS`, extract the folder and run `schnellerTyp-e.exe`.
Qt and compiler runtimes are included; developer tools and installation are
not required. Select **Automatic** to mix Vietnamese, English and German.

## Typing changes and fixes

- Recognizes short Vietnamese input such as `nos`, `maf` and `af`, including
  after English text. Tone changes remain editable before Space commits a word.
- **Ctrl+Alt+V** composes the current word as Vietnamese; **Ctrl+Alt+Z** undoes
  the last current-word edit or override. For example, `heens` followed by
  Ctrl+Alt+V produces `hến`. Escape restores the original keys before commit.
- Windows modifier presses now preserve the current word until the command
  arrives. Explicit composition edits wait for Ctrl/Alt release, with bounded
  cancellation, while ordinary application shortcuts and focus changes keep
  their cancellation behavior. Right Alt/AltGr does not invoke these commands.
- Fixes portable packaging under Windows PowerShell 5.1 when Qt emits a
  nonfatal deployment warning; failed deployment exit codes still stop packaging.

## Verification and limits

- Full Windows Release build with warnings treated as errors; all five CTest
  suites passed, including 181 core checks and 17 native settings checks.
- All 77 Windows scratch-editor keyboard checks passed: 52 Automatic Telex,
  15 Automatic VNI and 10 manual Vietnamese. These include held-modifier
  force/undo checks, addresses/paths, Escape/Backspace/Enter and 20 mixed-language
  lines typed with zero inter-key delay.
- The portable application starts with its bundled dependencies and no
  developer Qt environment. See `WINDOWS-VERIFICATION.md` for the test scope.
- These generated-input checks do not certify every browser, editor, keyboard
  layout, elevated window or physical keyboard. General visual UI review and
  broad application compatibility remain unverified for this release.
- This is the live-composition development line. The separate terminal UI and
  broader Vietnamese-priority changes on `codex/terminal-ui` are not merged here.

Windows and Linux downloads have separate release channels. The current Linux
binary remains the existing Omarchy/Fcitx 5 0.2.2 release; `codex/linux` contains
newer source development. This Windows release supplies Windows binaries.
