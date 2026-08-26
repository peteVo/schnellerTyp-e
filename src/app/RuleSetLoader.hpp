// SPDX-License-Identifier: MIT
//
// Bridges the JSON on disk to the Qt-free core.
//
// Everything a user can extend without a compiler lives here:
//   <config>/rules/*.json      additional languages / text expansions
//   <config>/layouts/*.json    keyboard layout overrides for the key mapper
//   <config>/german-exceptions.txt   extra words for the German dictionary
//
// The core never sees Qt types; this file converts.

#pragma once

#include "core/custom/RuleSetEngine.hpp"
#include "core/german/GermanRuleEngine.hpp"
#include "hook/KeyMapper.hpp"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

namespace st {

struct LoadReport {
    int         ruleSets      = 0;
    int         layouts       = 0;
    int         exceptionWords = 0;
    QStringList errors;
};

class RuleSetLoader {
public:
    /// Create the config directory tree and write the shipped examples the
    /// first time the app runs.
    static void ensureConfigDirectory(const QString& configDir);

    /// Parse one rules JSON document. Returns nullopt and fills `error` on a
    /// malformed file.
    [[nodiscard]] static std::optional<RuleSetDefinition>
    parseRuleSet(const QByteArray& json, QString* error);

    /// Load every *.json under <configDir>/rules and register it.
    static void loadRuleSets(const QString& configDir, LoadReport& report);

    /// Load every *.json under <configDir>/layouts into the key mapper.
    static void loadLayouts(const QString& configDir, KeyMapper& mapper, LoadReport& report);

    /// Merge <configDir>/german-exceptions.txt into the built-in dictionary.
    [[nodiscard]] static GermanRuleEngine::ExceptionTable
    loadGermanExceptions(const QString& configDir, LoadReport& report);
};

} // namespace st
