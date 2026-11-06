// SPDX-License-Identifier: MIT
#pragma once

#include "core/german/GermanRuleEngine.hpp"
#include "core/vietnamese/VietnameseRuleEngine.hpp"

#include <set>
#include <vector>

namespace st {

/// Offline word-level selection. Only the current word may be revised; a
/// separator commits it. Unknown and ambiguous words favour literal input.
class AutoRuleEngine final : public LanguageRuleEngine {
public:
    AutoRuleEngine();
    [[nodiscard]] std::string id() const override { return "auto"; }
    [[nodiscard]] std::string displayName() const override { return "Automatic"; }
    [[nodiscard]] std::string badge() const override { return "AUTO"; }

    void reset() noexcept override;
    EngineResult processKey(const KeyEvent& event) override;
    void applyOptions(const EngineOptions& options) override;
    [[nodiscard]] EngineOptions options() const override { return options_; }
    void setGermanExceptions(GermanRuleEngine::ExceptionTable exceptions);
    /// Replace visible text during native caret editing; prior raw keys remain undoable.
    EngineResult editWord(std::u32string text, std::size_t cursor, std::size_t previousCursor);
    [[nodiscard]] std::u32string literalWord() const;
    [[nodiscard]] const std::u32string& currentText() const noexcept { return emitted_; }
    [[nodiscard]] bool vietnameseOverride() const noexcept { return forcedVietnamese_; }

private:
    enum class Language { Literal, English, Vietnamese, German };
    struct Candidate {
        std::u32string text;
        Language language = Language::Literal;
        int score = 0;
    };

    Candidate choose(bool final);
    EngineResult replace(std::u32string text, char32_t natural = 0);
    void clearWord() noexcept;
    void remember(std::size_t cursor);
    struct Snapshot {
        std::u32string raw;
        std::u32string emitted;
        std::size_t cursor;
        bool literal, forcedVietnamese, vietnameseActive, learnable;
    };

    VietnameseRuleEngine vietnamese_;
    GermanRuleEngine german_;
    EngineOptions options_;
    std::set<std::u32string> literalWords_;
    std::u32string raw_;
    std::u32string emitted_;
    Language context_ = Language::Literal;
    bool literal_ = false;
    bool overflow_ = false;
    bool forcedVietnamese_ = false;
    bool vietnameseActive_ = false;
    bool learnable_ = true;
    std::vector<Snapshot> history_;
};

} // namespace st
