// SPDX-License-Identifier: MIT
// Focused word-level corpus for Automatic mode. Build as a separate test target.

#include "core/auto/AutoRuleEngine.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {

int gInvalidEdits = 0;

struct Case {
    const char* label;
    std::u32string_view keys;
    std::u32string_view expected;
    const char* method = "telex";
};

std::u32string type(st::AutoRuleEngine& engine, std::u32string_view keys)
{
    std::u32string screen;
    for (char32_t c : keys) {
        st::KeyEvent key;
        if (c == U'\b')
            key = st::KeyEvent::of(st::KeyKind::Backspace);
        else if (c == U'\x1b')
            key = st::KeyEvent::of(st::KeyKind::Escape);
        else if (c == U'\n')
            key = st::KeyEvent::of(st::KeyKind::Enter);
        else if (c == U'\t')
            key = st::KeyEvent::of(st::KeyKind::Tab);
        else if (c == U'\x7f')
            key = st::KeyEvent::of(st::KeyKind::Navigation);
        else if (c == U' ')
            key = st::KeyEvent::of(st::KeyKind::Space);
        else
            key = st::KeyEvent::character_(c, st::unicode::isAsciiUpper(c));

        const auto result = engine.processKey(key);
        if (result.handled) {
            if (result.backspaces < 0
                || static_cast<std::size_t>(result.backspaces) > screen.size()) {
                ++gInvalidEdits;
                continue;
            }
            const auto count = static_cast<std::size_t>(result.backspaces);
            screen.resize(screen.size() - count);
            screen += result.text;
        } else if (key.kind == st::KeyKind::Backspace) {
            if (!screen.empty())
                screen.pop_back();
        } else if (key.kind == st::KeyKind::Character) {
            screen.push_back(key.character);
        } else if (key.kind == st::KeyKind::Space) {
            screen.push_back(U' ');
        }
    }
    return screen;
}

