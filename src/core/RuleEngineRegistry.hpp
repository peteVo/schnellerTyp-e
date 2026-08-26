// SPDX-License-Identifier: MIT
//
// The registry is the single place that knows which engines exist.
//
// Adding a language is therefore either:
//   * C++  — subclass LanguageRuleEngine and call registerEngine(); or
//   * JSON — call registerRuleSet() with a definition parsed from disk.
//
// Nothing else in the application needs to change: the tray, the settings
// window and the persistence layer all work from EngineDescriptor and the
// string id.

#pragma once

#include "core/LanguageRuleEngine.hpp"
#include "core/custom/RuleSetEngine.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace st {

class RuleEngineRegistry {
public:
    using Factory = std::function<EnginePtr()>;

    /// Process-wide registry. Guarded by a mutex because rule files can be
    /// reloaded from the UI thread while the hook thread is instantiating.
    static RuleEngineRegistry& instance();

    /// German + Vietnamese. Idempotent.
    void registerBuiltins();

    void registerEngine(EngineDescriptor descriptor, Factory factory);

    /// Register (or replace) a JSON-defined rule set.
    void registerRuleSet(RuleSetDefinition definition);

    /// Drop every engine whose id starts with "custom:", used when the user
    /// asks to reload the rule files.
    void clearRuleSets();

    [[nodiscard]] std::vector<EngineDescriptor> descriptors() const;
    [[nodiscard]] bool                          contains(std::string_view id) const;

    /// Returns nullptr for an unknown id.
    [[nodiscard]] EnginePtr create(std::string_view id) const;

private:
    RuleEngineRegistry() = default;

    struct Entry {
        EngineDescriptor descriptor;
        Factory          factory;
    };

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    bool               builtinsRegistered_ = false;
};

} // namespace st
