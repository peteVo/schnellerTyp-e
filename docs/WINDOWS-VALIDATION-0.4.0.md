# Windows 0.4.0 verification

Validated on 7 November 2026 using Qt 6.11.2, MinGW 13.1.0, CMake/Ninja and
Windows PowerShell 5.1. The baseline is `ec8f75943c6486e304c63420398e225241f63ca6`
plus the Windows composition and packaging fixes in this release.

## Build and automated suites

The full Release application, QML resources and Windows hook compile with
warnings treated as errors. All five CTest suites pass:

- Core engines and composition: 181 checks, including physical modifier-event
  sequences, current-word force/undo, application shortcuts and AltGr exclusion.
- Automatic corpus: 181 cases plus five option cases.
- Vietnamese regressions: 116 cases across manual Vietnamese, Automatic and VNI.
- English audit: all 30,000 entries; zero unexpected protected-word changes,
  invalid edits or reset failures. Seventy-six ambiguous short-word context
  changes are reported separately, not counted as language-detection accuracy.
- Native settings: 17 checks with an isolated temporary profile.

## Real Windows hook and portable startup

The scratch-editor probe passed **77 checks, zero failures**: 52 Automatic
Telex (51 samples plus stress), 15 Automatic VNI and 10 manual Vietnamese.
The probe generates Windows key events and exercises the actual hook and
replacement delivery. A human launched the final run so the scratch field
could receive focus; these are not physical-keyboard certification results.

Force/undo cases press Ctrl and left Alt separately and keep both held for
75 ms after V/Z. All passed. Coverage includes `heens` forced to `hến`, tone
undo, repeated modifiers, mixed languages, addresses, paths, editing and
20 mixed-language lines at zero inter-key delay. The probe aborts on focus
loss. Failed or skipped preliminary runs are excluded from passing totals.

Portable startup passed after removing developer Qt/QML paths: QML loaded and
initialization completed with the hook disabled for the startup test. The
packaged executable matched the validated build:
`B7A935758FE3E0F1E3F88FB3A5885B8D3A06C07A2E17D11928D770988D1DC031`.
The temporary smoke-test process was stopped after verification.

## Bugs verified and fixed

Physical Ctrl/Alt modifier presses previously reset Automatic before the
composition command arrived. Four new regression assertions failed before
the event-order fix and passed afterward. Ordinary application chords still
reset/pass through, and AltGr is excluded from composition commands.

The Windows injector also canceled explicit composition edits while modifiers
were held. Those edits now wait for release for at most two seconds, with
shutdown/generation cancellation and a fresh target/focus check. Ordinary
queued edits retain the existing held-modifier cancellation behavior.

PowerShell 5.1 packaging treated a nonfatal Qt stderr warning as a fatal error.
The deployment call now permits warning output, restores the previous error
preference and still fails on a nonzero exit. Actual packaging then passed.

## Scope limits

The scratch editor and startup checks do not establish universal compatibility
with browsers/editors, keyboard layouts, elevated apps or physical keyboards.
General visual UI review was not performed in this validation. Linux/macOS
desktop behavior was not retested on this Windows machine.

The terminal redesign and broader Vietnamese-priority policy remain on their
separate branch. This report covers the live-composition Windows release line.
