// SPDX-License-Identifier: MIT
// Auto-mode must choose attested Vietnamese spellings over English-looking raw keys.

#include "core/auto/AutoRuleEngine.hpp"
#include "core/Unicode.hpp"
#include "core/vietnamese/VnTables.hpp"

#include <array>
#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct LexiconEntry { std::string_view word; int frequency; };
#include "core/auto/FrequencyData.inc"

struct Case {
    const char* label;
    std::u32string_view keys;
    std::u32string_view expected;
    bool vni = false;
};

std::u32string type(st::AutoRuleEngine& engine, std::u32string_view keys)
{
    std::u32string screen;
    for (char32_t c : keys) {
        st::KeyEvent key;
        if (c == U' ') { key = st::KeyEvent::of(st::KeyKind::Space); key.character = U' '; }
        else key = st::KeyEvent::character_(c, st::unicode::isAsciiUpper(c));
        const auto result = engine.processKey(key);
        if (!result.handled) {
            if (key.character != 0) screen.push_back(key.character);
        } else {
            if (result.backspaces < 0 || static_cast<std::size_t>(result.backspaces) > screen.size())
                return U"<invalid edit>";
            screen.resize(screen.size() - static_cast<std::size_t>(result.backspaces));
            screen += result.text;
        }
    }
    return screen;
}

// Telex keys are written independently here; each expected value is an
// attested standalone Vietnamese lexicon syllable (including low-frequency hến).
const Case cases[] = {
    {"acute-low-frequency-fresh", U"heens", U"hến"},
    {"acute-low-frequency-after-English", U"the heens", U"the hến"},
    {"grave", U"henf", U"hèn"},
    {"dot", U"henj", U"hẹn"},
    {"hook", U"thuyr", U"thủy"},
    {"tilde", U"suwxa", U"sữa"},
    {"thuở-telex", U"thuowr", U"thuở"},
    {"thuở-vni", U"thuo73", U"thuở", true},
    {"huơ", U"huow", U"huơ"},
    {"hươu", U"huowu", U"hươu"},
    {"uppercase", U"HEENS", U"HẾN"},
    {"title-case-after-English", U"The Heens", U"The Hến"},
    {"space-commit", U"heens ", U"hến "},
    {"English-axis", U"axis ", U"ái "},
    {"English-beef", U"beef ", U"bề "},
    {"English-cars", U"cars ", U"cá "},
    {"English-cheers", U"cheers ", U"chế "},
    {"English-hats", U"hats ", U"hát "},
    {"English-hurts", U"hurts ", U"hút "},
    {"English-moon", U"moon ", U"môn "},
    {"English-teen", U"teen ", U"tên "},
};

struct Syllable { std::u32string text; std::vector<st::vn::Letter> letters; st::vn::Tone tone; };

bool parseSyllable(std::u32string_view word, Syllable& syllable)
{
    syllable = {};
    for (char32_t c : word) {
        if (c >= U'a' && c <= U'z') {
            syllable.letters.push_back({c, st::vn::Mark::None, false});
            continue;
        }
        const auto decoded = st::vn::decompose(c);
        if (!decoded || (syllable.tone != st::vn::Tone::None
                         && decoded->tone != st::vn::Tone::None))
            return false;
        syllable.letters.push_back(decoded->letter);
        if (decoded->tone != st::vn::Tone::None)
            syllable.tone = decoded->tone;
    }
    if (syllable.letters.empty()
        || !st::vn::isValidSyllable(syllable.letters, syllable.tone, false))
        return false;
    syllable.text = st::vn::render(syllable.letters, syllable.tone, false);
    return syllable.text == word;
}

std::u32string encode(const Syllable& syllable, bool vni)
{
    std::u32string keys;
    for (const auto& letter : syllable.letters) {
        const char32_t base = letter.base;
        keys.push_back(base);
        switch (letter.mark) {
        case st::vn::Mark::None: break;
        case st::vn::Mark::Circumflex: keys.push_back(vni ? U'6' : base); break;
        case st::vn::Mark::Breve: keys.push_back(vni ? U'8' : U'w'); break;
        case st::vn::Mark::Horn: keys.push_back(vni ? U'7' : U'w'); break;
        case st::vn::Mark::Stroke: keys.push_back(vni ? U'9' : U'd'); break;
        }
    }
    if (syllable.tone != st::vn::Tone::None) {
        constexpr std::array<char32_t, 6> telex{0, U's', U'f', U'r', U'x', U'j'};
        constexpr std::array<char32_t, 6> vniKeys{0, U'1', U'2', U'3', U'4', U'5'};
        const auto tone = static_cast<std::size_t>(syllable.tone);
        keys.push_back(vni ? vniKeys[tone] : telex[tone]);
    }
    return keys;
}

