// SPDX-License-Identifier: MIT
//
// Vietnamese orthography tables: composition, decomposition and syllable
// validation. Clean-room implementation — no code or data is taken from UniKey;
// what is reproduced is the behaviour of Vietnamese writing itself.
//
// Output is always precomposed Unicode (NFC), which is what every modern
// toolkit on Windows, macOS and Linux renders correctly.

#pragma once

#include "core/Unicode.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace st::vn {

enum class Tone : std::uint8_t {
    None  = 0,
    Acute = 1,  ///< sắc
    Grave = 2,  ///< huyền
    Hook  = 3,  ///< hỏi
    Tilde = 4,  ///< ngã
    Dot   = 5,  ///< nặng
};

enum class Mark : std::uint8_t {
    None       = 0,
    Circumflex,  ///< â ê ô
    Breve,       ///< ă
    Horn,        ///< ơ ư
    Stroke,      ///< đ
};

/// One letter of a syllable, kept in decomposed form while the user types.
struct Letter {
    char32_t base  = 0;      ///< lowercase ASCII base letter
    Mark     mark  = Mark::None;
    bool     upper = false;

    friend bool operator==(const Letter& a, const Letter& b) noexcept
    {
        return a.base == b.base && a.mark == b.mark && a.upper == b.upper;
    }
};

[[nodiscard]] bool isVowelBase(char32_t lowerBase) noexcept;

/// True when `mark` is orthographically possible on `base` (â yes, ê yes,
/// ă only on a, horn only on o/u, stroke only on d).
[[nodiscard]] bool markAllowed(char32_t lowerBase, Mark mark) noexcept;

/// Compose one Unicode code point. Returns 0 when the combination does not
/// exist (e.g. a tone on a consonant).
[[nodiscard]] char32_t compose(char32_t lowerBase, Mark mark, Tone tone, bool upper) noexcept;

/// Inverse of compose(), for the case where the user pastes or types a
/// precomposed Vietnamese character with a national keyboard layout.
struct Decomposed {
    Letter letter;
    Tone   tone = Tone::None;
};
[[nodiscard]] std::optional<Decomposed> decompose(char32_t codepoint) noexcept;

// ---------------------------------------------------------------------------
// Syllable structure
// ---------------------------------------------------------------------------

/// Split of a syllable into onset (initial consonants), nucleus (vowel run) and
/// coda (final consonants). `nucleusBegin`/`nucleusEnd` are indices into the
/// letter vector; `nucleusEnd` is one past the last vowel.
struct SyllableParts {
    std::size_t nucleusBegin = 0;
    std::size_t nucleusEnd   = 0;
    bool        hasVowel     = false;
    /// Index of the first nucleus letter that may carry the tone. Differs from
    /// nucleusBegin for the "qu" and "gi" digraphs, whose u/i belong to the
    /// onset rather than to the nucleus.
    std::size_t toneScanBegin = 0;
};

[[nodiscard]] SyllableParts split(const std::vector<Letter>& letters) noexcept;

/// Index of the letter that carries the tone mark, or SIZE_MAX when the
/// syllable has no vowel yet.
///
/// `modernStyle` selects where the tone goes on the open clusters oa / oe / uy:
/// false gives the traditional hòa / thùy, true gives the "new" hoà / thuỳ.
[[nodiscard]] std::size_t toneTargetIndex(const std::vector<Letter>& letters,
                                          bool                       modernStyle) noexcept;

/// Validity of a (possibly still incomplete) syllable.
///
/// `partial == true` accepts anything that could still grow into a legal
/// syllable — that is the check used while the user is typing, so the engine
/// only refuses a diacritic when it is already certain the word is not
/// Vietnamese. `partial == false` is the stricter end-of-word check that drives
/// auto-restore.
[[nodiscard]] bool isValidSyllable(const std::vector<Letter>& letters, Tone tone,
                                   bool partial) noexcept;

/// Render a letter vector plus its tone into precomposed Unicode.
[[nodiscard]] std::u32string render(const std::vector<Letter>& letters, Tone tone,
                                    bool modernStyle);

} // namespace st::vn