const Case kCorpus[] = {
    // German substitutions, capitalization, inflection and context.
    {"de/strasse", U"Strasse ", U"Straße "},
    {"de/strassen", U"Strassen ", U"Straßen "},
    {"de/gruesse", U"Gruesse ", U"Grüße "},
    {"de/heisst", U"heisst ", U"heißt "},
    {"de/weiss", U"weiss ", U"weiß "},
    {"de/fuss", U"Fuss ", U"Fuß "},
    {"de/groesse", U"Groesse ", U"Größe "},
    {"de/sentence", U"Die Strasse ist schoen. ", U"Die Straße ist schön. "},
    {"de/context-through-homograph", U"Die Strasse ist in der Stadt. ",
     U"Die Straße ist in der Stadt. "},
    {"vi/after-German", U"Die Strasse heens ", U"Die Straße hến "},
    {"de/plurals", U"Strassen und Fuesse ", U"Straßen und Füße "},
    {"de/umlaut-oel", U"Oel ", U"Öl "},
    {"de/umlaut-ueber", U"Ueber ", U"Über "},
    {"de/exception", U"Wasser Duell Dauer ", U"Wasser Duell Dauer "},
    {"de/aepfel", U"Aepfel ", U"Äpfel "},
    {"de/aeusserung", U"Aeusserung ", U"Äußerung "},
    {"de/oeffnung", U"Oeffnung ", U"Öffnung "},
    {"de/uebung", U"Uebung ", U"Übung "},
    {"de/ueberraschung", U"Ueberraschung ", U"Überraschung "},
    {"de/fuellmenge", U"Fuellmenge ", U"Füllmenge "},
    {"de/aendern", U"aendern ", U"ändern "},
    {"de/haeuser", U"Haeuser ", U"Häuser "},
    {"de/maedchen", U"Maedchen ", U"Mädchen "},
    {"de/koerper", U"Koerper ", U"Körper "},
    {"de/koennen", U"koennen ", U"können "},
    {"de/muessen", U"muessen ", U"müssen "},
    {"de/duerfen", U"duerfen ", U"dürfen "},
    {"de/schoenheit", U"Schoenheit ", U"Schönheit "},
    {"de/frueh", U"frueh ", U"früh "},
    {"de/gruen", U"gruen ", U"grün "},
    {"de/glueck", U"Glueck ", U"Glück "},
    {"de/muede", U"muede ", U"müde "},
    {"de/fuehlen", U"fuehlen ", U"fühlen "},
    {"de/gehoeren", U"gehoeren ", U"gehören "},
    {"de/aussen", U"aussen ", U"außen "},
    {"de/ausser", U"ausser ", U"außer "},
    {"de/uebersicht", U"Uebersicht ", U"Übersicht "},
    {"de/uebersetzen", U"uebersetzen ", U"übersetzen "},
    {"de/kaelte", U"Kaelte ", U"Kälte "},
    {"de/staerker", U"staerker ", U"stärker "},
    {"de/waehrend", U"waehrend ", U"während "},
    {"de/zufaellig", U"zufaellig ", U"zufällig "},
    {"de/loesung", U"Loesung ", U"Lösung "},
    {"de/moeglich", U"moeglich ", U"möglich "},
    {"de/noetig", U"noetig ", U"nötig "},
    {"de/haeufig", U"haeufig ", U"häufig "},
    {"de/waende", U"Waende ", U"Wände "},
    {"de/baeume", U"Baeume ", U"Bäume "},
    {"de/traeume", U"Traeume ", U"Träume "},
    {"de/voegel", U"Voegel ", U"Vögel "},
    {"de/hoeren", U"Hoeren ", U"Hören "},
    {"de/kuessen", U"kuessen ", U"küssen "},
    {"de/suess", U"suess ", U"süß "},
    {"de/fuesse", U"Fuesse ", U"Füße "},
    {"de/strassenbahn", U"Strassenbahn ", U"Straßenbahn "},
    {"de/grossstadt", U"Grossstadt ", U"Großstadt "},
    {"de/house-sentence", U"Die Haeuser stehen an der Strasse. ",
     U"Die Häuser stehen an der Straße. "},
    {"de/bridge-sentence", U"Ueber den Fluss fuehrt eine Bruecke. ",
     U"Über den Fluss führt eine Brücke. "},
    {"de/compound-sentence", U"Die Strassenbahn faehrt durch die Grossstadt. ",
     U"Die Straßenbahn fährt durch die Großstadt. "},
    {"de/exception-sentence", U"Wasser und Dauer sind keine Duelle. ",
     U"Wasser und Dauer sind keine Duelle. "},

    // Vietnamese conversions take priority over English, including here.
    {"en/street", U"street ", U"street "},
    {"en/address", U"address ", U"address "},
    {"en/stress", U"stress ", U"stress "},
    {"en/class", U"class ", U"class "},
    {"en/glass", U"glass ", U"glass "},
    {"en/press", U"press ", U"press "},
    {"en/coffee", U"coffee ", U"coffee "},
    {"en/queue", U"queue ", U"queue "},
    {"en/flower", U"flower ", U"flower "},
    {"en/gross-context", U"The gross total caused a fuss. ",
     U"The gross total caused a fuss. "},
    {"en/name", U"New York ", U"New York "},
    {"en/english-sentence", U"The street address is across the class. ",
     U"The street address í across the class. "},
    {"en/weiss-name-context", U"Dr Weiss works in Berlin. ", U"Dr Weiss works in Berlin. "},
    {"en/gross-fuss-context", U"The gross error caused a fuss. ",
     U"The gross error caused a fuss. "},
    {"en/website", U"Open the website and click the link. ",
     U"Open the website and click the link. "},
    {"en/software", U"Install the software update today. ",
     U"Install the software update today. "},
    {"en/keyboard", U"The keyboard shortcut opens settings. ",
     U"The keyboard shortcut opens settings. "},
    {"en/variable", U"userName = firstName + lastName ",
     U"userName = firstName + lastName "},
    {"en/function", U"function getUserName() { return value; }",
     U"function getUserName() { return value; }"},
    {"en/package", U"npm install schnellertyp-e ", U"npm install schnellertyp-e "},
    {"en/file-name", U"README.md package.json ", U"README.md package.json "},
    {"en/url-query", U"https://example.com/search?q=street ",
     U"https://example.com/search?q=street "},
    {"en/email-address", U"first.last@example.com ", U"first.last@example.com "},
    {"en/ipv4", U"192.168.1.10 ", U"192.168.1.10 "},
    {"en/identifier", U"strasse_count user_id ", U"strasse_count user_id "},
    {"en/acronym", U"NASA API URL HTTP ", U"NASA API URL HTTP "},
    {"en/company", U"Microsoft Google Mozilla ", U"Microsoft Google Mozilla "},
    {"en/person", U"Robert Smith Alice Johnson ", U"Robert Smith Alice Johnson "},
    {"en/city", U"London Boston New York ", U"London Boston New York "},
    {"en/product", U"Windows Firefox Visual Studio ", U"Windows Firefox Visual Studio "},
    {"en/contractions", U"don't we're they've can't ", U"don't we're they've can't "},
    {"en/possessive", U"James's book is here. ", U"James's book í here. "},
    {"en/numbered-model", U"Model X100 costs $250. ", U"Model X100 costs $250. "},
    {"en/quotes", U"She said, \"the class is full\". ",
     U"She said, \"the class í full\". "},
    {"en/long-sentence", U"Please press the glass button near the street. ",
     U"Please press the glass button near the street. "},
    {"en/two-sentences", U"The address is correct. The coffee is ready. ",
     U"The address í correct. The coffee í ready. "},
    {"en/language-switch", U"I like coffee und Berlin is lovely. ",
     U"I like coffee und Berlin í lovely. "},
    {"en/german-name", U"Mr Weiss moved to Berlin. ", U"Mr Weiss moved to Berlin. "},
    {"en/german-city", U"Visit Essen in Germany. ", U"Visit Essen in Germany. "},
    {"en/command-line", U"git status --short ", U"git status --short "},
    {"en/version", U"version 2.4.1 is available ", U"version 2.4.1 í available "},
    {"en/units", U"Add 5 mm to the 10 cm board. ", U"Add 5 mm to the 10 cm board. "},
    {"en/domain", U"example.org docs.example.com ", U"example.org docs.example.com "},
    {"en/filename-path", U"C:\\Program Files\\MyApp\\readme.txt ",
     U"C:\\Program Files\\MyApp\\readme.txt "},
    {"en/mixed-case", U"iPhone eBay YouTube ", U"iPhone eBay YouTube "},
    {"en/plural-forms", U"classes glasses addresses ", U"classes glasses addresses "},
    {"en/past-forms", U"pressed stressed addressed ", U"pressed stressed addressed "},
    {"en/common-prose", U"There were several people in the room. ",
     U"Thể were several people in the room. "},
    {"en/question", U"Where is the nearest coffee shop? ",
     U"Where í the nearest coffee shop? "},
    {"en/exclamation", U"That flower looks beautiful! ",
     U"That flower looks beautiful! "},
    {"en/mixed-name", U"Robert moved to Berlin last year. ",
     U"Robert moved to Berlin lát year. "},
    {"en/street-number", U"Meet me at 221B Baker Street. ",
     U"Meet me at 221B Baker Street. "},
    {"en/contact", U"Call +1-555-0100 after lunch. ",
     U"Call +1-555-0100 after lunch. "},
    {"en/search-terms", U"stress class glass press ", U"stress class glass press "},
    {"en/coffees", U"Coffee, tea, and flowers. ", U"Coffee, tea, and flowers. "},
    {"en/technical-sentence", U"The address field accepts a URL or email. ",
     U"The address field accepts a URL or email. "},
    {"en/english-plural", U"The streets and classes are busy. ",
     U"The streets and classes are busy. "},
    {"en/english-possessive", U"The glass's edge is sharp. ",
     U"The glass's edge í sharp. "},
    {"en/english-ellipsis", U"First, press save; then close the window. ",
     U"First, press save; then close the window. "},

    // Vietnamese Telex and VNI mixed with German and English.
    {"vi/telex", U"hoom nay tooi ddeens Berlin ", U"hôm nay tôi đến Berlin "},
    {"vi/vni", U"ho6m nay to6i d9e6n1 Berlin ", U"hôm nay tôi đến Berlin ", "vni"},
    {"vi/telex-toi", U"tooi ", U"tôi "},
    {"vi/telex-yeu", U"yeeu ", U"yêu "},
    {"vi/telex-tieng", U"tieengs ", U"tiếng "},
    {"vi/telex-viet", U"Vieejt ", U"Việt "},
    {"vi/telex-nuoc", U"nuowcs ", U"nước "},
    {"vi/telex-nguoi", U"nguowif ", U"người "},
    {"vi/telex-duong", U"dduwowngf ", U"đường "},
    {"vi/telex-hoa", U"hoaf ", U"hòa "},
    {"vi/telex-qua", U"quaf ", U"quà "},
    {"vi/telex-qua-s", U"quas ", U"quá "},
    {"vi/telex-giua", U"giuwxa ", U"giữa "},
    {"vi/telex-toan", U"toans ", U"toán "},
    {"vi/telex-cuoi", U"cuoosi ", U"cuối "},
    {"vi/telex-hoang", U"hoangf ", U"hoàng "},
    {"vi/telex-nhieu", U"nhieeuf ", U"nhiều "},
    {"vi/telex-nha", U"nhaf ", U"nhà "},
    {"vi/telex-ban", U"banj ", U"bạn "},
    {"vi/telex-ngay", U"ngafy ", U"ngày "},
    {"vi/telex-thang", U"thangs ", U"tháng "},
    {"vi/telex-viet-nam", U"Vieejt Nam ", U"Việt Nam "},
    {"vi/telex-sentence-love", U"tooi yeeu Vieejt Nam ",
     U"tôi yêu Việt Nam "},
    {"vi/telex-sentence-home", U"tooi ddeens nhaf ", U"tôi đến nhà "},
    {"vi/telex-sentence-people", U"nguowif Vieejt ", U"người Việt "},
    {"vi/telex-sentence-water", U"nuowcs vaf nguyeen ", U"nước và nguyên "},
    {"vi/vni-toi", U"to6i ", U"tôi ", "vni"},
    {"vi/vni-yeu", U"ye6u ", U"yêu ", "vni"},
    {"vi/vni-tieng", U"tie6ng1 ", U"tiếng ", "vni"},
    {"vi/vni-viet", U"Vie6t5 ", U"Việt ", "vni"},
    {"vi/vni-nuoc", U"nu7o7c1 ", U"nước ", "vni"},
    {"vi/vni-nguoi", U"ngu7o7i2 ", U"người ", "vni"},
    {"vi/vni-duong", U"d9u7o7ng2 ", U"đường ", "vni"},
    {"vi/vni-hoa", U"ho2a ", U"hòa ", "vni"},
    {"vi/vni-qua", U"qua2 ", U"quà ", "vni"},
    {"vi/vni-qua-acute", U"qua1 ", U"quá ", "vni"},
    {"vi/vni-giua", U"giu7a4 ", U"giữa ", "vni"},
    {"vi/vni-toan", U"toa1n ", U"toán ", "vni"},
    {"vi/vni-cuoi", U"cuo61i ", U"cuối ", "vni"},
    {"vi/vni-nha", U"nha2 ", U"nhà ", "vni"},
    {"vi/vni-ban", U"ban5 ", U"bạn ", "vni"},
    {"vi/vni-ngay", U"nga2y ", U"ngày ", "vni"},
    {"vi/vni-vietnam", U"to6i ye6u Vie6t5 Nam ",
     U"tôi yêu Việt Nam ", "vni"},
    {"vi/vni-sentence-home", U"to6i d9e6n1 nha2 ",
     U"tôi đến nhà ", "vni"},
    {"mixed/three-languages", U"tooi moechte coffee ", U"tôi möchte coffee "},
    {"mixed/two-languages", U"Ich moechte tooi ", U"Ich möchte tôi "},

    // Delimiters, addresses, editing and per-word commit behavior.
    {"literal/email", U"hoom@tooi.com ", U"hoom@tooi.com "},
    {"literal/path", U"C:\\users\\tooi\\hoom ", U"C:\\users\\tooi\\hoom "},
    {"literal/punctuation", U"tooi,New York!", U"tôi,New York!"},
    {"edit/backspace", U"tooi\b ", U"tô "},
    {"edit/escape", U"tooi\x1b ", U"tooi "},
    {"edit/escape-word-only", U"tooi\x1b tooi ", U"tooi tôi "},
    {"edit/backspace-sharp-s", U"Strasse\b ", U"Straß "},
    {"edit/backspace-umlaut", U"schoen\b ", U"schö "},
    {"edit/backspace-vietnamese", U"tooi\b ", U"tô "},
    {"boundary/enter", U"Strasse\n", U"Straße"},
    {"boundary/tab", U"Strasse\t", U"Straße"},
    {"boundary/navigation", U"tooi\x7f tooi ", U"tôi tôi "},
    {"boundary/commit-prefix", U"moechte Strasse tooi ", U"möchte Straße tôi "},
    {"de/all-caps", U"STRASSE Hoom tooi ", U"STRASSE Hôm tôi "},
    {"context/commit-words", U"moechte Strasse ", U"möchte Straße "},
    {"de/live-strasse", U"Strasse", U"Straße"},
    {"de/no-invalid-sharp-s-name", U"Deutschland Borussia Grossman ",
     U"Deutschland Borussia Grossman "},
    {"en/rare-inflections", U"This is cans bans carts choirs tarts overpass ",
     U"Thí í cán bán cát chói tát overpass "},
};

