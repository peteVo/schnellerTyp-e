// SPDX-License-Identifier: MIT
//
// Data-driven rule engine.
//
// This is how a new language or a set of text expansions is added without
// writing C++: describe the substitutions in JSON, drop the file in the user
// rules directory, and the app offers it in the language selector alongside the
// built-in German and Vietnamese engines. See config/custom-rules.example.json.
//
// The matching model is deliberately the same one the German engine uses, so
// the behaviour is predictable across the app:
//   * the longest trigger matching the tail of the current word wins;
//   * a trigger only fires on characters that are not "locked";
//   * pressing the last character of the trigger again undoes the substitution
//     and locks the result (when `doubleKeyRevert` is on).

#pragma once

#include "core/LanguageRuleEngine.hpp"
#include "core/WordBuffer.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace st {

struct SubstitutionRule {
    std::u32string trigger;        ///< keystrokes to match, e.g. "e'" or "btw"
    std::u32string replacement;    ///< text to produce, e.g. "é"
    bool           caseSensitive = false;
    bool           smartCase     = true;  ///< "Btw" -> "By the way", "BTW" -> "BY THE WAY"
    bool           wordStart     = false; ///< only fire at the start of a word
};

struct RuleSetDefinition {
    std::string                   id;           ///< "custom:fr"
    std::string                   displayName;  ///< "French accents"
    std::string                   badge;        ///< "FR"
    std::vector<SubstitutionRule> rules;
    bool                          doubleKeyRevert = true;
    char32_t                      escapeKey       = U'\\';
};

class RuleSetEngine final : public LanguageRuleEngine {
public:
    explicit RuleSetEngine(RuleSetDefinition definition);
    ~RuleSetEngine() override;

    [[nodiscard]] std::string id() const override { return def_.id; }
    [[nodiscard]] std::string displayName() const override { return def_.displayName; }
    [[nodiscard]] std::string badge() const override { return def_.badge; }

    void         reset() noexcept override;
    EngineResult processKey(const KeyEvent& event) override;

    void                        applyOptions(const EngineOptions& options) override;
    [[nodiscard]] EngineOptions options() const override;

    [[nodiscard]] const RuleSetDefinition& definition() const noexcept { return def_; }

private:
    struct Applied {
        std::u32string original;
        std::u32string produced;
        char32_t       undoKey;
    };

    [[nodiscard]] const SubstitutionRule* longestMatch(std::size_t* matchedLength) const;
    [[nodiscard]] std::u32string          applyCase(const SubstitutionRule& rule,
                                                    std::u32string_view     typed) const;

    RuleSetDefinition      def_;
    WordBuffer             buf_;
    bool                   escapeArmed_ = false;
    std::optional<Applied> pendingUndo_;
};

} // namespace st
