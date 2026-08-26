// SPDX-License-Identifier: MIT
#include "app/Settings.hpp"

#include <QDir>
#include <QStandardPaths>

namespace st {
namespace {

// The group is called "app", not "general": QSettings reserves the section name
// "General" (case-insensitively) for keys that have no group, and writing a
// group of that name produces a second [%General] section in the INI file which
// then shadows the first. That cost an afternoon once.
const auto kEnabledKey   = QStringLiteral("app/enabled");
const auto kLanguageKey  = QStringLiteral("app/language");
const auto kStartHookKey = QStringLiteral("app/startHookOnLaunch");
const auto kCycleChordKey = QStringLiteral("app/cycleChord");

} // namespace

Settings::Settings() : settings_(QStringLiteral("schnellerTyp-e"), QStringLiteral("schnellerTyp-e"))
{
}

bool Settings::enabled() const
{
    return settings_.value(kEnabledKey, true).toBool();
}

void Settings::setEnabled(bool enabled)
{
    settings_.setValue(kEnabledKey, enabled);
}

QString Settings::languageId() const
{
    return settings_.value(kLanguageKey, QStringLiteral("de")).toString();
}

void Settings::setLanguageId(const QString& id)
{
    settings_.setValue(kLanguageKey, id);
}

bool Settings::startHookOnLaunch() const
{
    return settings_.value(kStartHookKey, true).toBool();
}

void Settings::setStartHookOnLaunch(bool value)
{
    settings_.setValue(kStartHookKey, value);
}

QString Settings::cycleChord() const
{
    return settings_.value(kCycleChordKey, QStringLiteral("ctrl+shift")).toString();
}

void Settings::setCycleChord(const QString& chord)
{
    settings_.setValue(kCycleChordKey, chord);
}

EngineOptions Settings::engineOptions(const QString& engineId) const
{
    EngineOptions options;
    auto&         mutableSettings = const_cast<QSettings&>(settings_);
    mutableSettings.beginGroup(QStringLiteral("engines/") + engineId);
    const QStringList keys = mutableSettings.childKeys();
    for (const QString& key : keys)
        options[key.toStdString()] = mutableSettings.value(key).toString().toStdString();
    mutableSettings.endGroup();
    return options;
}

void Settings::setEngineOptions(const QString& engineId, const EngineOptions& options)
{
    settings_.beginGroup(QStringLiteral("engines/") + engineId);
    for (const auto& [key, value] : options)
        settings_.setValue(QString::fromStdString(key), QString::fromStdString(value));
    settings_.endGroup();
}

QString Settings::configDirectory()
{
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(path);
    return path;
}

void Settings::sync() { settings_.sync(); }

} // namespace st
