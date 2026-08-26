// SPDX-License-Identifier: MIT
//
// Thin, typed wrapper over QSettings. Engine options round-trip through the
// same string map the core uses, so a new engine option needs no change here.

#pragma once

#include "core/LanguageRuleEngine.hpp"

#include <QSettings>
#include <QString>
#include <QStringList>

namespace st {

class Settings {
public:
    Settings();

    [[nodiscard]] bool enabled() const;
    void               setEnabled(bool enabled);

    [[nodiscard]] QString languageId() const;
    void                  setLanguageId(const QString& id);

    [[nodiscard]] bool startHookOnLaunch() const;
    void               setStartHookOnLaunch(bool value);

    /// The modifier-only shortcut that cycles language, as one of the strings
    /// HookService::chordToString() produces.
    [[nodiscard]] QString cycleChord() const;
    void                  setCycleChord(const QString& chord);

    /// Per-engine option maps, stored under "engines/<id>/<key>".
    [[nodiscard]] EngineOptions engineOptions(const QString& engineId) const;
    void setEngineOptions(const QString& engineId, const EngineOptions& options);

    /// Directory holding custom rule files, layout overrides and the German
    /// exception list. Created on first use.
    [[nodiscard]] static QString configDirectory();

    void sync();

private:
    QSettings settings_;
};

} // namespace st
