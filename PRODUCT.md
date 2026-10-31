# schnellerTyp-e

A Windows-first desktop typing assistant for people who mix English, Vietnamese
and German in the same sentence. The central job is to type naturally without
changing language modes between words. The current word can change; Space
settles it. Automatic gives a valid complete Vietnamese candidate priority
over English, regardless of frequency or recent English context. German
passages retain their existing contextual rules.
Dictionary entries support Vietnamese recognition, and valid unlisted syllables
can compose when the raw spelling has no known English or German match. Some spellings
remain ambiguous, so preserving literal text and recovering quickly are part
of the product.

The engine runs offline. Personal literal words stay on the device. No typing
history, account, subscription or network service is needed. Debug tracing is
explicitly opt-in and must never be enabled in a distributed build by default.

The desktop settings window is an Operate surface. Its first screen lets a
person choose a mode and try real raw keystrokes against the actual engine.
Preferences contain Vietnamese/German rules, literal words and shortcuts. Setup
explains readiness, permissions and custom-rule recovery. Closing the settings
window keeps the tray utility running.

Windows is the release priority. Linux uses an optional Fcitx 5 adapter, with
Omarchy/Hyprland as the first runtime target. macOS has an optional InputMethodKit
input source. Native adapters must report their actual test status; compilation
does not establish application-wide compatibility.
