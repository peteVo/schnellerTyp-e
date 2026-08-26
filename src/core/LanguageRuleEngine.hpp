// SPDX-License-Identifier: MIT
//
// The extension point of schnellerTyp-e.
//
// A LanguageRuleEngine is a deterministic state machine fed one keystroke at a
// time. It owns the notion of "the word currently being typed" and answers, for
// every keystroke, whether the on-screen text needs editing.
//
// Implementations must be:
//   * self-contained  — no Qt, no OS calls, no I/O;
//   * deterministic   — identical key sequences give identical results, with no
//                       dependency on wall-clock timing. This is what makes fast
//                       typing safe: the hook layer delivers keystrokes in order
//                       and the engine never races itself;
//   * cheap           — processKey() runs inside the OS keyboard hook budget
//                       (a few hundred microseconds on Windows).

#pragma once

#include "core/Types.hpp"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace st {

/// Free-form engine configuration. Strings keep the interface stable across
/// engines and map 1:1 onto both QSettings and the JSON rule files, so a new
/// engine needs no changes anywhere in the Qt or persistence layers.
using EngineOptions = std::map<std::string, std::string, std::less<>>;

class LanguageRuleEngine {
public:
    virtual ~LanguageRuleEngine() = default;

    LanguageRuleEngine(const LanguageRuleEngine&)            = delete;
    LanguageRuleEngine& operator=(const LanguageRuleEngine&) = delete;

    // --- identity -----------------------------------------------------------

    /// Stable machine id, e.g. "de", "vi", "custom:fr".
    [[nodiscard]] virtual std::string id() const = 0;
    /// Human-readable name shown in the settings window.
    [[nodiscard]] virtual std::string displayName() const = 0;
    /// Two/three character tray badge, e.g. "DE", "VN".
    [[nodiscard]] virtual std::string badge() const = 0;

    // --- state --------------------------------------------------------------

    /// Drop all word state. Called on focus change, mouse click, caret movement,
    /// language switch and whenever the hook layer loses confidence in what is
    /// on screen. Must never emit anything.
    virtual void reset() noexcept = 0;

    /// Feed one keystroke.
    [[nodiscard]] virtual EngineResult processKey(const KeyEvent& event) = 0;

    // --- configuration ------------------------------------------------------

    /// Apply options. Unknown keys must be ignored, not rejected, so that a
    /// settings file written by a newer build still loads.
    virtual void applyOptions(const EngineOptions& options) { (void)options; }

    /// Current effective options, for round-tripping into QSettings.
    [[nodiscard]] virtual EngineOptions options() const { return {}; }

protected:
    LanguageRuleEngine() = default;
};

using EnginePtr = std::unique_ptr<LanguageRuleEngine>;

/// Descriptor used to populate the language selector without instantiating
/// every engine.
struct EngineDescriptor {
    std::string id;
    std::string displayName;
    std::string badge;
};

} // namespace st
