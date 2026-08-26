// SPDX-License-Identifier: MIT
//
// Engine tests. No Qt, no hook, no display server: the core is a pure state
// machine, so the whole rule set can be exercised in-process.
//
//   cmake -DSCHNELLERTYPE_BUILD_TESTS=ON .. && ctest --output-on-failure
//
// The simulator below models what the OS actually does with an EngineResult —
// swallow the key and apply (backspaces, text), or let the key through — so a
// passing test means the visible text is right, not merely that the engine
// returned some plausible struct.

#include "core/RuleEngineRegistry.hpp"
#include "core/custom/RuleSetEngine.hpp"
#include "core/german/GermanRuleEngine.hpp"
#include "core/vietnamese/VietnameseRuleEngine.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int gFailures = 0;
int gChecks   = 0;

/// Feed a keystroke sequence and return what would be on screen.
/// '\b' is Backspace, ' ' is the space bar, everything else is a character key.
std::u32string typeKeys(st::LanguageRuleEngine& engine, std::u32string_view keys)
{
    std::u32string screen;
    for (char32_t c : keys) {
        st::KeyEvent ev;
        if (c == U'\b') {
            ev = st::KeyEvent::of(st::KeyKind::Backspace);
        } else if (c == U' ') {
            ev           = st::KeyEvent::of(st::KeyKind::Space);
            ev.character = U' ';
        } else {
            ev = st::KeyEvent::character_(c, st::unicode::isAsciiUpper(c));
        }

        const st::EngineResult r = engine.processKey(ev);
        if (!r.handled) {
            if (ev.kind == st::KeyKind::Backspace) {
                if (!screen.empty())
                    screen.pop_back();
            } else if (ev.character != 0) {
                screen.push_back(ev.character);
            }
        } else {
            for (int i = 0; i < r.backspaces && !screen.empty(); ++i)
                screen.pop_back();
            screen += r.text;
        }
    }
    return screen;
}

void check(const char* label, std::u32string_view keys, std::u32string_view expected,
           st::LanguageRuleEngine& engine)
{
    engine.reset();
    ++gChecks;
    const std::u32string got = typeKeys(engine, keys);
    if (got != expected) {
        ++gFailures;
        std::printf("FAIL  %-28s keys=\"%s\"  expected=\"%s\"  got=\"%s\"\n", label,
                    st::unicode::toUtf8(keys).c_str(), st::unicode::toUtf8(expected).c_str(),
                    st::unicode::toUtf8(got).c_str());
    }
}

void checkTrue(const char* label, bool condition)
{
    ++gChecks;
    if (!condition) {
        ++gFailures;
        std::printf("FAIL  %-28s condition was false\n", label);
    }
}

// ---------------------------------------------------------------------------

void testGerman()
{
    st::GermanRuleEngine de;

    check("de/ae", U"aepfel", U"äpfel", de);
    check("de/Ae", U"Aepfel", U"Äpfel", de);
    check("de/AE", U"AEPFEL", U"ÄPFEL", de);
    check("de/oe", U"oel", U"öl", de);
    check("de/ue", U"ueber", U"über", de);
    check("de/Ue", U"Ueber", U"Über", de);
    check("de/ss", U"strasse", U"straße", de);
    check("de/mixed", U"Fuellmenge", U"Füllmenge", de);
    check("de/multi", U"Aussenmoebel", U"Außenmöbel", de);

    // Exception dictionary, including the self-correcting behaviour.
    check("de/exception-dauer", U"dauer", U"dauer", de);
    check("de/exception-duell", U"duell", U"duell", de);
    check("de/diverge-duester", U"duester", U"düster", de);
    check("de/exception-wasser", U"wasser", U"wasser", de);
    check("de/exception-muessen", U"muessen", U"müssen", de);
    check("de/exception-aussehen", U"aussehen", U"aussehen", de);
    check("de/diverge-aussen", U"aussen", U"außen", de);
    check("de/exception-neue", U"neue", U"neue", de);

    // Double-key undo and escape key.
    check("de/undo-ue", U"mueede", U"muede", de);
    check("de/undo-keeps-pinned", U"mueedee", U"muedee", de);
    check("de/undo-not-needed", U"muede", U"müde", de);
    check("de/undo-ss", U"ssss", U"sss", de);
    check("de/escape", U"mu\\esli", U"muesli", de);

    // Word boundaries and editing.
    check("de/two-words", U"aepfel oel", U"äpfel öl", de);
    check("de/backspace", U"ueber\b\b", U"üb", de);
    check("de/backspace-digraph", U"ue\b", U"", de);

    // Options.
    st::GermanRuleEngine noSharp({/*umlauts*/ true, /*sharpS*/ false});
    check("de/sharpS-off", U"strasse", U"strasse", noSharp);

    st::GermanRuleEngine noDict;
    st::EngineOptions    opt = noDict.options();
    opt["useExceptions"]     = "false";
    noDict.applyOptions(opt);
    check("de/dict-off", U"dauer", U"daür", noDict);
}

// ---------------------------------------------------------------------------

