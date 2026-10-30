# German sharp-S word data

`GermanSharpS.inc` is generated from the `words` file in
[enz/german-wordlist](https://github.com/enz/german-wordlist), commit
`1071d87d9535b57b80ec076c699f2578a28ee9dc`.

- Source file SHA-256: `20095928ca70974fb5d86e44f10181cee16e2831b0d4b1a4ddd12c261aebc5e9`
- License file SHA-256: `a2010f343487d3f7618affe54f789f5487602331c0a8d03f49e9a7c547cf0499`
- License: CC0 1.0 Universal; see `LICENSE-CC0-1.0.txt`.

The generator normalizes entries to lowercase NFC and keeps only alphabetic
words containing `ß`. The source is a Scrabble-oriented wordlist; its README
says it is incomplete and contains mistakes. This data is used only for
presence checks on sharp-S candidates. It is not a universal German
spellchecker and does not guarantee coverage of Swiss spelling.

Rebuild with `python tools/generate-german-spelling.py`; the pinned source is
expected at `build/german-spelling-source/words`.
