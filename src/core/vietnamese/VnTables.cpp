// SPDX-License-Identifier: MIT
#include "core/vietnamese/VnTables.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>

namespace st::vn {
namespace {

// ---------------------------------------------------------------------------
// Composition table
// ---------------------------------------------------------------------------
//
// Layout: for each vowel base, a contiguous block of (diacritic x tone) pairs.
//   a : none, breve, circumflex   (3 diacritics)
//   e : none, circumflex          (2)
//   i : none                      (1)
//   o : none, circumflex, horn    (3)
//   u : none, horn                (2)
//   y : none                      (1)
// Six tones each, in Tone order: none, acute, grave, hook, tilde, dot.
//
// Generated from the Unicode NFC of base + combining mark, then frozen here so
// the binary needs no ICU and no normalisation pass at runtime.

struct Cell {
    char32_t lower;
    char32_t upper;
};

// clang-format off
constexpr std::array<Cell, 72> kCells = {{
    // a --------------------------------------------------------------------
    {0x0061, 0x0041}, {0x00E1, 0x00C1}, {0x00E0, 0x00C0}, {0x1EA3, 0x1EA2}, {0x00E3, 0x00C3}, {0x1EA1, 0x1EA0}, // a á à ả ã ạ
    {0x0103, 0x0102}, {0x1EAF, 0x1EAE}, {0x1EB1, 0x1EB0}, {0x1EB3, 0x1EB2}, {0x1EB5, 0x1EB4}, {0x1EB7, 0x1EB6}, // ă ắ ằ ẳ ẵ ặ
    {0x00E2, 0x00C2}, {0x1EA5, 0x1EA4}, {0x1EA7, 0x1EA6}, {0x1EA9, 0x1EA8}, {0x1EAB, 0x1EAA}, {0x1EAD, 0x1EAC}, // â ấ ầ ẩ ẫ ậ
    // e --------------------------------------------------------------------
    {0x0065, 0x0045}, {0x00E9, 0x00C9}, {0x00E8, 0x00C8}, {0x1EBB, 0x1EBA}, {0x1EBD, 0x1EBC}, {0x1EB9, 0x1EB8}, // e é è ẻ ẽ ẹ
    {0x00EA, 0x00CA}, {0x1EBF, 0x1EBE}, {0x1EC1, 0x1EC0}, {0x1EC3, 0x1EC2}, {0x1EC5, 0x1EC4}, {0x1EC7, 0x1EC6}, // ê ế ề ể ễ ệ
    // i --------------------------------------------------------------------
    {0x0069, 0x0049}, {0x00ED, 0x00CD}, {0x00EC, 0x00CC}, {0x1EC9, 0x1EC8}, {0x0129, 0x0128}, {0x1ECB, 0x1ECA}, // i í ì ỉ ĩ ị
    // o --------------------------------------------------------------------
    {0x006F, 0x004F}, {0x00F3, 0x00D3}, {0x00F2, 0x00D2}, {0x1ECF, 0x1ECE}, {0x00F5, 0x00D5}, {0x1ECD, 0x1ECC}, // o ó ò ỏ õ ọ
    {0x00F4, 0x00D4}, {0x1ED1, 0x1ED0}, {0x1ED3, 0x1ED2}, {0x1ED5, 0x1ED4}, {0x1ED7, 0x1ED6}, {0x1ED9, 0x1ED8}, // ô ố ồ ổ ỗ ộ
    {0x01A1, 0x01A0}, {0x1EDB, 0x1EDA}, {0x1EDD, 0x1EDC}, {0x1EDF, 0x1EDE}, {0x1EE1, 0x1EE0}, {0x1EE3, 0x1EE2}, // ơ ớ ờ ở ỡ ợ
    // u --------------------------------------------------------------------
    {0x0075, 0x0055}, {0x00FA, 0x00DA}, {0x00F9, 0x00D9}, {0x1EE7, 0x1EE6}, {0x0169, 0x0168}, {0x1EE5, 0x1EE4}, // u ú ù ủ ũ ụ
    {0x01B0, 0x01AF}, {0x1EE9, 0x1EE8}, {0x1EEB, 0x1EEA}, {0x1EED, 0x1EEC}, {0x1EEF, 0x1EEE}, {0x1EF1, 0x1EF0}, // ư ứ ừ ử ữ ự
    // y --------------------------------------------------------------------
    {0x0079, 0x0059}, {0x00FD, 0x00DD}, {0x1EF3, 0x1EF2}, {0x1EF7, 0x1EF6}, {0x1EF9, 0x1EF8}, {0x1EF5, 0x1EF4}, // y ý ỳ ỷ ỹ ỵ
}};
// clang-format on

constexpr char32_t kDLower = 0x0064, kDUpper = 0x0044;  // d D
constexpr char32_t kDStrokeLower = 0x0111, kDStrokeUpper = 0x0110;  // đ Đ

/// Block offset and diacritic count for each vowel base.
struct BaseInfo {
    char32_t    base;
    std::size_t offset;
    std::size_t diacritics;
};

constexpr std::array<BaseInfo, 6> kBases = {{
    {U'a', 0, 3},
    {U'e', 18, 2},
    {U'i', 30, 1},
    {U'o', 36, 3},
    {U'u', 54, 2},
    {U'y', 66, 1},
}};

[[nodiscard]] const BaseInfo* baseInfo(char32_t lowerBase) noexcept
{
    for (const auto& b : kBases)
        if (b.base == lowerBase)
            return &b;
    return nullptr;
}

/// Diacritic slot inside a base's block, or -1 when the mark is impossible.
[[nodiscard]] int diacriticIndex(char32_t lowerBase, Mark mark) noexcept
{
    if (mark == Mark::None)
        return 0;
    switch (lowerBase) {
    case U'a': return mark == Mark::Breve ? 1 : (mark == Mark::Circumflex ? 2 : -1);
    case U'e': return mark == Mark::Circumflex ? 1 : -1;
    case U'o': return mark == Mark::Circumflex ? 1 : (mark == Mark::Horn ? 2 : -1);
    case U'u': return mark == Mark::Horn ? 1 : -1;
    default:   return -1;
    }
}

// ---------------------------------------------------------------------------
// Reverse lookup, built once on first use.
// ---------------------------------------------------------------------------

struct ReverseEntry {
    Letter letter;
    Tone   tone;
};

const std::unordered_map<char32_t, ReverseEntry>& reverseTable()
{
    static const std::unordered_map<char32_t, ReverseEntry> table = [] {
        std::unordered_map<char32_t, ReverseEntry> m;
        m.reserve(160);
        for (const auto& b : kBases) {
            for (std::size_t d = 0; d < b.diacritics; ++d) {
                Mark mark = Mark::None;
                if (d == 1)
                    mark = (b.base == U'a') ? Mark::Breve
                                            : ((b.base == U'o' || b.base == U'e') ? Mark::Circumflex
                                                                                  : Mark::Horn);
                else if (d == 2)
                    mark = (b.base == U'a') ? Mark::Circumflex : Mark::Horn;

                for (std::size_t t = 0; t < 6; ++t) {
                    const Cell& c = kCells[b.offset + d * 6 + t];
                    m.emplace(c.lower, ReverseEntry{Letter{b.base, mark, false}, static_cast<Tone>(t)});
                    m.emplace(c.upper, ReverseEntry{Letter{b.base, mark, true}, static_cast<Tone>(t)});
                }
            }
        }
        m.emplace(kDStrokeLower, ReverseEntry{Letter{U'd', Mark::Stroke, false}, Tone::None});
        m.emplace(kDStrokeUpper, ReverseEntry{Letter{U'd', Mark::Stroke, true}, Tone::None});
        return m;
    }();
    return table;
}

// ---------------------------------------------------------------------------
// Orthography tables for validation
// ---------------------------------------------------------------------------

/// Onsets, written with the letters the user actually types ("đ" appears as
/// "dd" nowhere — it is normalised to "d" plus Mark::Stroke, spelled "D" here).
const std::vector<std::string>& onsets()
{
    static const std::vector<std::string> v = [] {
        std::vector<std::string> s = {"",  "b",  "c",  "ch", "d",  "D",  "g",  "gh", "gi",
                                      "h", "k",  "kh", "l",  "m",  "n",  "ng", "ngh", "nh",
                                      "p", "ph", "qu", "r",  "s",  "t",  "th", "tr",  "v", "x"};
        std::sort(s.begin(), s.end());
        return s;
    }();
    return v;
}

const std::vector<std::string>& codas()
{
    static const std::vector<std::string> v = [] {
        std::vector<std::string> s = {"", "c", "ch", "m", "n", "ng", "nh", "p", "t"};
        std::sort(s.begin(), s.end());
        return s;
    }();
    return v;
}

/// Nuclei written in composed, toneless form.
const std::vector<std::u32string>& nuclei()
{
    static const std::vector<std::u32string> v = [] {
        static const char* const utf8[] = {
            // one vowel
            "a", "ă", "â", "e", "ê", "i", "o", "ô", "ơ", "u", "ư", "y",
            // two vowels
            "ai", "ao", "au", "ay", "âu", "ây", "eo", "êu", "ia", "iê", "iu",
            "oa", "oă", "oe", "oi", "oo", "ôi", "ơi", "ua", "uâ", "uê", "ui",
            "uô", "uơ", "uy", "ưa", "ưi", "ươ", "ưu", "yê",
            // three vowels
            "iêu", "oai", "oao", "oay", "oeo", "uao", "uây", "uôi", "uya",
            "uyê", "uyu", "ươi", "ươu", "yêu",
        };
        std::vector<std::u32string> s;
        s.reserve(std::size(utf8));
        for (const char* u : utf8)
            s.push_back(unicode::fromUtf8(u));
        std::sort(s.begin(), s.end());
        return s;
    }();
    return v;
}

template <typename Container, typename Value>
[[nodiscard]] bool containsExact(const Container& sorted, const Value& v)
{
    return std::binary_search(sorted.begin(), sorted.end(), v);
}

template <typename Container, typename Value>
[[nodiscard]] bool containsPrefix(const Container& sorted, const Value& v)
{
    if (v.empty())
        return true;
    const auto it = std::lower_bound(sorted.begin(), sorted.end(), v);
    return it != sorted.end() && it->size() >= v.size()
           && std::equal(v.begin(), v.end(), it->begin());
}

/// Consonant letters spelled out for onset/coda matching. đ is spelled "D" so
/// it cannot be confused with plain d.
[[nodiscard]] std::string consonantString(const std::vector<Letter>& letters, std::size_t from,
                                          std::size_t to)
{
    std::string s;
    s.reserve(to - from);
    for (std::size_t i = from; i < to && i < letters.size(); ++i) {
        const Letter& l = letters[i];
        s.push_back(l.mark == Mark::Stroke ? 'D' : static_cast<char>(l.base));
    }
    return s;
}

} // namespace

// ---------------------------------------------------------------------------

bool isVowelBase(char32_t lowerBase) noexcept { return baseInfo(lowerBase) != nullptr; }

bool markAllowed(char32_t lowerBase, Mark mark) noexcept
{
    if (mark == Mark::Stroke)
        return lowerBase == U'd';
    return diacriticIndex(lowerBase, mark) >= 0;
}

char32_t compose(char32_t lowerBase, Mark mark, Tone tone, bool upper) noexcept
{
    if (lowerBase == U'd') {
        if (mark == Mark::Stroke)
            return upper ? kDStrokeUpper : kDStrokeLower;
        if (mark == Mark::None)
            return upper ? kDUpper : kDLower;
        return 0;
    }
    const BaseInfo* info = baseInfo(lowerBase);
    if (info == nullptr) {
        // Any other consonant: no marks, no tones.
        if (mark != Mark::None || tone != Tone::None)
            return 0;
        return upper ? unicode::toUpperAscii(lowerBase) : lowerBase;
    }
    const int d = diacriticIndex(lowerBase, mark);
    if (d < 0 || static_cast<std::size_t>(d) >= info->diacritics)
        return 0;
    const Cell& c = kCells[info->offset + static_cast<std::size_t>(d) * 6
                           + static_cast<std::size_t>(tone)];
    return upper ? c.upper : c.lower;
}

std::optional<Decomposed> decompose(char32_t codepoint) noexcept
{
    const auto& t  = reverseTable();
    const auto  it = t.find(codepoint);
    if (it != t.end())
        return Decomposed{it->second.letter, it->second.tone};
    if (unicode::isAsciiAlpha(codepoint)) {
        Letter l;
        l.base  = unicode::toLowerAscii(codepoint);
        l.upper = unicode::isAsciiUpper(codepoint);
        return Decomposed{l, Tone::None};
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------

SyllableParts split(const std::vector<Letter>& letters) noexcept
{
    SyllableParts p;
    std::size_t   i = 0;
    while (i < letters.size() && !isVowelBase(letters[i].base))
        ++i;
    p.nucleusBegin = i;
    while (i < letters.size() && isVowelBase(letters[i].base))
        ++i;
    p.nucleusEnd = i;
    p.hasVowel   = p.nucleusEnd > p.nucleusBegin;

    p.toneScanBegin = p.nucleusBegin;
    if (p.hasVowel && p.nucleusBegin > 0 && (p.nucleusEnd - p.nucleusBegin) > 1) {
        const char32_t prev  = letters[p.nucleusBegin - 1].base;
        const Letter&  first = letters[p.nucleusBegin];
        // "qu" and "gi" are onset digraphs: their u / i is not part of the
        // nucleus and must never take the tone (quá, giữ — not qùa, gìư).
        if ((prev == U'q' && first.base == U'u' && first.mark == Mark::None)
            || (prev == U'g' && first.base == U'i' && first.mark == Mark::None))
            p.toneScanBegin = p.nucleusBegin + 1;
    }
    return p;
}

std::size_t toneTargetIndex(const std::vector<Letter>& letters, bool modernStyle) noexcept
{
    const SyllableParts p = split(letters);
    if (!p.hasVowel)
        return static_cast<std::size_t>(-1);

    const std::size_t begin = p.toneScanBegin;
    const std::size_t end   = p.nucleusEnd;
    if (begin >= end)
        return p.nucleusBegin;  // bare "qu"/"gi": park the tone on the u/i

    const std::size_t count = end - begin;
    if (count == 1)
        return begin;

    // 1. A vowel that already carries a diacritic wins; with two of them
    //    (ươ) the later one is correct.
    std::size_t marked = static_cast<std::size_t>(-1);
    for (std::size_t i = begin; i < end; ++i)
        if (letters[i].mark != Mark::None)
            marked = i;
    if (marked != static_cast<std::size_t>(-1))
        return marked;

    // 2. Closed syllable: the tone sits on the last vowel (toán, hoàng).
    const bool hasCoda = end < letters.size();
    if (hasCoda)
        return end - 1;

    // 3. Open syllable.
    if (count >= 3)
        return begin + 1;  // hoài, ngoáy, khuỷu

    if (modernStyle) {
        const char32_t a = letters[begin].base;
        const char32_t b = letters[begin + 1].base;
        const bool     shifts =
            (a == U'o' && (b == U'a' || b == U'e')) || (a == U'u' && b == U'y');
        if (shifts)
            return begin + 1;  // hoà, thuỳ
    }
    return begin;  // hòa, thùy, mùa, mía, tài, kéo, núi
}

// ---------------------------------------------------------------------------

bool isValidSyllable(const std::vector<Letter>& letters, Tone tone, bool partial) noexcept
{
    if (letters.empty())
        return true;

    const SyllableParts p = split(letters);

    // Onset ------------------------------------------------------------------
    std::size_t onsetEnd = p.nucleusBegin;
    std::string onset    = consonantString(letters, 0, onsetEnd);
    // Fold the "qu" / "gi" digraphs into the onset for validation purposes.
    if (p.toneScanBegin > p.nucleusBegin) {
        onset.push_back(static_cast<char>(letters[p.nucleusBegin].base));
        onsetEnd = p.nucleusBegin + 1;
    }
    if (!containsExact(onsets(), onset) && !(partial && containsPrefix(onsets(), onset)))
        return false;

    if (!p.hasVowel)
        return partial;  // "ngh" is fine mid-word, not as a finished syllable

    // Nucleus ----------------------------------------------------------------
    std::u32string nucleus;
    for (std::size_t i = onsetEnd; i < p.nucleusEnd; ++i)
        nucleus.push_back(compose(letters[i].base, letters[i].mark, Tone::None, false));
    if (nucleus.empty())
        return partial;

    if (!containsExact(nuclei(), nucleus) && !(partial && containsPrefix(nuclei(), nucleus)))
        return false;

    // Coda -------------------------------------------------------------------
    const std::string coda = consonantString(letters, p.nucleusEnd, letters.size());
    if (!containsExact(codas(), coda) && !(partial && containsPrefix(codas(), coda)))
        return false;

    // Tone / coda agreement: a syllable closed by a stop (c, ch, p, t) can only
    // carry the acute or the dot tone (sắc / nặng).
    if (!coda.empty() && tone != Tone::None) {
        const bool stop = coda == "c" || coda == "ch" || coda == "p" || coda == "t";
        if (stop && tone != Tone::Acute && tone != Tone::Dot)
            return false;
    }
    return true;
}

std::u32string render(const std::vector<Letter>& letters, Tone tone, bool modernStyle)
{
    const std::size_t target = (tone == Tone::None)
                                   ? static_cast<std::size_t>(-1)
                                   : toneTargetIndex(letters, modernStyle);
    std::u32string out;
    out.reserve(letters.size());
    for (std::size_t i = 0; i < letters.size(); ++i) {
        const Letter& l  = letters[i];
        const Tone    t  = (i == target) ? tone : Tone::None;
        char32_t      cp = compose(l.base, l.mark, t, l.upper);
        if (cp == 0)
            cp = compose(l.base, l.mark, Tone::None, l.upper);
        if (cp == 0)
            cp = l.upper ? unicode::toUpperAscii(l.base) : l.base;
        out.push_back(cp);
    }
    return out;
}

} // namespace st::vn
