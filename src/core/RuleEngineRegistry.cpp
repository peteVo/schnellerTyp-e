// SPDX-License-Identifier: MIT
#include "core/RuleEngineRegistry.hpp"

#include "core/german/GermanRuleEngine.hpp"
#include "core/vietnamese/VietnameseRuleEngine.hpp"

#include <algorithm>

namespace st {

RuleEngineRegistry& RuleEngineRegistry::instance()
{
    static RuleEngineRegistry registry;
    return registry;
}

void RuleEngineRegistry::registerBuiltins()
{
    {
        std::scoped_lock lock(mutex_);
        if (builtinsRegistered_)
            return;
        builtinsRegistered_ = true;
    }
    registerEngine({"de", "German (Umlaut)", "DE"},
                   [] { return std::make_unique<GermanRuleEngine>(); });
    registerEngine({"vi", "Vietnamese", "VN"},
                   [] { return std::make_unique<VietnameseRuleEngine>(); });
}

void RuleEngineRegistry::registerEngine(EngineDescriptor descriptor, Factory factory)
{
    std::scoped_lock lock(mutex_);
    const auto       it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
        return e.descriptor.id == descriptor.id;
    });
    if (it != entries_.end()) {
        it->descriptor = std::move(descriptor);
        it->factory    = std::move(factory);
        return;
    }
    entries_.push_back(Entry{std::move(descriptor), std::move(factory)});
}

void RuleEngineRegistry::registerRuleSet(RuleSetDefinition definition)
{
    EngineDescriptor descriptor{definition.id, definition.displayName, definition.badge};
    registerEngine(std::move(descriptor), [definition = std::move(definition)] {
        return std::make_unique<RuleSetEngine>(definition);
    });
}

void RuleEngineRegistry::clearRuleSets()
{
    std::scoped_lock lock(mutex_);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [](const Entry& e) {
                                      return e.descriptor.id.rfind("custom:", 0) == 0;
                                  }),
                   entries_.end());
}

std::vector<EngineDescriptor> RuleEngineRegistry::descriptors() const
{
    std::scoped_lock              lock(mutex_);
    std::vector<EngineDescriptor> out;
    out.reserve(entries_.size());
    for (const Entry& e : entries_)
        out.push_back(e.descriptor);
    return out;
}

bool RuleEngineRegistry::contains(std::string_view id) const
{
    std::scoped_lock lock(mutex_);
    return std::any_of(entries_.begin(), entries_.end(),
                       [&](const Entry& e) { return e.descriptor.id == id; });
}

EnginePtr RuleEngineRegistry::create(std::string_view id) const
{
    std::scoped_lock lock(mutex_);
    const auto       it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
        return e.descriptor.id == id;
    });
    return it == entries_.end() ? nullptr : it->factory();
}

} // namespace st
