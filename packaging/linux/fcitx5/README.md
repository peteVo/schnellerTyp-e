# Fcitx 5 adapter

The addon presents the shared composition session as Fcitx client preedit and
commit text. Fcitx 5 development headers and `Fcitx5Core` are required; the
official tutorial links the addon with `Fcitx5::Core`.

API reference: [Fcitx simple input method tutorial](https://www.fcitx-im.org/wiki/Develop_an_simple_input_method), and upstream headers [`inputmethodengine.h`](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx/inputmethodengine.h), [`inputcontext.h`](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx/inputcontext.h), [`inputpanel.h`](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx/inputpanel.h), [`event.h`](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx/event.h), and [`key.h`](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx-utils/key.h). The adapter uses `InputContextProperty` for per-context state, capability flags to bypass password/sensitive contexts, and `InputPanel::setClientPreedit` plus `InputContext::updatePreedit` for preedit display.
