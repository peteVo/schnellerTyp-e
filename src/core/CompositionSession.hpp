// SPDX-License-Identifier: MIT
#pragma once

#include "core/LanguageRuleEngine.hpp"

namespace st {

/// Adapts the shared edit engine to native IME preedit/commit APIs. Backspaces
/// modify our uncommitted buffer, never text owned by another application.
class CompositionSession {
public:
    struct Result {
        bool handled = false;
        bool commit = false;
        std::u32string text;
        std::size_t cursor = 0; ///< Codepoint offset in uncommitted text.
    };

    explicit CompositionSession(EnginePtr engine) : engine_(std::move(engine)) {}
    Result processKey(const KeyEvent& key);
    Result finish();
    void reset();
    [[nodiscard]] std::u32string literalWord() const;
    [[nodiscard]] bool vietnameseOverride() const;

private:
    EnginePtr engine_;
    std::u32string preedit_;
    std::size_t cursor_ = 0;
    bool learningAllowed_ = true;
};

} // namespace st
