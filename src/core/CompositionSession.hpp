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
    };

    explicit CompositionSession(EnginePtr engine) : engine_(std::move(engine)) {}
    Result processKey(const KeyEvent& key);
    Result finish();
    void reset();

private:
    EnginePtr engine_;
    std::u32string preedit_;
};

} // namespace st