void testVietnameseTelex()
{
    st::VietnameseRuleEngine vi;

    // Marks
    check("vi/aa", U"aa", U"â", vi);
    check("vi/aw", U"aw", U"ă", vi);
    check("vi/ee", U"ee", U"ê", vi);
    check("vi/oo", U"oo", U"ô", vi);
    check("vi/ow", U"ow", U"ơ", vi);
    check("vi/uw", U"uw", U"ư", vi);
    check("vi/dd", U"dd", U"đ", vi);
    check("vi/w-alone", U"w", U"ư", vi);

    // Tones on a simple syllable
    check("vi/as", U"as", U"á", vi);
    check("vi/af", U"af", U"à", vi);
    check("vi/ar", U"ar", U"ả", vi);
    check("vi/ax", U"ax", U"ã", vi);
    check("vi/aj", U"aj", U"ạ", vi);
    check("vi/remove-tone", U"asz", U"a", vi);

    // Words
    check("vi/tieng", U"tieengs", U"tiếng", vi);
    check("vi/viet", U"Vieejt", U"Việt", vi);
    check("vi/nuoc", U"nuwowcs", U"nước", vi);
    check("vi/nuoc-free", U"nuocws", U"nước", vi);
    check("vi/duong", U"dduwowngf", U"đường", vi);
    check("vi/nguoi", U"nguowif", U"người", vi);
    check("vi/quaf", U"quaf", U"quà", vi);
    check("vi/quas", U"quas", U"quá", vi);
    check("vi/gif", U"gif", U"gì", vi);
    check("vi/gius", U"giuwxa", U"giữa", vi);
    check("vi/hoaf-classic", U"hoaf", U"hòa", vi);
    check("vi/thuyr", U"thuyr", U"thủy", vi);
    check("vi/toans", U"toans", U"toán", vi);
    check("vi/toasn", U"toasn", U"toán", vi);
    check("vi/cuoins", U"cuoosi", U"cuối", vi);
    check("vi/hoafng", U"hoangf", U"hoàng", vi);
    check("vi/nhieeuf", U"nhieeuf", U"nhiều", vi);
    check("vi/khuyr", U"khuyru", U"khuỷu", vi);

    // Sentence with word boundaries
    check("vi/sentence", U"tooi yeeu Vieejt Nam", U"tôi yêu Việt Nam", vi);

    // Double-key undo
    check("vi/undo-tone", U"ass", U"as", vi);
    check("vi/undo-tone-3", U"asss", U"ass", vi);
    check("vi/undo-aa", U"aaa", U"aa", vi);
    check("vi/undo-aa-4", U"aaaa", U"aaa", vi);
    check("vi/undo-w", U"uww", U"uw", vi);

    // Spelling check refuses an impossible diacritic and types the key.
    check("vi/spell-reject-tone", U"batf", U"batf", vi);
    check("vi/mark-literal", U"kew", U"kew", vi);
    check("vi/w-after-consonant", U"bws", U"bứ", vi);

    // Auto-restore: an English word typed in Telex comes back intact.
    check("vi/restore-away", U"away ", U"away ", vi);
    check("vi/restore-tools", U"tools ", U"tools ", vi);
    check("vi/keep-valid", U"toans ", U"toán ", vi);

    // Modern tone placement
    st::VietnameseRuleEngine modern({st::VnInputMethod::Telex, /*modernTone=*/true});
    check("vi/hoaf-modern", U"hoaf", U"hoà", modern);
    check("vi/thuyr-modern", U"thuyr", U"thuỷ", modern);
}

void testVietnameseVni()
{
    st::VietnameseRuleEngine vi({st::VnInputMethod::Vni});

    check("vni/a6", U"a6", U"â", vi);
    check("vni/a8", U"a8", U"ă", vi);
    check("vni/o7", U"o7", U"ơ", vi);
    check("vni/u7", U"u7", U"ư", vi);
    check("vni/d9", U"d9", U"đ", vi);
    check("vni/tone1", U"a1", U"á", vi);
    check("vni/tone5", U"a5", U"ạ", vi);
    check("vni/tieng", U"tie6ng1", U"tiếng", vi);
    check("vni/viet", U"Vie6t5", U"Việt", vi);
    check("vni/nuoc", U"nu7o7c1", U"nước", vi);
    check("vni/duong", U"d9u7o7ng2", U"đường", vi);
    check("vni/remove", U"a1 0", U"á 0", vi);
}

// ---------------------------------------------------------------------------

void testRuleSet()
{
    st::RuleSetDefinition def;
    def.id          = "custom:test";
    def.displayName = "Test";
    def.badge       = "TS";
    def.rules       = {
        {st::unicode::fromUtf8("e'"), st::unicode::fromUtf8("é"), false, false, false},
        {st::unicode::fromUtf8("a`"), st::unicode::fromUtf8("à"), false, false, false},
        {st::unicode::fromUtf8("btw"), st::unicode::fromUtf8("by the way"), false, true, false},
    };
    st::RuleSetEngine engine(def);

    check("rs/accent", U"cafe'", U"café", engine);
    check("rs/grave", U"a`", U"à", engine);
    check("rs/expansion", U"btw", U"by the way", engine);
    check("rs/smartcase", U"Btw", U"By the way", engine);
    check("rs/undo", U"e''", U"e'", engine);
    check("rs/escape", U"e\\'", U"e'", engine);
}

void testRegistry()
{
    auto& registry = st::RuleEngineRegistry::instance();
    registry.registerBuiltins();
    checkTrue("registry/de", registry.contains("de"));
    checkTrue("registry/vi", registry.contains("vi"));
    checkTrue("registry/create", registry.create("vi") != nullptr);
    checkTrue("registry/unknown", registry.create("nope") == nullptr);

    st::RuleSetDefinition def;
    def.id          = "custom:fr";
    def.displayName = "French";
    def.badge       = "FR";
    registry.registerRuleSet(def);
    checkTrue("registry/ruleset", registry.contains("custom:fr"));
    registry.clearRuleSets();
    checkTrue("registry/clear", !registry.contains("custom:fr"));
    checkTrue("registry/keeps-builtins", registry.contains("de"));
}

} // namespace

int main()
{
    testGerman();
    testVietnameseTelex();
    testVietnameseVni();
    testRuleSet();
    testRegistry();

    std::printf("\n%d checks, %d failure(s)\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
