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
#include "core/CompositionSession.hpp"
#include "core/auto/AutoRuleEngine.hpp"
#include "core/custom/RuleSetEngine.hpp"
#include "core/german/GermanRuleEngine.hpp"
#include "core/vietnamese/VietnameseRuleEngine.hpp"

#include <cstdio>
#include <iterator>
#include <random>
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
    checkTrue("registry/auto", registry.contains("auto"));
    checkTrue("registry/auto-first", registry.descriptors().front().id == "auto");
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

void testAutomatic()
{
    st::AutoRuleEngine engine;
    check("auto/requested-example", U"hoom nay tooi ddeens New York ",
          U"hôm nay tôi đến New York ", engine);
    check("auto/english", U"I have been using new tools at work. ",
          U"I have been using new tools at work. ", engine);
    check("auto/english-triggers", U"as is or of for were own our power flower street class ",
          U"as is or of for were own our power flower street class ", engine);
    check("auto/names", U"New York Berlin Microsoft Firefox Robert ",
          U"New York Berlin Microsoft Firefox Robert ", engine);
    check("auto/vietnamese", U"tooi yeeu tieengs Vieejt ", U"tôi yêu tiếng Việt ", engine);
    check("auto/vietnamese-free", U"nuocws nguowif toasn ", U"nước người toán ", engine);
    check("auto/german", U"Ich moechte schoene Aepfel und Wasser. ",
          U"Ich möchte schöne Äpfel und Wasser. ", engine);
    check("auto/german-exceptions", U"Dauer Duell Wasser aussehen muessen ",
          U"Dauer Duell Wasser aussehen müssen ", engine);
    check("auto/three-languages", U"tooi ddeens Berlin und moechte coffee ",
          U"tôi đến Berlin und möchte coffee ", engine);
    check("auto/unknown", U"Zyqwert Fjordson Qwark ", U"Zyqwert Fjordson Qwark ", engine);
    check("auto/email", U"hoom@tooi.com ", U"hoom@tooi.com ", engine);
    check("auto/path", U"C:\\users\\tooi\\hoom ", U"C:\\users\\tooi\\hoom ", engine);
    check("auto/url", U"https://example.com/hoom ", U"https://example.com/hoom ", engine);
    check("auto/identifier", U"hoom_test ", U"hoom_test ", engine);
    check("auto/contraction", U"we're don't it's ", U"we're don't it's ", engine);
    check("auto/number", U"hoom123 ", U"hoom123 ", engine);
    check("auto/precomposed", U"hôm nay tôi đến New York ", U"hôm nay tôi đến New York ", engine);
    check("auto/backspace", U"tooi\b ", U"tô ", engine);
    check("auto/backspace-composed", U"oo\b ", U" ", engine);
    check("auto/punctuation", U"tooi,New York!", U"tôi,New York!", engine);

    engine.applyOptions({{"method", "vni"}});
    check("auto/vni", U"ho6m nay to6i d9e6n1 New York ",
          U"hôm nay tôi đến New York ", engine);
    engine.applyOptions({{"method", "telex"}, {"literalWords", "hoom\ntooi"}});
    check("auto/personal-literal", U"Hoom tooi tieengs ", U"Hoom tooi tiếng ", engine);

    engine.applyOptions({{"literalWords", ""}});
    engine.reset();
    const auto before = typeKeys(engine, U"tooi");
    const auto undo = engine.processKey(st::KeyEvent::of(st::KeyKind::Escape));
    auto screen = before;
    screen.resize(screen.size() - static_cast<std::size_t>(undo.backspaces));
    screen += undo.text;
    checkTrue("auto/escape-restores", undo.handled && screen == U"tooi");

    engine.reset();
    (void)typeKeys(engine, U"tooi");
    engine.reset();  // new window/input field must never edit the old word
    const auto fresh = engine.processKey(st::KeyEvent::character_(U'a'));
    checkTrue("auto/reset-safe", fresh.backspaces == 0);

    auto chord = st::KeyEvent::character_(U'c');
    chord.ctrl = true;
    checkTrue("auto/shortcuts", !engine.processKey(chord).handled);
    check("auto/bounded-word", std::u32string(1024, U'a') + U" ",
          std::u32string(1024, U'a') + U" ", engine);
}

