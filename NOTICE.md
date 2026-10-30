# Notices

## schnellerTyp-e

Copyright the schnellerTyp-e authors. Released under the MIT licence
(SPDX-License-Identifier: MIT), as recorded in the header of every source file.

## Relationship to UniKey

UniKey (<http://unikey.org>), copyright 1998–2005 Phạm Kim Long, is licensed
under the GNU General Public License version 2.

UniKey 3.62's source was consulted as a **behavioural specification** while
writing this program: what a Vietnamese typist expects a given Telex or VNI
keystroke sequence to produce. No source code, no data tables and no derived
work from UniKey are present in this repository.

Specifically:

- the Unicode composition tables in `src/core/vietnamese/VnTables.cpp` were
  generated from Unicode NFC normalisation of base letters plus combining
  marks, not copied from UniKey's `vnconv` tables;
- the syllable model (onset / nucleus / coda), the tone-placement rules and the
  spelling check were written from Vietnamese orthography;
- the engine's design — keep the raw keystrokes, re-derive the syllable after
  every key, emit the difference — is unrelated to UniKey's packed bit-field
  state machine in `keyhook/vietkey.cpp`.

schnellerTyp-e is therefore not a derivative work of UniKey and is not subject
to the GPL. It also shares no code with UniKey's Win32 hook or its RtfIO
component.

## Third-party dependencies

Automatic-mode frequency tables in src/core/auto/FrequencyData.inc are modified,
filtered subsets of wordfreq 3.1.1 data by Robyn Speer, licensed under CC BY-SA
4.0. The subsets contain 30,000 English, 40,000 German and 10,403 Vietnamese
entries. The complete upstream attribution and source-specific conditions are
preserved in src/core/auto/data/NOTICE-wordfreq.md; the full CC BY-SA 4.0 license
is beside it. These data files remain CC BY-SA 4.0 and do not change the MIT
license of independently written application code.

Automatic-mode German sharp-S candidate validation uses a filtered word list
from [enz/german-wordlist](https://github.com/enz/german-wordlist), licensed
under CC0 1.0. The source commit, hash, scope and limitations are documented in
src/core/auto/data/NOTICE-german-spelling.md. It is used for presence checks and
does not change the MIT license of independently written application code.

The optional Linux addon dynamically uses Fcitx 5 (LGPL-2.1-or-later). The
optional macOS input source uses Apple's InputMethodKit/AppKit frameworks.

| Component | Licence | Linked how |
|---|---|---|
| Qt 6 (Core, Gui, Qml, Quick, QuickControls2, Widgets) | LGPL-3.0 or commercial | dynamically |
| libuiohook | LGPL-3.0 | dynamically |
| libX11, libXtst (Linux only) | MIT | dynamically |

Both Qt and libuiohook are LGPL. Link them dynamically — which is what the
CMake configuration does — and ship the means to relink, or hold a commercial
Qt licence. A statically linked build has obligations you should read up on
before distributing it.
