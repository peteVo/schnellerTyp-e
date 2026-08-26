// SPDX-License-Identifier: MIT
#include "core/vietnamese/VietnameseRuleEngine.hpp"

#include <algorithm>

namespace st {
namespace {

using vn::Letter;
using vn::Mark;
using vn::Tone;

[[nodiscard]] bool boolFromString(std::string_view v, bool fallback)
{
    if (v == "1" || v == "true" || v == "yes" || v == "on")
        return true;
    if (v == "0" || v == "false" || v == "no" || v == "off")
        return false;
    return fallback;
}

} // namespace

// ---------------------------------------------------------------------------

VietnameseRuleEngine::VietnameseRuleEngine() : VietnameseRuleEngine(Options{}) {}
VietnameseRuleEngine::VietnameseRuleEngine(Options options) : opts_(options) {}
VietnameseRuleEngine::~VietnameseRuleEngine() = default;

void VietnameseRuleEngine::setVnOptions(const Options& o)
{
    opts_ = o;
    reset();
}

void VietnameseRuleEngine::reset() noexcept
{
    letters_.clear();
    tone_        = Tone::None;
    raw_.clear();
    emitted_.clear();
    toneKeyUsed_ = 0;
    markKeyUsed_ = 0;
    toneBlockAt_ = kNoBlock;
    markBlockAt_ = kNoBlock;
    rawDirty_    = false;
}

// ---------------------------------------------------------------------------
// Key classification
// ---------------------------------------------------------------------------

std::optional<Tone> VietnameseRuleEngine::toneKeyFor(char32_t lower) const noexcept
{
    if (opts_.method == VnInputMethod::Telex) {
        switch (lower) {
        case U's': return Tone::Acute;
        case U'f': return Tone::Grave;
        case U'r': return Tone::Hook;
        case U'x': return Tone::Tilde;
        case U'j': return Tone::Dot;
        case U'z': return Tone::None;  // remove tone
        default:   return std::nullopt;
        }
    }
    switch (lower) {  // VNI
    case U'1': return Tone::Acute;
    case U'2': return Tone::Grave;
    case U'3': return Tone::Hook;
    case U'4': return Tone::Tilde;
    case U'5': return Tone::Dot;
    case U'0': return Tone::None;
    default:   return std::nullopt;
    }
}

std::optional<Mark> VietnameseRuleEngine::markKeyFor(char32_t lower) const noexcept
{
    if (opts_.method == VnInputMethod::Telex) {
        // 'w' is the only dedicated mark key; aa/ee/oo/dd are handled as
        // letter doubling inside processKey().
        if (lower == U'w')
            return Mark::Horn;  // resolved to Breve on 'a' by markTargets()
        return std::nullopt;
    }
    switch (lower) {  // VNI
    case U'6': return Mark::Circumflex;
    case U'7': return Mark::Horn;
    case U'8': return Mark::Breve;
    case U'9': return Mark::Stroke;
    default:   return std::nullopt;
    }
}

bool VietnameseRuleEngine::isModifierKey(char32_t lower) const noexcept
{
    return toneKeyFor(lower).has_value() || markKeyFor(lower).has_value();
}

// ---------------------------------------------------------------------------
// Transformations
// ---------------------------------------------------------------------------

std::vector<std::size_t> VietnameseRuleEngine::markTargets(Mark mark) const
{
    std::vector<std::size_t> targets;

    if (mark == Mark::Stroke) {
        for (std::size_t i = letters_.size(); i-- > 0;)
            if (letters_[i].base == U'd') {
                targets.push_back(i);
                break;
            }
        return targets;
    }

    const vn::SyllableParts p = vn::split(letters_);
    if (!p.hasVowel)
        return targets;

    const std::size_t begin = p.nucleusBegin;  // "qu"/"gi" u/i may still take a horn
    const std::size_t end   = p.nucleusEnd;

    // "uo" always takes the horn as a pair: ươ, never ưo or uơ.
    if (mark == Mark::Horn) {
        for (std::size_t i = begin; i + 1 < end; ++i) {
            if (letters_[i].base == U'u' && letters_[i + 1].base == U'o') {
                // ...unless "u" belongs to the "qu" onset (quở, not qưở).
                if (p.toneScanBegin > i)
                    break;
                targets.push_back(i);
                targets.push_back(i + 1);
                return targets;
            }
        }
    }

    // Otherwise the last vowel in the nucleus that can carry the mark wins.
    for (std::size_t i = end; i-- > begin;) {
        Mark effective = mark;
        if (mark == Mark::Horn && letters_[i].base == U'a')
            effective = Mark::Breve;  // Telex 'w' means breve on 'a'
        if (vn::markAllowed(letters_[i].base, effective)) {
            targets.push_back(i);
            return targets;
        }
    }
    return targets;
}

bool VietnameseRuleEngine::applyTone(Tone tone, char32_t key)
{
    if (letters_.size() == toneBlockAt_)
        return false;  // the key was just used literally at this position

    const vn::SyllableParts p = vn::split(letters_);
    if (!p.hasVowel)
        return false;  // "s" in "sao", "r" in "ra" — no vowel yet, it is a letter

    if (tone == Tone::None) {  // Telex 'z' / VNI '0'
        if (tone_ == Tone::None)
            return false;
        tone_        = Tone::None;
        toneKeyUsed_ = 0;
        return true;
    }

    if (tone_ == tone && toneKeyUsed_ == key) {
        // Second press of the same tone key: undo, and type the key literally.
        tone_        = Tone::None;
        toneKeyUsed_ = 0;
        appendLiteral(key);
        toneBlockAt_ = letters_.size();
        return true;
    }

    const Tone saved = tone_;
    tone_            = tone;
    if (opts_.spellCheck && !vn::isValidSyllable(letters_, tone_, /*partial=*/true)) {
        tone_ = saved;
        return false;
    }
    toneKeyUsed_ = key;
    return true;
}

bool VietnameseRuleEngine::applyMark(Mark mark, char32_t key)
{
    if (letters_.size() == markBlockAt_)
        return false;

    std::vector<std::size_t> targets = markTargets(mark);

    // Telex: a bare 'w' in a syllable that has no vowel yet types "ư" — the
    // standard shortcut for "ư", "tư", "thư"… When the syllable *does* have a
    // vowel but none of them can take the mark ("kew"), the key is a literal.
    const bool syllableHasVowel = vn::split(letters_).hasVowel;
    if (targets.empty() && !syllableHasVowel && opts_.method == VnInputMethod::Telex
        && mark == Mark::Horn) {
        Letter l;
        l.base  = U'u';
        l.mark  = Mark::Horn;
        l.upper = unicode::isAsciiUpper(key);
        letters_.push_back(l);
        markKeyUsed_ = U'w';
        toneBlockAt_ = kNoBlock;
        markBlockAt_ = kNoBlock;
        return true;
    }
    if (targets.empty())
        return false;

    // Resolve 'w' to breve where the target is an 'a'.
    auto effectiveMark = [&](std::size_t idx) {
        if (mark == Mark::Horn && letters_[idx].base == U'a')
            return Mark::Breve;
        return mark;
    };

    const bool alreadySet = std::all_of(targets.begin(), targets.end(), [&](std::size_t i) {
        return letters_[i].mark == effectiveMark(i);
    });

    if (alreadySet) {
        // Second press: strip the mark and type the key literally ("uww" -> "uw").
        for (std::size_t i : targets)
            letters_[i].mark = Mark::None;
        markKeyUsed_ = 0;
        appendLiteral(key);
        markBlockAt_ = letters_.size();
        return true;
    }

    std::vector<Mark> saved;
    saved.reserve(targets.size());
    for (std::size_t i : targets) {
        saved.push_back(letters_[i].mark);
        letters_[i].mark = effectiveMark(i);
    }

    if (opts_.spellCheck && !vn::isValidSyllable(letters_, tone_, /*partial=*/true)) {
        for (std::size_t k = 0; k < targets.size(); ++k)
            letters_[targets[k]].mark = saved[k];
        return false;
    }

    markKeyUsed_ = key;
    return true;
}

void VietnameseRuleEngine::appendLiteral(char32_t c)
{
    Letter l;
    if (const auto d = vn::decompose(c)) {
        l = d->letter;
        if (d->tone != Tone::None && tone_ == Tone::None)
            tone_ = d->tone;  // user typed a precomposed character directly
    } else {
        l.base  = unicode::toLowerAscii(c);
        l.upper = unicode::isAsciiUpper(c);
    }
    letters_.push_back(l);

    // A modifier character typed literally must not immediately act as a
    // modifier again at the same position.
    const char32_t lower = unicode::toLowerAscii(c);
    toneBlockAt_ = toneKeyFor(lower).has_value() ? letters_.size() : kNoBlock;
    markBlockAt_ = markKeyFor(lower).has_value() ? letters_.size() : kNoBlock;
}

// ---------------------------------------------------------------------------
// Flow
// ---------------------------------------------------------------------------

EngineResult VietnameseRuleEngine::finish(const std::u32string& before, char32_t typedChar)
{
    emitted_ = vn::render(letters_, tone_, opts_.modernTone);

    const std::size_t common = unicode::commonPrefix(before, emitted_);

    EngineResult r;
    r.backspaces = static_cast<int>(before.size() - common);
    r.text       = emitted_.substr(common);

    // Nothing changed except appending exactly the character the OS would have
    // produced: let the keystroke through untouched.
    if (r.backspaces == 0 && r.text.size() == 1 && r.text[0] == typedChar)
        return EngineResult::passthrough();

    r.handled = true;
    return r;
}

bool VietnameseRuleEngine::shouldRestore() const
{
    if (!opts_.autoRestore || !opts_.spellCheck || rawDirty_)
        return false;
    if (letters_.empty() || raw_.empty())
        return false;
    if (emitted_ == raw_)
        return false;  // nothing was transformed, nothing to undo
    return !vn::isValidSyllable(letters_, tone_, /*partial=*/false);
}

EngineResult VietnameseRuleEngine::handleBackspace()
{
    if (letters_.empty()) {
        reset();
        return EngineResult::passthrough();
    }

    const std::u32string before = emitted_;

    letters_.pop_back();
    if (letters_.empty())
        tone_ = Tone::None;
    if (!raw_.empty())
        raw_.pop_back();
    rawDirty_    = true;  // raw_ and letters_ are no longer in lock-step
    toneBlockAt_ = kNoBlock;
    markBlockAt_ = kNoBlock;

    emitted_ = vn::render(letters_, tone_, opts_.modernTone);

    // The common case — the rendering simply lost its last character — is what
    // the OS's own Backspace already does. Let it through.
    if (emitted_.size() + 1 == before.size()
        && unicode::commonPrefix(before, emitted_) == emitted_.size())
        return EngineResult::passthrough();

    const std::size_t common = unicode::commonPrefix(before, emitted_);
    EngineResult      r;
    r.handled    = true;
    r.backspaces = static_cast<int>(before.size() - common);
    r.text       = emitted_.substr(common);
    return r;
}

EngineResult VietnameseRuleEngine::handleBoundary(const KeyEvent& event)
{
    const bool     printable = event.kind == KeyKind::Character || event.kind == KeyKind::Space;
    const char32_t c         = event.kind == KeyKind::Space ? U' ' : event.character;

    if (printable && c != 0 && shouldRestore()) {
        EngineResult r;
        r.handled    = true;
        r.backspaces = static_cast<int>(emitted_.size());
        r.text       = raw_;
        r.text.push_back(c);
        reset();
        return r;
    }
    reset();
    return EngineResult::passthrough();
}

// ---------------------------------------------------------------------------

EngineResult VietnameseRuleEngine::processKey(const KeyEvent& event)
{
    if (event.isChord()) {
        reset();
        return EngineResult::passthrough();
    }

    switch (event.kind) {
    case KeyKind::Backspace:
        return handleBackspace();
    case KeyKind::Modifier:
        return EngineResult::passthrough();
    case KeyKind::Character:
        break;
    default:
        return handleBoundary(event);
    }

    const char32_t c     = event.character;
    const char32_t lower = unicode::toLowerAscii(c);

    const bool isLetter = unicode::isAsciiAlpha(c) || vn::decompose(c).has_value();
    const bool isVniDigit =
        opts_.method == VnInputMethod::Vni && unicode::isAsciiDigit(c) && isModifierKey(lower);

    if (!isLetter && !isVniDigit)
        return handleBoundary(event);

    const std::u32string before = emitted_;
    bool                 consumed = false;

    // Telex letter doubling: aa -> â, ee -> ê, oo -> ô, dd -> đ, plus their
    // undos (aaa -> aa). Checked before the generic literal path.
    if (opts_.method == VnInputMethod::Telex && !letters_.empty()
        && letters_.size() != markBlockAt_) {
        Letter&    last = letters_.back();
        const Mark want = (lower == U'd') ? Mark::Stroke : Mark::Circumflex;
        const bool doubling =
            (lower == U'a' || lower == U'e' || lower == U'o' || lower == U'd')
            && last.base == lower;

        if (doubling && last.mark == Mark::None) {
            last.mark = want;
            if (opts_.spellCheck && !vn::isValidSyllable(letters_, tone_, /*partial=*/true)) {
                last.mark = Mark::None;
            } else {
                markKeyUsed_ = lower;
                consumed     = true;
            }
        } else if (doubling && last.mark == want) {
            last.mark = Mark::None;  // undo
            appendLiteral(c);
            markBlockAt_ = letters_.size();
            consumed     = true;
        }
    }

    if (!consumed) {
        if (const auto tone = toneKeyFor(lower))
            consumed = applyTone(*tone, c);
    }
    if (!consumed) {
        if (const auto mark = markKeyFor(lower))
            consumed = applyMark(*mark, c);
    }
    if (!consumed) {
        if (isVniDigit) {
            // A VNI digit that could not be applied is not part of the word.
            return handleBoundary(event);
        }
        appendLiteral(c);
    }

    raw_.push_back(c);
    return finish(before, c);
}

// ---------------------------------------------------------------------------

void VietnameseRuleEngine::applyOptions(const EngineOptions& options)
{
    if (const auto it = options.find("method"); it != options.end())
        opts_.method = (it->second == "vni" || it->second == "VNI") ? VnInputMethod::Vni
                                                                    : VnInputMethod::Telex;

    auto get = [&](std::string_view key, bool fallback) {
        const auto it = options.find(key);
        return it == options.end() ? fallback : boolFromString(it->second, fallback);
    };
    opts_.modernTone  = get("modernTone", opts_.modernTone);
    opts_.spellCheck  = get("spellCheck", opts_.spellCheck);
    opts_.autoRestore = get("autoRestore", opts_.autoRestore);
    reset();
}

EngineOptions VietnameseRuleEngine::options() const
{
    EngineOptions o;
    o["method"]      = opts_.method == VnInputMethod::Vni ? "vni" : "telex";
    o["modernTone"]  = opts_.modernTone ? "true" : "false";
    o["spellCheck"]  = opts_.spellCheck ? "true" : "false";
    o["autoRestore"] = opts_.autoRestore ? "true" : "false";
    return o;
}

} // namespace st
