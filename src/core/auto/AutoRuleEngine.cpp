// SPDX-License-Identifier: MIT
#include "core/auto/AutoRuleEngine.hpp"

#include <algorithm>
#include <array>
#include <string_view>

namespace st {
namespace {

struct LexiconEntry {
    std::string_view word;
    int frequency;
};

// The frequency tables have their own CC BY-SA 4.0 attribution. The engine
// code is MIT; see data/NOTICE-wordfreq.md and tools/generate-frequency-data.py.
#include "core/auto/FrequencyData.inc"
#include "core/auto/data/GermanSharpS.inc"

char32_t lowerLatin(char32_t c)
{
    if (const auto letter = vn::decompose(c))
        return vn::compose(letter->letter.base, letter->letter.mark, letter->tone, false);
    if (c == U'ẞ')
        return U'ß';
    if (c >= U'À' && c <= U'Þ' && c != U'×')
        return c + 32;
    return unicode::toLowerAscii(c);
}

std::u32string lower(std::u32string_view word)
{
    std::u32string result(word);
    std::transform(result.begin(), result.end(), result.begin(), lowerLatin);
    return result;
}

// wordfreq case-folds sharp S to "ss". Keep that normalization in lookup
// keys only: the candidate shown to the user must retain its German spelling.
std::u32string frequencyKey(std::u32string_view word)
{
    std::u32string result;
    for (const char32_t c : lower(word)) {
        if (c == U'ß')
            result += U"ss";
        else
            result.push_back(c);
    }
    return result;
}

template<std::size_t N>
int frequency(const std::array<LexiconEntry, N>& dictionary, std::u32string_view word)
{
    const std::string key = unicode::toUtf8(frequencyKey(word));
    const auto it = std::lower_bound(dictionary.begin(), dictionary.end(), key,
                                   [](const LexiconEntry& entry, std::string_view value) {
                                       return entry.word < value;
                                   });
    return it != dictionary.end() && it->word == key ? it->frequency : 0;
}

int germanFrequency(std::u32string_view word)
{
    if (const int direct = frequency(kGermanWords, word))
        return direct;
    // German compounds need not occur in a capped whole-word table. Recognize
    // two attested parts, including verb stems such as füll(en) + Menge.
    int compound = 0;
    for (std::size_t split = 3; split + 3 <= word.size(); ++split) {
        const auto head = word.substr(0, split);
        const auto tail = word.substr(split);
        const int left = std::max(frequency(kGermanWords, head),
                                  frequency(kGermanWords, std::u32string(head) + U"en"));
        const int right = frequency(kGermanWords, tail);
        if (left >= 350 && right >= 350)
            compound = std::max(compound, std::min(left, right) - 40);
    }
    return compound;
}

bool wordCharacter(char32_t c)
{
    return unicode::isAsciiAlpha(c) || vn::decompose(c).has_value()
           || c == U'ä' || c == U'Ä' || c == U'ö' || c == U'Ö'
           || c == U'ü' || c == U'Ü' || c == U'ß' || c == U'ẞ';
}

void apply(std::u32string& screen, const KeyEvent& key, const EngineResult& result)
{
    if (result.handled) {
        const auto remove = std::min(screen.size(), static_cast<std::size_t>(result.backspaces));
        screen.resize(screen.size() - remove);
        screen += result.text;
    } else if (key.character != 0) {
        screen.push_back(key.character);
    }
}

std::u32string render(LanguageRuleEngine& engine, std::u32string_view raw, bool final)
{
    engine.reset();
    std::u32string result;
    for (const char32_t c : raw) {
        const auto key = KeyEvent::character_(c, unicode::isAsciiUpper(c));
        apply(result, key, engine.processKey(key));
    }
    if (final) {
        // The separator itself is handled by AutoRuleEngine. An empty space
        // event still asks the Vietnamese engine to validate/restore its word.
        const auto key = KeyEvent::of(KeyKind::Space);
        auto edit = engine.processKey(key);
        if (edit.handled && !edit.text.empty() && edit.text.back() == U' ')
            edit.text.pop_back();
        apply(result, key, edit);
    }
    return result;
}

} // namespace

AutoRuleEngine::AutoRuleEngine()
{
    applyOptions({});
}

void AutoRuleEngine::clearWord() noexcept
{
    raw_.clear();
    emitted_.clear();
    literal_ = false;
    overflow_ = false;
}

void AutoRuleEngine::reset() noexcept
{
    clearWord();
    context_ = Language::Literal;
    vietnamese_.reset();
    german_.reset();
}

void AutoRuleEngine::setGermanExceptions(GermanRuleEngine::ExceptionTable exceptions)
{
    german_.setExceptions(std::move(exceptions));
    reset();
}

void AutoRuleEngine::applyOptions(const EngineOptions& options)
{
    options_ = options;
    vietnamese_.applyOptions(options);
    auto germanOptions = options;
    // Escape is Automatic's literal recovery. A third s is part of compounds
    // such as Grossstadt, not the manual German mode's double-key undo.
    germanOptions["doubleKeyRevert"] = "false";
    german_.applyOptions(germanOptions);
    literalWords_.clear();
    if (const auto it = options.find("literalWords"); it != options.end()) {
        const auto words = unicode::fromUtf8(it->second);
        std::u32string word;
        for (const char32_t c : words + U"\n") {
            if (c == U'\n' || c == U'\r' || c == U' ' || c == U'\t' || c == U',') {
                if (!word.empty())
                    literalWords_.insert(lower(word));
                word.clear();
            } else {
                word.push_back(c);
            }
        }
    }
    reset();
}

AutoRuleEngine::Candidate AutoRuleEngine::choose(bool final)
{
    if (literal_ || raw_.empty() || literalWords_.contains(lower(raw_)))
        return {raw_, Language::Literal, 0};

    const int english = frequency(kEnglishWords, raw_);
    const int german = frequency(kGermanWords, raw_);
    const int vietnamese = frequency(kVietnameseWords, raw_);
    Language language = Language::English;
    if (german > english + 60 && german > vietnamese)
        language = Language::German;
    else if (vietnamese > english + 60 && vietnamese > german)
        language = Language::Vietnamese;

    // ponytail: unigram frequencies plus one committed word of language context;
    // add a compact contextual model if held-out mixed prose needs more accuracy.
    auto de = render(german_, raw_, final);
    if (raw_.size() > 1 && std::all_of(raw_.begin(), raw_.end(), [](char32_t c) {
            return lowerLatin(c) != c;
        })) {
        // Default German uppercase spelling uses SS. The explicit capitalSharpS
        // option already renders ẞ, which is preserved here.
        std::u32string uppercase;
        for (const char32_t c : de) {
            if (c == U'ß')
                uppercase += U"SS";
            else
                uppercase.push_back(c);
        }
        de = std::move(uppercase);
    }
    // Case-folded frequencies cannot tell Borussia from the invalid Borußia.
    // Accept a sharp-S candidate only when an independent spelling list attests
    // that exact spelling. Absence favours preserving the user's original keys.
    if (de.find(U'ß') != std::u32string::npos || de.find(U'ẞ') != std::u32string::npos) {
        const auto spelling = unicode::toUtf8(lower(de));
        if (!std::binary_search(kGermanSharpSWords.begin(), kGermanSharpSWords.end(), spelling))
            de = raw_;
    }
    const int deScore = germanFrequency(de);
    int literalGerman = german;
    // A folded dictionary entry is evidence for Straße, not evidence that
    // the ASCII spelling Strasse should win over it. Common English words
    // (gross) and English-context homographs (fuss) still favour literal input.
    const bool sharpS = de.find(U'ß') != std::u32string::npos
                        || de.find(U'ẞ') != std::u32string::npos;
    // English dictionary evidence protects cars/cans/moon from Telex. At a
    // fresh start only a strongly German sharp-S spelling (Fuss/Weiss) can
    // override it; after English prose even those homographs stay literal.
    if (english > 0
        && (context_ == Language::English
            || (context_ == Language::Literal
                && (english >= 400 || raw_.size() >= 3)
                && !(sharpS && deScore > english + 100))))
        return {raw_, Language::English, english};
    if (sharpS && de != raw_ && frequencyKey(de) == frequencyKey(raw_)
        && deScore >= 300 && german > vietnamese + 60
        && (english < 400 || context_ == Language::German)
        && !(context_ == Language::English && english >= 300))
        literalGerman = 0;

    Candidate best{raw_, language, std::max({english, literalGerman, vietnamese, 280}) + 45};
    auto consider = [&](std::u32string text, Language candidateLanguage, int score) {
        if (text == raw_)
            return;
        if (candidateLanguage == context_)
            score += 55;
        // A clear margin favours keeping a word over a speculative conversion.
        if (score > best.score + 25)
            best = {std::move(text), candidateLanguage, score};
    };

    auto vi = render(vietnamese_, raw_, false);
    const bool literalUndo = vietnamese_.hasLiteralUndo();
    if (final)
        vi = render(vietnamese_, raw_, true);
    // Repeated modifiers explicitly request literal keys (uww -> uw). That
    // result need not be a Vietnamese dictionary word. Known English words
    // retain their protection above; personal literals were checked first.
    // The two-key standalone-w undo is literal even though "ww" also occurs
    // as an abbreviation in the tables; it must not establish Vietnamese context.
    if (literalUndo && (raw_.size() == 2 || (english == 0 && german == 0))
        && !(de != raw_ && deScore >= 300))
        return {std::move(vi), Language::Literal, 0};
    const int viScore = frequency(kVietnameseWords, vi);
    if (viScore >= 300)
        consider(std::move(vi), Language::Vietnamese, viScore);

    if (deScore >= 300)
        consider(std::move(de), Language::German, deScore);
    return best;
}

EngineResult AutoRuleEngine::replace(std::u32string text, char32_t natural)
{
    const auto common = unicode::commonPrefix(emitted_, text);
    const int erase = static_cast<int>(emitted_.size() - common);
    auto insert = text.substr(common);
    emitted_ = std::move(text);
    if (erase == 0 && insert.size() == 1 && insert.front() == natural)
        return EngineResult::passthrough();
    return EngineResult::edit(erase, std::move(insert));
}

EngineResult AutoRuleEngine::processKey(const KeyEvent& event)
{
    if (event.isChord()) {
        reset();
        return EngineResult::passthrough();
    }
    if (event.kind == KeyKind::Modifier)
        return EngineResult::passthrough();
    if (event.kind == KeyKind::Escape) {
        if (raw_ != emitted_ && !raw_.empty()) {
            literal_ = true;
            return replace(raw_);
        }
        reset();
        return EngineResult::passthrough();
    }
    if (event.kind == KeyKind::Backspace) {
        if (emitted_.empty() || overflow_) {
            reset();
            return EngineResult::passthrough();
        }
        // Backspace removes one visible character, even when several physical
        // keys produced it. Keep remaining accents as directly typed Unicode.
        raw_ = emitted_;
        raw_.pop_back();
        emitted_ = raw_;
        return EngineResult::passthrough();
    }

    const char32_t c = event.character;
    const bool vniDigit = vietnamese_.vnOptions().method == VnInputMethod::Vni
                          && unicode::isAsciiDigit(c) && !raw_.empty();
    if (event.kind == KeyKind::Character && (wordCharacter(c) || vniDigit || literal_)) {
        if (overflow_)
            return EngineResult::passthrough();
        raw_.push_back(c);
        if (raw_.size() > 128) {
            auto edit = replace(raw_, c);
            raw_.clear();
            emitted_.clear();
            overflow_ = true;
            return edit;
        }
        return replace(choose(false).text, c);
    }

    // Keep email addresses, paths, identifiers, numbers and contractions literal.
    // This also restores a partially composed username before its '@' arrives.
    if (event.kind == KeyKind::Character
        && (c == U'@' || c == U'/' || c == U'\\' || c == U'_' || c == U':'
            || c == U'=' || c == U'#' || c == U'\'' || unicode::isAsciiDigit(c))) {
        literal_ = true;
        raw_.push_back(c);
        return replace(raw_, c);
    }

    if (event.kind == KeyKind::Space || event.kind == KeyKind::Enter
        || event.kind == KeyKind::Tab || event.kind == KeyKind::Character) {
        auto candidate = choose(true);
        context_ = candidate.language;
        // Space/punctuation can be included in the replacement atomically.
        // Enter/Tab must still reach the focused application as command keys.
        if (event.kind == KeyKind::Space || event.kind == KeyKind::Character) {
            candidate.text.push_back(event.kind == KeyKind::Space ? U' ' : c);
            auto result = replace(std::move(candidate.text), event.kind == KeyKind::Space ? U' ' : c);
            clearWord();
            if (c == U'.' || c == U'!' || c == U'?')
                context_ = Language::Literal;
            return result;
        }
        clearWord();
        context_ = Language::Literal;
        return EngineResult::passthrough();
    }
    reset();
    return EngineResult::passthrough();
}

} // namespace st
