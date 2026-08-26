// SPDX-License-Identifier: MIT
#include "core/custom/RuleSetEngine.hpp"

#include <algorithm>

namespace st {
namespace {

[[nodiscard]] bool isWordCharacter(char32_t c) noexcept
{
    // Letters (ASCII or beyond), digits, and the punctuation commonly used as a
    // diacritic key in transliteration schemes.
    if (unicode::isAsciiAlpha(c) || unicode::isAsciiDigit(c))
        return true;
    if (c > 0x7F)
        return true;
    switch (c) {
    case U'\'': case U'`': case U'^': case U'~': case U'"': case U':':
    case U'-':  case U'_':
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool equalsIgnoreAsciiCase(std::u32string_view a, std::u32string_view b) noexcept
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (unicode::toLowerAscii(a[i]) != unicode::toLowerAscii(b[i]))
            return false;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------

RuleSetEngine::RuleSetEngine(RuleSetDefinition definition) : def_(std::move(definition))
{
    // Longest triggers first: the search can then stop at the first hit.
    std::stable_sort(def_.rules.begin(), def_.rules.end(),
                     [](const SubstitutionRule& a, const SubstitutionRule& b) {
                         return a.trigger.size() > b.trigger.size();
                     });
}

RuleSetEngine::~RuleSetEngine() = default;

void RuleSetEngine::reset() noexcept
{
    buf_.clear();
    escapeArmed_ = false;
    pendingUndo_.reset();
}

const SubstitutionRule* RuleSetEngine::longestMatch(std::size_t* matchedLength) const
{
    const std::u32string& text = buf_.text();
    for (const SubstitutionRule& rule : def_.rules) {
        const std::size_t n = rule.trigger.size();
        if (n == 0 || n > text.size())
            continue;
        const std::u32string_view tail{text.data() + (text.size() - n), n};

        // Every character of the candidate must still be unlocked.
        bool unlocked = true;
        for (std::size_t i = 0; i < n; ++i)
            if (buf_.lockedFromEnd(i)) {
                unlocked = false;
                break;
            }
        if (!unlocked)
            continue;

        if (rule.wordStart && text.size() != n)
            continue;

        const bool hit = rule.caseSensitive ? tail == rule.trigger
                                            : equalsIgnoreAsciiCase(tail, rule.trigger);
        if (hit) {
            *matchedLength = n;
            return &rule;
        }
    }
    return nullptr;
}

std::u32string RuleSetEngine::applyCase(const SubstitutionRule& rule,
                                        std::u32string_view     typed) const
{
    if (rule.caseSensitive || !rule.smartCase)
        return rule.replacement;

    const bool anyLetter =
        std::any_of(typed.begin(), typed.end(), [](char32_t c) { return unicode::isAsciiAlpha(c); });
    if (!anyLetter)
        return rule.replacement;

    const bool allUpper = std::all_of(typed.begin(), typed.end(), [](char32_t c) {
        return !unicode::isAsciiAlpha(c) || unicode::isAsciiUpper(c);
    });
    const bool firstUpper = unicode::isAsciiUpper(typed.front());

    std::u32string out = rule.replacement;
    if (allUpper && typed.size() > 1) {
        for (char32_t& c : out)
            c = unicode::toUpperAscii(c);
    } else if (firstUpper && !out.empty()) {
        out.front() = unicode::toUpperAscii(out.front());
    }
    return out;
}

// ---------------------------------------------------------------------------

EngineResult RuleSetEngine::processKey(const KeyEvent& event)
{
    if (event.isChord()) {
        reset();
        return EngineResult::passthrough();
    }

    switch (event.kind) {
    case KeyKind::Backspace:
        buf_.pop();
        pendingUndo_.reset();
        escapeArmed_ = false;
        return EngineResult::passthrough();
    case KeyKind::Modifier:
        return EngineResult::passthrough();
    case KeyKind::Character:
        break;
    default:
        reset();
        return EngineResult::passthrough();
    }

    const char32_t c = event.character;

    if (def_.escapeKey != 0 && c == def_.escapeKey && !escapeArmed_) {
        escapeArmed_ = true;
        pendingUndo_.reset();
        return EngineResult::swallow();
    }

    if (!isWordCharacter(c)) {
        reset();
        return EngineResult::passthrough();
    }

    const std::u32string before = buf_.text();
    const auto           undo   = pendingUndo_;
    pendingUndo_.reset();

    if (def_.doubleKeyRevert && undo && c == undo->undoKey && buf_.size() >= undo->produced.size()
        && buf_.text().compare(buf_.size() - undo->produced.size(), undo->produced.size(),
                               undo->produced)
               == 0) {
        buf_.replaceBack(undo->produced.size(), undo->original, /*locked=*/true);
    } else if (escapeArmed_) {
        escapeArmed_ = false;
        buf_.lockLast(1);
        buf_.push(c, std::u32string{c}, /*locked=*/true);
    } else {
        buf_.push(c, std::u32string{c});

        std::size_t             matched = 0;
        const SubstitutionRule* rule    = longestMatch(&matched);
        if (rule != nullptr) {
            const std::u32string typed{buf_.text().substr(buf_.size() - matched)};
            const std::u32string produced = applyCase(*rule, typed);
            buf_.replaceBack(matched, produced, /*locked=*/false);
            pendingUndo_ = Applied{typed, produced, c};
        }
    }

    const std::u32string& after  = buf_.text();
    const std::size_t     common = unicode::commonPrefix(before, after);

    EngineResult r;
    r.backspaces = static_cast<int>(before.size() - common);
    r.text       = after.substr(common);
    if (r.backspaces == 0 && r.text.size() == 1 && r.text[0] == c)
        return EngineResult::passthrough();
    r.handled = true;
    return r;
}

// ---------------------------------------------------------------------------

void RuleSetEngine::applyOptions(const EngineOptions& options)
{
    if (const auto it = options.find("doubleKeyRevert"); it != options.end())
        def_.doubleKeyRevert = it->second != "false" && it->second != "0";
    if (const auto it = options.find("escapeKey"); it != options.end()) {
        const std::u32string s = unicode::fromUtf8(it->second);
        def_.escapeKey         = s.empty() ? 0 : s.front();
    }
    reset();
}

EngineOptions RuleSetEngine::options() const
{
    EngineOptions o;
    o["doubleKeyRevert"] = def_.doubleKeyRevert ? "true" : "false";
    o["escapeKey"] = def_.escapeKey ? unicode::toUtf8(std::u32string{def_.escapeKey}) : "";
    return o;
}

} // namespace st