void testComposition()
{
    st::CompositionSession session(std::make_unique<st::AutoRuleEngine>());
    st::CompositionSession::Result result;
    for (const auto c : U"hoom") {
        if (c != 0)
            result = session.processKey(st::KeyEvent::character_(c));
    }
    checkTrue("composition/preedit", result.handled && !result.commit && result.text == U"hôm");
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Space));
    checkTrue("composition/commit-space", result.handled && result.commit && result.text == U"hôm ");
    result = session.finish();
    checkTrue("composition/no-double-commit", result.text.empty());
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Backspace));
    checkTrue("composition/backspace-committed", !result.handled && result.text.empty());

    for (const auto c : U"tooi")
        if (c != 0)
            result = session.processKey(st::KeyEvent::character_(c));
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Enter));
    checkTrue("composition/enter-forwarded", !result.handled && result.commit && result.text == U"tôi");
    for (const auto c : U"abc")
        if (c != 0)
            result = session.processKey(st::KeyEvent::character_(c));
    session.reset();
    checkTrue("composition/cancel", session.finish().text.empty());
    st::CompositionSession disabled(nullptr);
    checkTrue("composition/disabled", !disabled.processKey(st::KeyEvent::character_(U'a')).handled);
}

void testLiveComposition()
{
    st::AutoRuleEngine engine;
    check("auto/live-nos", U"nos", U"nó", engine);
    check("auto/short-after-English", U"This is nos maf af ", U"This is nó mà à ", engine);
    check("auto/live-tone-replacement", U"nosf", U"nò", engine);
    check("auto/live-tone-removal", U"nosz", U"no", engine);
    check("auto/live-repeated-tone", U"noss", U"nos", engine);
    engine.applyOptions({{"method", "vni"}});
    check("auto/short-vni", U"no1 ma2 a2 ", U"nó mà à ", engine);

    auto shortcut = [](char32_t c) {
        auto key = st::KeyEvent::character_(c);
        key.ctrl = key.alt = true;
        return key;
    };
    const auto force = shortcut(U'v'), undo = shortcut(U'z');
    const auto make = [] { return st::CompositionSession(std::make_unique<st::AutoRuleEngine>()); };
    auto session = make();
    const auto type = [](st::CompositionSession& target, std::u32string_view keys) {
        st::CompositionSession::Result result;
        for (auto c : keys)
            result = target.processKey(c == U' ' ? st::KeyEvent::of(st::KeyKind::Space) : st::KeyEvent::character_(c));
        return result;
    };
    checkTrue("live/common-English-preserved", type(session, U"as").text == U"as");
    auto result = session.processKey(force);
    checkTrue("live/word-override", result.handled && !result.commit && result.text == U"á" && session.vietnameseOverride());
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Escape));
    checkTrue("live/Escape-cancels-override", result.handled && result.text == U"as" && !session.vietnameseOverride());
    result = session.processKey(undo);
    checkTrue("live/undo-Escape", result.handled && result.text == U"á" && session.vietnameseOverride());
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Space));
    checkTrue("live/override-commits", result.commit && result.text == U"á " && !session.vietnameseOverride());
    checkTrue("live/override-word-only", type(session, U"as").text == U"as");
    session.reset();
    checkTrue("live/no-undo-across-reset", !session.processKey(undo).handled);
    checkTrue("live/empty-override-passes-through", !session.processKey(force).handled);
    type(session, U"nosf");
    checkTrue("live/undo-tone-replacement", session.processKey(undo).text == U"nó");
    checkTrue("live/undo-tone-keystroke", session.processKey(undo).text == U"no");
    session.reset();
    type(session, U"nos");
    session.processKey(st::KeyEvent::of(st::KeyKind::Backspace));
    result = session.processKey(undo);
    checkTrue("live/undo-visible-deletion", result.text == U"nó" && session.literalWord() == U"nos");

    session.reset();
    type(session, U"hoom");
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Left));
    checkTrue("live/caret-left-keeps-preedit", result.handled && !result.commit && result.text == U"hôm" && result.cursor == 2);
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Backspace));
    checkTrue("live/delete-before-caret", result.text == U"hm" && result.cursor == 1 && session.literalWord().empty());
    result = session.processKey(undo);
    checkTrue("live/undo-caret-edit-restores-raw-and-cursor", result.text == U"hôm" && result.cursor == 2 && session.literalWord() == U"hoom");
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Delete));
    checkTrue("live/delete-at-caret", result.text == U"hô" && result.cursor == 2);
    session.processKey(undo);
    result = session.processKey(st::KeyEvent::character_(U'f'));
    checkTrue("live/mark-inside-word", result.text == U"hồm" && result.cursor == 2);
    session.processKey(st::KeyEvent::of(st::KeyKind::End));
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Space));
    checkTrue("live/edited-word-commits-once", result.commit && result.text == U"hồm " && session.finish().text.empty());

    type(session, U"tooi");
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Home));
    checkTrue("live/Home-cursor", result.cursor == 0 && !result.commit);
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Backspace));
    checkTrue("live/Backspace-at-start-does-not-edit-client", result.handled && !result.commit && result.text == U"tôi");
    result = session.processKey(st::KeyEvent::of(st::KeyKind::Delete));
    checkTrue("live/Delete-at-start", result.text == U"ôi" && result.cursor == 0);
    result = session.processKey(st::KeyEvent::character_(U'b'));
    checkTrue("live/insert-at-start", result.text == U"bôi" && result.cursor == 1);
    auto copy = st::KeyEvent::character_(U'c'); copy.ctrl = true;
    result = session.processKey(copy);
    checkTrue("live/application-shortcut-commits-once", !result.handled && result.commit && result.text == U"bôi");
    checkTrue("live/no-undo-after-commit", !session.processKey(undo).handled);

    type(session, U"as");
    auto altGr = force; altGr.altGr = true;
    result = session.processKey(altGr);
    checkTrue("live/AltGr-does-not-force-mode", !result.handled && result.commit && result.text == U"as");

    // The hook's existing end-of-word edit contract uses the same commands.
    engine.applyOptions({{"method", "telex"}, {"literalWords", "nos"}});
    auto text = typeKeys(engine, U"nos");
    const auto apply = [&](const st::KeyEvent& key) {
        const auto edit = engine.processKey(key);
        if (edit.handled) { text.resize(text.size() - static_cast<std::size_t>(edit.backspaces)); text += edit.text; }
    };
    apply(force);
    checkTrue("live/hook-word-override-ignores-literal-list", text == U"nó");
    apply(undo);
    checkTrue("live/hook-undo-override", text == U"nos");

    session.reset();
    result = type(session, U"tooi,");
    checkTrue("live/punctuation-commits-owned-word", result.commit && result.text == U"tôi,");
    checkTrue("live/no-undo-across-punctuation", !session.processKey(undo).handled);
    type(session, U"as");
    result = session.processKey(force);
    checkTrue("live/override-after-punctuation", result.text == U"á" && result.cursor == 1);
    session.reset();
    type(session, U"test@example.com");
    checkTrue("live/address-suffix-not-learnable", session.literalWord().empty());
    session.processKey(st::KeyEvent::of(st::KeyKind::Space));
    type(session, U"hoom");
    checkTrue("live/learning-after-token-boundary", session.literalWord() == U"hoom");

    auto vni = std::make_unique<st::AutoRuleEngine>();
    vni->applyOptions({{"method", "vni"}, {"literalWords", "no1"}});
    st::CompositionSession vniSession(std::move(vni));
    type(vniSession, U"no1");
    checkTrue("live/forced-VNI-keeps-method", vniSession.processKey(force).text == U"nó");

    std::minstd_rand random(73);
    std::vector<st::KeyEvent> events;
    for (int i = 0; i < 2000; ++i) {
        constexpr std::u32string_view letters = U"nosmafzwhoộạ\U0001f680";
        constexpr st::KeyKind controls[] = {st::KeyKind::Space, st::KeyKind::Backspace,
            st::KeyKind::Left, st::KeyKind::Right, st::KeyKind::Home, st::KeyKind::End,
            st::KeyKind::Delete, st::KeyKind::UndoComposition, st::KeyKind::ForceVietnamese,
            st::KeyKind::Escape, st::KeyKind::Enter};
        const auto index = random() % (letters.size() + std::size(controls));
        events.push_back(index < letters.size() ? st::KeyEvent::character_(letters[index])
            : st::KeyEvent::of(controls[index - letters.size()]));
    }
    auto first = make(), second = make();
    bool deterministic = true, bounded = true;
    for (const auto& event : events) {
        const auto a = first.processKey(event), b = second.processKey(event);
        deterministic &= a.handled == b.handled && a.commit == b.commit && a.text == b.text && a.cursor == b.cursor;
        bounded &= a.text.size() <= 256 && a.cursor <= a.text.size();
    }
    checkTrue("live/edit-sequence-is-deterministic", deterministic);
    checkTrue("live/edit-sequence-keeps-text-and-cursor-bounded", bounded);
}

} // namespace

int main()
{
    testGerman();
    testVietnameseTelex();
    testVietnameseVni();
    testRuleSet();
    testRegistry();
    testAutomatic();
    testComposition();
    testLiveComposition();

    std::printf("\n%d checks, %d failure(s)\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