int roundTrip(st::AutoRuleEngine& engine, std::u32string_view keys,
              std::u32string_view expected, std::u32string_view prefix,
              const char* method, std::string_view sourceWord,
              std::size_t& checks, std::size_t& failures)
{
    engine.reset();
    const auto input = std::u32string(prefix) + std::u32string(keys);
    const auto wanted = std::u32string(prefix) + std::u32string(expected);
    const auto got = type(engine, input);
    ++checks;
    if (got == wanted)
        return 0;
    ++failures;
    if (failures <= 30)
        std::printf("FAIL lexicon-%s word=\"%.*s\" keys=\"%s\" expected=\"%s\" got=\"%s\"\n",
                    method, static_cast<int>(sourceWord.size()), sourceWord.data(),
                    st::unicode::toUtf8(keys).c_str(), st::unicode::toUtf8(wanted).c_str(),
                    st::unicode::toUtf8(got).c_str());
    return 1;
}

std::size_t runLexiconRoundTrips(st::AutoRuleEngine& engine)
{
    std::size_t syllables = 0, checks = 0, failures = 0;
    for (const auto& entry : kVietnameseWords) {
        auto word = st::unicode::fromUtf8(entry.word);
        Syllable syllable;
        if (!parseSyllable(word, syllable))
            continue;
        const bool hasVietnameseMark = syllable.tone != st::vn::Tone::None
            || std::any_of(syllable.letters.begin(), syllable.letters.end(), [](const auto& letter) {
                   return letter.mark != st::vn::Mark::None;
               });
        // These are valid Vietnamese spellings whose diacritics exercise the
        // Auto-mode priority under test; plain foreign entries like "moon"
        // otherwise produce false failures from a dictionary shared with other languages.
        if (!hasVietnameseMark)
            continue;
        // The source lexicon contains this typo; preserve the exception here
        // rather than weakening coverage for other validator-approved syllables.
        if (word == U"đuợc")
            continue;
        ++syllables;
        for (const bool vni : {false, true}) {
            const auto keys = encode(syllable, vni);
            const char* method = vni ? "VNI" : "Telex";
            if (vni)
                engine.applyOptions({{"method", "vni"}});
            else
                engine.applyOptions({{"method", "telex"}});
            roundTrip(engine, keys, syllable.text, U"", method, entry.word, checks, failures);
            roundTrip(engine, keys, syllable.text, U"Hello ", method, entry.word, checks, failures);
            roundTrip(engine, keys + U" ", syllable.text + U" ", U"", method, entry.word, checks, failures);
            roundTrip(engine, keys + U" ", syllable.text + U" ", U"Hello ", method, entry.word, checks, failures);
        }
    }
    std::printf("Vietnamese lexicon round trips: %zu syllables, %zu checks, %zu failure(s)\n",
                syllables, checks, failures);
    return failures;
}
}

int main()
{
    st::AutoRuleEngine engine;
    int failures = 0;
    for (const auto& test : cases) {
        engine.reset();
        engine.applyOptions({{"method", test.vni ? "vni" : "telex"}});
        const auto got = type(engine, test.keys);
        if (got != test.expected) {
            ++failures;
            std::printf("FAIL %-32s keys=\"%s\" expected=\"%s\" got=\"%s\"\n",
                        test.label, st::unicode::toUtf8(test.keys).c_str(),
                        st::unicode::toUtf8(test.expected).c_str(), st::unicode::toUtf8(got).c_str());
        }
    }
    failures += static_cast<int>(runLexiconRoundTrips(engine));
    std::printf("Vietnamese Auto priority: %zu focused cases; %d total failure(s)\n",
                std::size(cases), failures);
    return failures == 0 ? 0 : 1;
}
