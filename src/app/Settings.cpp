// SPDX-License-Identifier: MIT
#include "app/Settings.hpp"

#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QVariantMap>

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

Settings::Settings()
    : settings_(QSettings::defaultFormat(), QSettings::UserScope,
                QStringLiteral("schnellerTyp-e"), QStringLiteral("schnellerTyp-e"))
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
    return settings_.value(kLanguageKey, QStringLiteral("auto")).toString();
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

EngineOptions Settings::applicationModes() const
{
    EngineOptions result;
    const auto values = settings_.value(QStringLiteral("applications/modes")).toMap();
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        result[it.key().toStdString()] = it.value().toString().toStdString();
    return result;
}

bool Settings::validateApplicationModes(const EngineOptions& modes)
{
    if (modes.size() > 128)
        return false;
    for (const auto& [program, mode] : modes) {
        const auto name = QString::fromStdString(program);
        if (name.isEmpty() || name.size() > 256 || name.contains(QRegularExpression(QStringLiteral("[\\s=;]")))
            || (mode != "auto" && mode != "de" && mode != "vi" && mode != "literal"))
            return false;
    }
    return true;
}

bool Settings::setApplicationModes(const EngineOptions& modes)
{
    if (!validateApplicationModes(modes))
        return false;
    QVariantMap values;
    for (const auto& [name, mode] : modes)
        values.insert(QString::fromStdString(name), QString::fromStdString(mode));
    settings_.setValue(QStringLiteral("applications/modes"), values);
    return true;
}

QString Settings::nativeLanguageId(const std::string& program) const
{
    const auto modes = applicationModes();
    if (const auto it = modes.find(program); !program.empty() && it != modes.end())
        return QString::fromStdString(it->second);
    return languageId();
}

bool Settings::addLiteralWord(const QString& word)
{
    if (word.isEmpty() || word.size() > 256)
        return false;
    for (const auto c : word)
        if (!c.isLetter())
            return false;
    auto options = engineOptions(QStringLiteral("auto"));
    const auto value = QString::fromStdString(options["literalWords"]);
    const auto words = value.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    if (!words.contains(word, Qt::CaseInsensitive)) {
        options["literalWords"] = (value.isEmpty() ? word : value + QLatin1Char('\n') + word).toStdString();
        setEngineOptions(QStringLiteral("auto"), options);
    }
    sync();
    return settings_.status() == QSettings::NoError;
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
