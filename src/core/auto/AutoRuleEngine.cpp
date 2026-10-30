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

template<std::size_t N>
int frequency(const std::array<LexiconEntry, N>& dictionary, std::u32string_view word)
{
    const std::string key = unicode::toUtf8(lower(word));
    const auto it = std::lower_bound(dictionary.begin(), dictionary.end(), key,
                                   [](const LexiconEntry& entry, std::string_view value) {
                                       return entry.word < value;
                                   });
    return it != dictionary.end() && it->word == key ? it->frequency : 0;
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
    german_.applyOptions(options);
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
    Candidate best{raw_, language, std::max({english, german, vietnamese, 280}) + 45};
    auto consider = [&](std::u32string text, Language candidateLanguage, int score) {
        if (text == raw_)
            return;
        if (candidateLanguage == context_)
            score += 55;
        // A clear margin favours keeping a word over a speculative conversion.
        if (score > best.score + 25)
            best = {std::move(text), candidateLanguage, score};
    };

    auto vi = render(vietnamese_, raw_, final);
    const int viScore = frequency(kVietnameseWords, vi);
    if (viScore >= 300)
        consider(std::move(vi), Language::Vietnamese, viScore);

    auto de = render(german_, raw_, final);
    const int deScore = frequency(kGermanWords, de);
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