// These inputs are intentionally reported without asserting one language's
// interpretation: short words such as "weiss" can be valid in multiple languages.
const std::u32string_view kAmbiguous[] = {
    U"weiss", U"Weiss", U"gross", U"fuss", U"Masse", U"Gift", U"wer"
};

} // namespace

int main()
{
    st::AutoRuleEngine engine;
    int failures = 0;
    for (const auto& test : kCorpus) {
        engine.reset();
        engine.applyOptions({{"method", test.method}});
        const auto got = type(engine, test.keys);
        if (got != test.expected) {
            ++failures;
            std::printf("FAIL %-26s expected=\"%s\" got=\"%s\"\n", test.label,
                        st::unicode::toUtf8(test.expected).c_str(),
                        st::unicode::toUtf8(got).c_str());
        }
    }

    std::printf("Ambiguous cases (observed only):\n");
    for (const auto input : kAmbiguous) {
        engine.reset();
        const auto got = type(engine, std::u32string(input) + U" ");
        std::printf("  %s -> %s\n", st::unicode::toUtf8(input).c_str(),
                    st::unicode::toUtf8(got).c_str());
    }
    if (gInvalidEdits != 0) {
        std::printf("FAIL %d invalid edit(s): backspace count was negative or exceeded screen length\n",
                    gInvalidEdits);
        failures += gInvalidEdits;
    }

    engine.applyOptions({{"method", "telex"}, {"sharpS", "false"}});
    if (type(engine, U"Strasse ") != U"Strasse ") {
        std::puts("FAIL options/sharp-s-off expected literal Strasse");
        ++failures;
    }
    engine.applyOptions({{"method", "telex"}, {"umlauts", "false"}});
    if (type(engine, U"Aepfel ") != U"Aepfel ") {
        std::puts("FAIL options/umlauts-off expected literal Aepfel");
        ++failures;
    }
    engine.applyOptions({{"method", "telex"}, {"sharpS", "true"}, {"umlauts", "true"},
                         {"capitalSharpS", "true"}});
    auto got = type(engine, U"STRASSE ");
    if (got != U"STRAẞE ") {
        std::printf("FAIL options/capital-sharp-s expected STRAẞE got %s\n",
                    st::unicode::toUtf8(got).c_str());
        ++failures;
    }
    engine.applyOptions({{"method", "telex"}, {"sharpS", "true"}, {"umlauts", "true"},
                         {"capitalSharpS", "false"}, {"literalWords", "STRASSE\nHOOM"}});
    got = type(engine, U"STRASSE Hoom tooi ");
    if (got != U"STRASSE Hoom tôi ") {
        std::printf("FAIL options/personal-literals expected STRASSE Hoom tôi got %s\n",
                    st::unicode::toUtf8(got).c_str());
        ++failures;
    }
    engine.setGermanExceptions({{{"kaese"}, {}, {}, {}}});
    if (type(engine, U"Kaese ") != U"Kaese ") {
        std::puts("FAIL options/custom-german-exception expected Kaese");
        ++failures;
    }

    std::printf("\n%d corpus cases plus 5 option cases, %d failure(s)\n",
                static_cast<int>(sizeof(kCorpus) / sizeof(kCorpus[0])), failures);
    return failures == 0 ? 0 : 1;
}
