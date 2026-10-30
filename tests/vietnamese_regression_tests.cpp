// SPDX-License-Identifier: MIT
// Focused Telex and Automatic regressions for Vietnamese modifier ambiguity.

#include "core/auto/AutoRuleEngine.hpp"
#include "core/vietnamese/VietnameseRuleEngine.hpp"

#include <cstdio>
#include <string>
#include <string_view>

namespace {

struct Case {
    const char* label;
    std::u32string_view keys;
    std::u32string_view expected;
};

std::u32string type(st::LanguageRuleEngine& engine, std::u32string_view keys)
{
    std::u32string screen;
    for (char32_t c : keys) {
        st::KeyEvent key;
        if (c == U'\b') key = st::KeyEvent::of(st::KeyKind::Backspace);
        else if (c == U' ') {
            key = st::KeyEvent::of(st::KeyKind::Space);
            key.character = U' ';
        }
        else key = st::KeyEvent::character_(c, st::unicode::isAsciiUpper(c));

        const auto result = engine.processKey(key);
        if (!result.handled) {
            if (key.kind == st::KeyKind::Backspace) {
                if (!screen.empty()) screen.pop_back();
            } else if (key.character != 0) screen.push_back(key.character);
        } else {
            if (result.backspaces < 0 || static_cast<std::size_t>(result.backspaces) > screen.size())
                return U"<invalid edit>";
            screen.resize(screen.size() - static_cast<std::size_t>(result.backspaces));
            screen += result.text;
        }
    }
    return screen;
}

template <typename Engine, std::size_t N>
int runCorpus(const char* suite, Engine& engine, const Case (&corpus)[N])
{
    int failures = 0;
    for (const auto& test : corpus) {
        engine.reset();
        const auto got = type(engine, test.keys);
        if (got != test.expected) {
            ++failures;
            std::printf("FAIL %-8s %-24s keys=\"%s\" expected=\"%s\" got=\"%s\"\n",
                        suite, test.label, st::unicode::toUtf8(test.keys).c_str(),
                        st::unicode::toUtf8(test.expected).c_str(), st::unicode::toUtf8(got).c_str());
        }
    }
    return failures;
}

const Case kVietnamese[] = {
    {"ww-undo", U"ww", U"w"}, {"uww-undo", U"uww", U"uw"},
    {"upper-WW", U"WW", U"W"}, {"upper-UWW", U"UWW", U"UW"},
    {"tw-undo", U"tw", U"tư"}, {"tww-undo", U"tww", U"tw"},
    {"thw-undo", U"thw", U"thư"}, {"thww-undo", U"thww", U"thw"},
    {"aw-undo", U"aw", U"ă"}, {"aww-undo", U"aww", U"aw"},
    {"ua-horn-late", U"nuawx", U"nữa"}, {"ua-horn-early", U"nuwax", U"nữa"},
    {"ua-tone-before-horn", U"nuaxw", U"nữa"}, {"ua-tone-after-horn", U"nuwxa", U"nữa"},
    {"mua-w-late", U"muaw", U"mưa"}, {"mua-w-early", U"muwa", U"mưa"},
    {"dua-w-late", U"duaw", U"dưa"}, {"dua-w-early", U"duwa", U"dưa"},
    {"lua-tone", U"luwra", U"lửa"}, {"sua-tone", U"suwxa", U"sữa"},
    {"chua-horn", U"chuwa", U"chưa"}, {"cuu-tone", U"cuuws", U"cứu"},
    {"huu-tone", U"huuwx", U"hữu"}, {"ruou-tone", U"ruouwj", U"rượu"},
    {"qu-onset", U"quawng", U"quăng"}, {"qu-onset-tone", U"quas", U"quá"},
    {"gia-onset", U"giuwxa", U"giữa"}, {"tone-sac", U"nuwowcs", U"nước"},
    {"tone-huyen", U"dduwowngf", U"đường"}, {"tone-hoi", U"thuyr", U"thủy"},
    {"tone-nang", U"Vieejt", U"Việt"}, {"free-tone-first", U"toasn", U"toán"},
    {"free-tone-last", U"toans", U"toán"},
    {"all-caps", U"VIEEJT", U"VIỆT"}, {"double-aa", U"aaa", U"aa"},
    {"double-tone", U"ass", U"as"}, {"backspace-final", U"nuawx\b", U"nữ"},
    {"space-boundary", U"nuawx muaw", U"nữa mưa"},
    {"sentence", U"Tooi muoons aawn nuawx.", U"Tôi muốn ăn nữa."},
    {"english-away", U"away ", U"away "}, {"english-tools", U"tools ", U"tools "},
    {"triple-w-after-undo", U"www ", U"ww "}, {"german-words", U"Fuellmenge Strasse ", U"Fuellmenge Strasse "},
    {"ww-space", U"ww ", U"w "}, {"WW-space", U"WW ", U"W "},
    {"uww-space", U"uww ", U"uw "}, {"thww-space", U"thww ", U"thw "},
    {"cww-space", U"cww ", U"cw "},
    {"aa-space", U"aa ", U"â "}, {"aaa-space", U"aaa ", U"aa "},
    {"ass-space", U"ass ", U"as "},
    {"reject-invalid", U"batf", U"batf"}, {"tone-undo-three", U"asss", U"ass"},
    {"uppercase-word", U"NUAWX", U"NỮA"},
    {"consonant-w", U"bws", U"bứ"}, {"no-tone", U"nuw", U"nư"},
};

const Case kAutomatic[] = {
    {"literal-w-keeps-neutral-context", U"ww cars hats moon ", U"w cars hats moon "},
    {"ww-undo", U"ww", U"w"}, {"uww-undo", U"uww", U"uw"},
    {"upper-WW", U"WW", U"W"}, {"tw-undo", U"tw", U"tư"},
    {"tww-undo", U"tww", U"tw"}, {"thww-undo", U"thww", U"thw"},
    {"nuawx", U"nuawx", U"nữa"}, {"nuwax", U"nuwax", U"nữa"},
    {"ua-tone-before-horn", U"nuaxw", U"nữa"}, {"ua-tone-after-horn", U"nuwxa", U"nữa"},
    {"mua-w-late", U"muaw", U"mưa"}, {"mua-w-early", U"muwa", U"mưa"},
    {"dua-w-late", U"duaw", U"dưa"}, {"dua-w-early", U"duwa", U"dưa"},
    {"lua-tone", U"luwra", U"lửa"}, {"sua-tone", U"suwxa", U"sữa"},
    {"chua-horn", U"chuwa", U"chưa"}, {"cuu-tone", U"cuuws", U"cứu"},
    {"huu-tone", U"huuwx", U"hữu"}, {"ruou-tone", U"ruouwj", U"rượu"},
    {"ww-space", U"ww ", U"w "}, {"WW-space", U"WW ", U"W "},
    {"uww-space", U"uww ", U"uw "}, {"thww-space", U"thww ", U"thw "},
    {"cww-space", U"cww ", U"cw "},
    {"nuawx-space", U"nuawx ", U"nữa "},
    {"quawng", U"quawng", U"quăng"}, {"qu-tone", U"quas", U"quá"},
    {"all-tones", U"tieengs dduwowngf thuyr", U"tiếng đường thủy"},
    {"caps", U"Vieejt Nam", U"Việt Nam"}, {"all-caps", U"VIEEJT", U"VIỆT"},
    {"sentence", U"Tooi muoons aawn nuawx.", U"Tôi muốn ăn nữa."},
    {"english-away", U"away ", U"away "}, {"english-tools", U"tools ", U"tools "},
    {"english-www", U"www ", U"www "}, {"german-context", U"Die Strasse ist schoen. ", U"Die Straße ist schön. "},
    {"mixed-context", U"The Strasse is schoen, then nuawx.", U"The Straße is schön, then nữa."},
    {"backspace", U"nuawx\b", U"nữ"}, {"space-two", U"muaw nuawx", U"mưa nữa"},
    {"tone-varied", U"quas toasn Vieejt", U"quá toán Việt"},
    {"dd", U"dduwowngf", U"đường"}, {"invalid-tone", U"batf", U"batf"},
    {"shorthand-pair", U"cww", U"cw"},
    {"word-with-space", U"muaw nuawx ", U"mưa nữa "},
    {"protected-words", U"Wasser Essen Öffnung Körper ", U"Wasser Essen Öffnung Körper "},
};

const Case kVni[] = {
    {"nua7", U"nua7", U"nưa"}, {"nu7a4", U"nu7a4", U"nữa"},
    {"nua74", U"nua74", U"nữa"}, {"nu7a3", U"nu7a3", U"nửa"},
    {"nua73", U"nua73", U"nửa"}, {"cu7u1", U"cu7u1", U"cứu"},
    {"cuu71", U"cuu71", U"cứu"}, {"huu74", U"huu74", U"hữu"},
    {"ru7o7u5", U"ru7o7u5", U"rượu"},
    {"qua8ng", U"qua8ng", U"quăng"}, {"a7-literal", U"a7", U"a7"},
    {"a8-breve", U"a8", U"ă"}, {"tone-tilde", U"a4", U"ã"},
    {"backspace", U"nu7a4\b", U"nữ"},
};

} // namespace

int main()
{
    st::VietnameseRuleEngine vietnamese;
    st::AutoRuleEngine automatic;
    st::VietnameseRuleEngine vni({st::VnInputMethod::Vni});
    const int failures = runCorpus("vi", vietnamese, kVietnamese)
                       + runCorpus("auto", automatic, kAutomatic)
                       + runCorpus("vni", vni, kVni);
    std::printf("Vietnamese regression corpus: %zu Vietnamese + %zu Automatic + %zu VNI cases; %d failure(s)\n",
                std::size(kVietnamese), std::size(kAutomatic), std::size(kVni), failures);
    return failures == 0 ? 0 : 1;
}
