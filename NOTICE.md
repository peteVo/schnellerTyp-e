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

| Component | Licence | Linked how |
|---|---|---|
| Qt 6 (Core, Gui, Qml, Quick, QuickControls2, Widgets) | LGPL-3.0 or commercial | dynamically |
| libuiohook | LGPL-3.0 | dynamically |
| libX11, libXtst (Linux only) | MIT | dynamically |

Both Qt and libuiohook are LGPL. Link them dynamically — which is what the
CMake configuration does — and ship the means to relink, or hold a commercial
Qt licence. A statically linked build has obligations you should read up on
before distributing it.
