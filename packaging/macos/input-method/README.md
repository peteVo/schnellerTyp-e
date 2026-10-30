# macOS InputMethodKit host

This is an opt-in native input method that runs the shared `CompositionSession`
engine inside Apple's InputMethodKit. It is separate from the desktop app's
existing CGEvent hook and text injector.

## Local installation

1. Build the `schnellerTyp-e-input-method` bundle for macOS.
2. Copy `schnellerTyp-e-input-method.app` to `~/Library/Input Methods/`.
3. Open **System Settings → Keyboard → Text Input → Edit**, add schnellerTyp-e,
   then select its **Automatic** input source. [Apple's guide](https://support.apple.com/en-euro/guide/mac-help/mchl84525d76/mac) describes the current Input Sources settings path.
4. If the source does not appear, log out and back in, or restart the affected
   text application.

This directory does not install or register the bundle automatically. The
bundle is a local development artifact; signing and notarization for public
distribution remain future work. Do not represent it as a signed or notarized
release.

InputMethodKit exposes marked-text APIs to compatible text clients, but support
varies by application. The host bypasses transformation while macOS secure
event input is active; that system-wide signal cannot identify which field
enabled it, and it is not a guarantee for every password field.
