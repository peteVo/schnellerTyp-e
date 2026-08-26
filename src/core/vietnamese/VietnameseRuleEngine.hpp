// SPDX-License-Identifier: MIT
//
// Vietnamese input engine — Telex and VNI.
//
// Clean-room implementation. It reproduces the behaviour a UniKey user expects
// but shares no code or data with UniKey:
//
//   * free tone marking  — the tone key may come anywhere after the vowel, so
//                          both "toasn" and "toans" give "toán";
//   * correct tone placement, with the traditional/modern choice for the open
//     clusters oa / oe / uy (hòa vs hoà);
//   * "qu" and "gi" treated as onset digraphs (quá, giữ — never qùa, gìư);
//   * double-key undo — "as" -> "á", "ass" -> "as"; "aa" -> "â", "aaa" -> "aa";
//   * spelling check   — a diacritic that would produce an impossible Vietnamese
//                        syllable is refused and the key is taken literally;
//   * auto-restore     — a word that ends up not being Vietnamese is rewritten
//                        with the original keystrokes when the word ends, so
//                        typing "away" in Telex does not leave "ăway" behind.
//
// The engine keeps the syllable decomposed (base letter + mark, plus one tone
// for the whole syllable) and re-renders it after every keystroke. The visible
// edit is then the diff between the previous rendering and the new one, which
// is what makes free marking and tone migration fall out for free.

#pragma once

#include "core/LanguageRuleEngine.hpp"
#include "core/vietnamese/VnTables.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace st {

enum class VnInputMethod : std::uint8_t { Telex, Vni };

class VietnameseRuleEngine final : public LanguageRuleEngine {
public:
    struct Options {
        VnInputMethod method      = VnInputMethod::Telex;
        bool          modernTone  = false;  ///< true: hoà / thuỳ. false: hòa / thùy.
        bool          spellCheck  = true;   ///< refuse diacritics that break the syllable
        bool          autoRestore = true;   ///< rewrite non-Vietnamese words on word end
    };

    VietnameseRuleEngine();
    explicit VietnameseRuleEngine(Options options);
    ~VietnameseRuleEngine() override;

    [[nodiscard]] std::string id() const override { return "vi"; }
    [[nodiscard]] std::string displayName() const override { return "Vietnamese"; }
    [[nodiscard]] std::string badge() const override { return "VN"; }

    void         reset() noexcept override;
    EngineResult processKey(const KeyEvent& event) override;

    void                        applyOptions(const EngineOptions& options) override;
    [[nodiscard]] EngineOptions options() const override;

    [[nodiscard]] const Options& vnOptions() const noexcept { return opts_; }
    void                         setVnOptions(const Options& o);

    /// Current composed word — exposed for tests and for the diagnostics pane.
    [[nodiscard]] const std::u32string& composed() const noexcept { return emitted_; }

private:
    static constexpr std::size_t kNoBlock = static_cast<std::size_t>(-1);

    // --- key classification -------------------------------------------------
    [[nodiscard]] std::optional<vn::Tone> toneKeyFor(char32_t lower) const noexcept;
    [[nodiscard]] std::optional<vn::Mark> markKeyFor(char32_t lower) const noexcept;
    [[nodiscard]] bool                    isModifierKey(char32_t lower) const noexcept;

    // --- transformations ----------------------------------------------------
    bool applyTone(vn::Tone tone, char32_t key);
    bool applyMark(vn::Mark mark, char32_t key);
    void appendLiteral(char32_t c);

    /// Indices in the nucleus the given mark should be written on, honouring the
    /// "uo -> ươ" pair rule. Empty when the mark cannot be placed.
    [[nodiscard]] std::vector<std::size_t> markTargets(vn::Mark mark) const;

    // --- flow ---------------------------------------------------------------
    EngineResult finish(const std::u32string& before, char32_t typedChar);
    EngineResult handleBackspace();
    EngineResult handleBoundary(const KeyEvent& event);
    [[nodiscard]] bool shouldRestore() const;

    Options opts_{};

    std::vector<vn::Letter> letters_;
    vn::Tone                tone_ = vn::Tone::None;
    std::u32string          raw_;      ///< every keystroke of the current word
    std::u32string          emitted_;  ///< what we believe is on screen

    char32_t    toneKeyUsed_ = 0;        ///< key that set tone_, for double-key undo
    char32_t    markKeyUsed_ = 0;        ///< key that set the last mark
    std::size_t toneBlockAt_ = kNoBlock; ///< tone keys are literal at this length
    std::size_t markBlockAt_ = kNoBlock; ///< mark keys are literal at this length
    bool        rawDirty_    = false;    ///< a Backspace made raw_ unreliable
};

} // namespace st
