// SPDX-License-Identifier: MIT
#include "core/german/GermanRuleEngine.hpp"

#include <algorithm>
#include <cctype>

namespace st {
namespace {

constexpr char32_t kSmallSharpS   = U'ß';
constexpr char32_t kCapitalSharpS = U'ẞ';

[[nodiscard]] constexpr std::size_t idx(GermanDigraph d) noexcept
{
    return static_cast<std::size_t>(d);
}

[[nodiscard]] bool isGermanWordChar(char32_t c) noexcept
{
    if (unicode::isAsciiAlpha(c))
        return true;
    switch (c) {
    case U'ä': case U'Ä':
    case U'ö': case U'Ö':
    case U'ü': case U'Ü':
    case kSmallSharpS: case kCapitalSharpS:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] std::string toLowerAsciiString(std::string s)
{
    for (char& ch : s)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

[[nodiscard]] bool boolFromString(std::string_view v, bool fallback)
{
    if (v == "1" || v == "true" || v == "yes" || v == "on")
        return true;
    if (v == "0" || v == "false" || v == "no" || v == "off")
        return false;
    return fallback;
}

/// The two ASCII characters that spell each digraph.
[[nodiscard]] constexpr std::string_view digraphSpelling(GermanDigraph d) noexcept
{
    switch (d) {
    case GermanDigraph::Ae: return "ae";
    case GermanDigraph::Oe: return "oe";
    case GermanDigraph::Ue: return "ue";
    case GermanDigraph::Ss: return "ss";
    default:                return "";
    }
}

// Words in which a trigger digraph is literal, written the way the user types
// them and always containing that digraph. Prefix stems are preferred: "dauer"
// also covers "dauern", "dauerhaft" and "Dauerkarte".
//
// The list can never be complete, and it does not have to be: the double-key
// undo covers whatever it misses, and users can extend it through
// config/german-exceptions.txt without rebuilding.
const GermanRuleEngine::ExceptionTable& builtinTable()
{
    static const GermanRuleEngine::ExceptionTable table = [] {
        GermanRuleEngine::ExceptionTable t;

        t[idx(GermanDigraph::Ae)] = {
            "aerobic", "aerodynamik", "aeronautik", "aerosol",
            "israel",  "maestro",     "michael",
        };

        t[idx(GermanDigraph::Oe)] = {
            "koedukation", "koeffizient", "koexistenz", "oeuvre",
            "poem",        "poesie",      "poet",       "proem", "zoeliakie",
        };

        t[idx(GermanDigraph::Ue)] = {
            "abenteuer",    "aktuel",   "bauer",       "bedauer", "beteuer",
            "betreuer",     "blauer",   "dauer",       "duel",    "duett",
            "erneuer",      "eventuel", "feuer",       "genauer", "getreue",
            "grauer",       "individuel", "intellektuel", "kasuel", "lauer",
            "manuel",       "mauer",    "neue",        "punktuel", "queue",
            "rauer",        "sauer",    "scheuer",     "schlauer", "sexuel",
            "steuer",       "streue",   "teuer",       "trauer",   "treue",
            "virtuel",      "visuel",   "zerstreue",   "zuerst",
        };

        t[idx(GermanDigraph::Ss)] = {
            "abschluss", "adresse",   "assist",   "ausschuss", "aussehen",
            "besser",    "bisschen",  "dass",     "diskussion", "esse",
            "essig",     "fass",      "fluss",    "fressen",   "gasse",
            "gewiss",    "hass",      "hessen",   "interess",  "kasse",
            "kissen",    "klasse",    "kommissar", "kompass",  "kongress",
            "kuessen",   "kuss",      "lassen",   "masse",     "messe",
            "missen",    "missstand", "missverstaendnis",      "muessen",
            "muss",      "nass",      "pass",     "presse",    "professor",
            "prozess",   "risse",     "russland", "schloss",   "schluessel",
            "schluss",   "sessel",    "session",  "stress",    "tasse",
            "wasser",    "wissen",    "wuss",
        };
        return t;
    }();
    return table;
}

/// True when `word` and `typed` are prefix-related in either direction.
[[nodiscard]] bool related(std::string_view word, std::string_view typed) noexcept
{
    const std::size_t n = std::min(word.size(), typed.size());
    return word.compare(0, n, typed, 0, n) == 0;
}

} // namespace

// ---------------------------------------------------------------------------

GermanRuleEngine::GermanRuleEngine() : GermanRuleEngine(Options{}) {}

GermanRuleEngine::GermanRuleEngine(Options options) : opts_(options)
{
    exceptions_ = builtinTable();
    normaliseExceptions();
}

GermanRuleEngine::~GermanRuleEngine() = default;

const GermanRuleEngine::ExceptionTable& GermanRuleEngine::builtinExceptions()
{
    return builtinTable();
}

void GermanRuleEngine::setGermanOptions(const Options& o)
{
    opts_ = o;
    reset();
}

void GermanRuleEngine::normaliseExceptions()
{
    for (auto& list : exceptions_) {
        for (auto& w : list)
            w = toLowerAsciiString(std::move(w));
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
}

void GermanRuleEngine::setExceptions(ExceptionTable table)
{
    exceptions_ = std::move(table);
    normaliseExceptions();
}

void GermanRuleEngine::addExceptions(GermanDigraph digraph, const std::vector<std::string>& words)
{
    auto& list = exceptions_[idx(digraph)];
    list.insert(list.end(), words.begin(), words.end());
    normaliseExceptions();
}

GermanRuleEngine::ExceptionTable GermanRuleEngine::parseExceptionText(std::string_view text)
{
    ExceptionTable table;
    std::size_t    pos = 0;
    while (pos <= text.size()) {
        const std::size_t eol = text.find('\n', pos);
        std::string_view line = text.substr(pos, eol == std::string_view::npos ? eol : eol - pos);
        pos = (eol == std::string_view::npos) ? text.size() + 1 : eol + 1;

        auto trim = [](std::string_view& s) {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
                s.remove_prefix(1);
            while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
                s.remove_suffix(1);
        };
        trim(line);
        if (line.empty() || line.front() == '#')
            continue;

        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos)
            continue;

        const std::string            key = toLowerAsciiString(std::string(line.substr(0, colon)));
        std::optional<GermanDigraph> digraph;
        if (key == "ae")      digraph = GermanDigraph::Ae;
        else if (key == "oe") digraph = GermanDigraph::Oe;
        else if (key == "ue") digraph = GermanDigraph::Ue;
        else if (key == "ss") digraph = GermanDigraph::Ss;
        if (!digraph)
            continue;

        std::string_view rest = line.substr(colon + 1);
        std::size_t      p    = 0;
        while (p <= rest.size()) {
            const std::size_t comma = rest.find(',', p);
            std::string_view  word =
                rest.substr(p, comma == std::string_view::npos ? comma : comma - p);
            p = (comma == std::string_view::npos) ? rest.size() + 1 : comma + 1;
            trim(word);
            if (!word.empty())
                table[idx(*digraph)].push_back(toLowerAsciiString(std::string(word)));
        }
    }
    for (auto& list : table) {
        std::sort(list.begin(), list.end());
        list.erase(std::unique(list.begin(), list.end()), list.end());
    }
    return table;
}

void GermanRuleEngine::reset() noexcept
{
    ascii_.clear();
    literal_.clear();
    emitted_.clear();
    escapeArmed_ = false;
    undoAt_.reset();
}

// ---------------------------------------------------------------------------

std::optional<GermanRuleEngine::Match> GermanRuleEngine::matchDigraph(char32_t first,
                                                                     char32_t second) const
{
    const char32_t f     = unicode::toLowerAscii(first);
    const char32_t s     = unicode::toLowerAscii(second);
    const bool     upper = unicode::isAsciiUpper(first);

    if (opts_.umlauts && s == U'e') {
        switch (f) {
        case U'a': return Match{GermanDigraph::Ae, upper ? U'Ä' : U'ä'};
        case U'o': return Match{GermanDigraph::Oe, upper ? U'Ö' : U'ö'};
        case U'u': return Match{GermanDigraph::Ue, upper ? U'Ü' : U'ü'};
        default:   break;
        }
    }
    if (opts_.sharpS && f == U's' && s == U's') {
        const bool bothUpper = upper && unicode::isAsciiUpper(second);
        return Match{GermanDigraph::Ss,
                     (opts_.capitalSharpS && bothUpper) ? kCapitalSharpS : kSmallSharpS};
    }
    return std::nullopt;
}

std::vector<bool> GermanRuleEngine::suppressedPositions() const
{
    std::vector<bool> suppressed(ascii_.size(), false);
    if (!opts_.useExceptions || ascii_.empty())
        return suppressed;

    const std::string typed = toLowerAsciiString(unicode::toUtf8(ascii_));

    for (std::size_t d = 0; d < static_cast<std::size_t>(GermanDigraph::Count); ++d) {
        const auto&            list     = exceptions_[d];
        const std::string_view spelling = digraphSpelling(static_cast<GermanDigraph>(d));
        if (list.empty() || spelling.size() != 2)
            continue;

        // Every candidate starts with the same letter as the typed word, so the
        // scan is confined to one short run of the sorted list.
        const std::string firstLetter(1, typed.front());
        auto              it = std::lower_bound(list.begin(), list.end(), firstLetter);
        for (; it != list.end() && !it->empty() && it->front() == typed.front(); ++it) {
            if (!related(*it, typed))
                continue;
            for (std::size_t i = 0; i + 1 < it->size(); ++i)
                if (it->compare(i, 2, spelling) == 0 && i + 1 < suppressed.size())
                    suppressed[i] = true;
        }
    }
    return suppressed;
}

GermanRuleEngine::Rendering GermanRuleEngine::recompose() const
{
    const std::vector<bool> suppressed = suppressedPositions();

    Rendering out;
    out.text.reserve(ascii_.size());
    out.width.reserve(ascii_.size());

    std::size_t i = 0;
    while (i < ascii_.size()) {
        const bool pinned = literal_[i] || (i + 1 < ascii_.size() && literal_[i + 1]);
        if (!pinned && !suppressed[i] && i + 1 < ascii_.size()) {
            if (const auto m = matchDigraph(ascii_[i], ascii_[i + 1])) {
                out.text.push_back(m->composed);
                out.width.push_back(2);
                i += 2;
                continue;
            }
        }
        out.text.push_back(ascii_[i]);
        out.width.push_back(1);
        ++i;
    }
    return out;
}

EngineResult GermanRuleEngine::emitDiff(const std::u32string& before, char32_t typedChar)
{
    const std::size_t common = unicode::commonPrefix(before, emitted_);

    EngineResult r;
    r.backspaces = static_cast<int>(before.size() - common);
    r.text       = emitted_.substr(common);

    // Fast path: the keystroke produces exactly the character the OS would have
    // typed anyway. Letting it through is lower latency and bypasses injection.
    if (r.backspaces == 0 && r.text.size() == 1 && r.text[0] == typedChar)
        return EngineResult::passthrough();

    r.handled = true;
    return r;
}

// ---------------------------------------------------------------------------

EngineResult GermanRuleEngine::processKey(const KeyEvent& event)
{
    if (event.isChord()) {
        reset();
        return EngineResult::passthrough();
    }

    switch (event.kind) {
    case KeyKind::Backspace: {
        if (ascii_.empty()) {
            reset();
            return EngineResult::passthrough();
        }
        const std::u32string before = emitted_;
        // Drop as many ASCII characters as the last rendered character stands
        // for, so one Backspace deletes one visible character.
        const Rendering current = recompose();
        std::size_t     drop    = current.width.empty() ? 1 : current.width.back();
        drop                    = std::min(drop, ascii_.size());
        ascii_.erase(ascii_.size() - drop);
        literal_.resize(literal_.size() - drop);
        undoAt_.reset();
        escapeArmed_ = false;
        emitted_     = recompose().text;

        if (emitted_.size() + 1 == before.size()
            && unicode::commonPrefix(before, emitted_) == emitted_.size())
            return EngineResult::passthrough();  // the OS Backspace already does this
        return emitDiff(before, 0);
    }

    case KeyKind::Modifier:
        return EngineResult::passthrough();

    case KeyKind::Character:
        break;

    default:  // Space, Enter, Tab, Escape, Navigation, Other -> word boundary
        reset();
        return EngineResult::passthrough();
    }

    const char32_t c = event.character;

    if (opts_.escapeKey != 0 && c == opts_.escapeKey && !escapeArmed_) {
        escapeArmed_ = true;
        undoAt_.reset();
        return EngineResult::swallow();  // the escape character is never typed
    }

    if (!isGermanWordChar(c)) {
        reset();
        return EngineResult::passthrough();
    }

    const std::u32string before = emitted_;
    const auto           undo   = undoAt_;
    undoAt_.reset();

    if (opts_.doubleKeyRevert && !escapeArmed_ && undo && *undo + 1 < ascii_.size()
        && unicode::toLowerAscii(c) == unicode::toLowerAscii(ascii_[*undo + 1])) {
        // Second press of the digraph's own second key: pin it as literal and
        // consume the keystroke. "ue" -> "ü", then "e" -> "ue".
        literal_[*undo]     = true;
        literal_[*undo + 1] = true;
    } else {
        ascii_.push_back(c);
        literal_.push_back(escapeArmed_);
        if (escapeArmed_) {
            escapeArmed_ = false;
            if (ascii_.size() >= 2)
                literal_[ascii_.size() - 2] = true;
        }
    }

    const Rendering rendering = recompose();
    emitted_                  = rendering.text;

    // Arm the undo when the keystroke just produced a two-character digraph.
    if (!rendering.width.empty() && rendering.width.back() == 2 && ascii_.size() >= 2)
        undoAt_ = ascii_.size() - 2;

    return emitDiff(before, c);
}

// ---------------------------------------------------------------------------

void GermanRuleEngine::applyOptions(const EngineOptions& options)
{
    auto get = [&](std::string_view key, bool fallback) {
        const auto it = options.find(key);
        return it == options.end() ? fallback : boolFromString(it->second, fallback);
    };
    opts_.umlauts         = get("umlauts", opts_.umlauts);
    opts_.sharpS          = get("sharpS", opts_.sharpS);
    opts_.capitalSharpS   = get("capitalSharpS", opts_.capitalSharpS);
    opts_.doubleKeyRevert = get("doubleKeyRevert", opts_.doubleKeyRevert);
    opts_.useExceptions   = get("useExceptions", opts_.useExceptions);

    if (const auto it = options.find("escapeKey"); it != options.end()) {
        const std::u32string s = unicode::fromUtf8(it->second);
        opts_.escapeKey        = s.empty() ? 0 : s.front();
    }
    reset();
}

EngineOptions GermanRuleEngine::options() const
{
    EngineOptions o;
    o["umlauts"]         = opts_.umlauts ? "true" : "false";
    o["sharpS"]          = opts_.sharpS ? "true" : "false";
    o["capitalSharpS"]   = opts_.capitalSharpS ? "true" : "false";
    o["doubleKeyRevert"] = opts_.doubleKeyRevert ? "true" : "false";
    o["useExceptions"]   = opts_.useExceptions ? "true" : "false";
    o["escapeKey"]       = opts_.escapeKey ? unicode::toUtf8(std::u32string{opts_.escapeKey}) : "";
    return o;
}

} // namespace st
