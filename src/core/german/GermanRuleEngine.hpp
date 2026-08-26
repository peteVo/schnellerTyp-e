// SPDX-License-Identifier: MIT
//
// German digraph engine: ae -> ä, oe -> ö, ue -> ü, ss -> ß.
//
// The mapping is trivial; knowing when *not* to apply it is not. "Dauer",
// "Duell", "Wasser", "essen" and "aussehen" all contain a trigger digraph that
// must stay literal, while "düster", "außen" and "müssen" — which share their
// first letters with those words — must not.
//
// The engine therefore does not decide once and commit. It keeps the plain
// ASCII the user typed, re-derives the whole word after every keystroke, and
// emits the difference. A digraph is left literal while the word so far is
// related to an entry in the exception dictionary — "related" meaning either
// side is a prefix of the other — and the moment the word diverges from every
// entry, the transformation reappears. So:
//
//     d u e            -> "due"      (could still be "duell")
//     d u e s          -> "düs"      (no longer can be)
//     d u e s t e r    -> "düster"
//     d u e l l        -> "duell"
//
// Two further escape hatches, neither of them timing-dependent:
//
//   * double-key undo — UniKey's convention. `ue` gives `ü`; pressing `e` once
//     more gives back `ue` and pins it, so "Dauer" can also be typed d-a-u-e-e-r
//     even with the dictionary switched off.
//   * escape key — `\` by default. The next keystroke is taken literally and
//     the backslash itself is swallowed: "Mu\uesli".
//
// Because the engine is a pure function of the keystroke sequence, fast typing
// and mixed casing are safe by construction: there is no timer to lose a race
// against, and the hook layer delivers keystrokes strictly in order.

#pragma once

#include "core/LanguageRuleEngine.hpp"
#include "core/Unicode.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace st {

/// The four transliteration digraphs, used to index the exception dictionary.
enum class GermanDigraph : std::uint8_t { Ae = 0, Oe = 1, Ue = 2, Ss = 3, Count = 4 };

class GermanRuleEngine final : public LanguageRuleEngine {
public:
    struct Options {
        bool     umlauts         = true;   ///< ae/oe/ue -> ä/ö/ü
        bool     sharpS          = true;   ///< ss -> ß
        bool     capitalSharpS   = false;  ///< SS -> ẞ (U+1E9E) instead of ß
        bool     doubleKeyRevert = true;   ///< uee -> ue
        bool     useExceptions   = true;   ///< consult the exception dictionary
        char32_t escapeKey       = U'\\';  ///< 0 disables the escape key
    };

    /// One word list per digraph, in the spelling the user types. Kept per
    /// digraph on purpose: "muessen" must block `ss` and still allow `ue`.
    using ExceptionTable =
        std::array<std::vector<std::string>, static_cast<std::size_t>(GermanDigraph::Count)>;

    GermanRuleEngine();
    explicit GermanRuleEngine(Options options);
    ~GermanRuleEngine() override;

    [[nodiscard]] std::string id() const override { return "de"; }
    [[nodiscard]] std::string displayName() const override { return "German (Umlaut)"; }
    [[nodiscard]] std::string badge() const override { return "DE"; }

    void         reset() noexcept override;
    EngineResult processKey(const KeyEvent& event) override;

    void                        applyOptions(const EngineOptions& options) override;
    [[nodiscard]] EngineOptions options() const override;

    [[nodiscard]] const Options& germanOptions() const noexcept { return opts_; }
    void                         setGermanOptions(const Options& o);

    void setExceptions(ExceptionTable table);
    void addExceptions(GermanDigraph digraph, const std::vector<std::string>& words);

    [[nodiscard]] const ExceptionTable&        exceptions() const noexcept { return exceptions_; }
    [[nodiscard]] static const ExceptionTable& builtinExceptions();

    /// Parse the "digraph: word, word, ..." format of
    /// config/german-exceptions.txt. Unknown digraph names and comment lines
    /// (starting with '#') are ignored.
    [[nodiscard]] static ExceptionTable parseExceptionText(std::string_view text);

    /// Current composed word — exposed for tests and the diagnostics pane.
    [[nodiscard]] const std::u32string& composed() const noexcept { return emitted_; }

private:
    struct Match {
        GermanDigraph digraph{};
        char32_t      composed = 0;
    };

    struct Rendering {
        std::u32string            text;
        std::vector<std::uint8_t> width;  ///< ASCII characters behind each output char
    };

    [[nodiscard]] std::optional<Match> matchDigraph(char32_t first, char32_t second) const;
    [[nodiscard]] std::vector<bool>    suppressedPositions() const;
    [[nodiscard]] Rendering            recompose() const;
    void                               normaliseExceptions();
    EngineResult                       emitDiff(const std::u32string& before, char32_t typedChar);

    Options opts_{};

    std::u32string    ascii_;    ///< the literal characters of the current word
    std::vector<bool> literal_;  ///< positions pinned by an undo or an escape
    std::u32string    emitted_;  ///< what we believe is on screen

    bool escapeArmed_ = false;
    /// Start index in ascii_ of the digraph the next keystroke can undo, valid
    /// only for the keystroke immediately following the transformation.
    std::optional<std::size_t> undoAt_;

    ExceptionTable exceptions_{};
};

} // namespace st
