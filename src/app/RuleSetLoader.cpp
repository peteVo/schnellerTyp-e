// SPDX-License-Identifier: MIT
#include "app/RuleSetLoader.hpp"

#include "app/Diagnostics.hpp"
#include "core/RuleEngineRegistry.hpp"
#include "core/Unicode.hpp"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTextStream>

namespace st {
namespace {

[[nodiscard]] std::u32string toU32(const QString& s)
{
    return unicode::fromUtf8(s.toUtf8().constData());
}

const char* const kExampleRules = R"JSON({
  "id": "fr",
  "displayName": "French accents",
  "badge": "FR",
  "doubleKeyRevert": true,
  "escapeKey": "\\",
  "rules": [
    { "trigger": "e'",  "replacement": "é" },
    { "trigger": "e`",  "replacement": "è" },
    { "trigger": "e^",  "replacement": "ê" },
    { "trigger": "e\"", "replacement": "ë" },
    { "trigger": "a`",  "replacement": "à" },
    { "trigger": "a^",  "replacement": "â" },
    { "trigger": "u`",  "replacement": "ù" },
    { "trigger": "u^",  "replacement": "û" },
    { "trigger": "i^",  "replacement": "î" },
    { "trigger": "i\"", "replacement": "ï" },
    { "trigger": "o^",  "replacement": "ô" },
    { "trigger": "c,",  "replacement": "ç" },

    { "trigger": "btw",  "replacement": "by the way",  "smartCase": true },
    { "trigger": "@@",   "replacement": "you@example.com", "smartCase": false }
  ]
}
)JSON";

const char* const kExampleLayout = R"JSON({
  "name": "German QWERTZ",
  "comment": "Only the keys whose characters differ from US QWERTY need listing.",
  "keys": {
    "VC_Y": { "plain": "z", "shift": "Z" },
    "VC_Z": { "plain": "y", "shift": "Y" },
    "VC_MINUS":     { "plain": "ß", "shift": "?" },
    "VC_SEMICOLON": { "plain": "ö", "shift": "Ö" },
    "VC_QUOTE":     { "plain": "ä", "shift": "Ä" },
    "VC_OPEN_BRACKET": { "plain": "ü", "shift": "Ü" }
  }
}
)JSON";

const char* const kExampleExceptions = R"TXT(# schnellerTyp-e — extra German exception words.
#
# One line per digraph. Words are written the way you TYPE them (ASCII), are
# matched case-insensitively, and act as prefixes: listing "dauer" also covers
# "dauern", "Dauerkarte" and "dauerhaft".
#
# A word listed here keeps that digraph literal for as long as what you have
# typed and the listed word agree. As soon as they diverge the transformation
# comes back on its own, so "duell" stays "duell" while "duester" still becomes
# "düster".
#
# Entries here are merged with the list compiled into the program.

ue: sauerteig, feuerzeug, abenteurer
oe: koedukativ
ae: aeroplan
ss: nussschale, flussschifffahrt, ausschuesse
)TXT";

void writeIfMissing(const QString& path, const char* contents)
{
    if (QFile::exists(path))
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << QString::fromUtf8(contents);
}

} // namespace

// ---------------------------------------------------------------------------

void RuleSetLoader::ensureConfigDirectory(const QString& configDir)
{
    QDir dir(configDir);
    dir.mkpath(QStringLiteral("rules"));
    dir.mkpath(QStringLiteral("layouts"));

    writeIfMissing(dir.filePath(QStringLiteral("rules/french.example.json")), kExampleRules);
    writeIfMissing(dir.filePath(QStringLiteral("layouts/qwertz.example.json")), kExampleLayout);
    writeIfMissing(dir.filePath(QStringLiteral("german-exceptions.txt")), kExampleExceptions);
}

std::optional<RuleSetDefinition> RuleSetLoader::parseRuleSet(const QByteArray& json,
                                                             QString*          error)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (document.isNull() || !document.isObject()) {
        if (error != nullptr)
            *error = parseError.errorString();
        return std::nullopt;
    }

    const QJsonObject root = document.object();
    RuleSetDefinition definition;
    const QString     id = root.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        if (error != nullptr)
            *error = QStringLiteral("missing \"id\"");
        return std::nullopt;
    }

    definition.id = id.startsWith(QStringLiteral("custom:"))
                        ? id.toStdString()
                        : ("custom:" + id).toStdString();
    definition.displayName =
        root.value(QStringLiteral("displayName")).toString(id).toStdString();
    definition.badge = root.value(QStringLiteral("badge")).toString(id.left(2).toUpper())
                           .toStdString();
    definition.doubleKeyRevert = root.value(QStringLiteral("doubleKeyRevert")).toBool(true);

    const QString escape = root.value(QStringLiteral("escapeKey")).toString(QStringLiteral("\\"));
    const std::u32string escapeU32 = toU32(escape);
    definition.escapeKey           = escapeU32.empty() ? 0 : escapeU32.front();

    const QJsonArray rules = root.value(QStringLiteral("rules")).toArray();
    definition.rules.reserve(static_cast<std::size_t>(rules.size()));
    for (const QJsonValue& value : rules) {
        const QJsonObject entry   = value.toObject();
        const QString     trigger = entry.value(QStringLiteral("trigger")).toString();
        const QString replacement = entry.value(QStringLiteral("replacement")).toString();
        if (trigger.isEmpty() || replacement.isEmpty())
            continue;

        SubstitutionRule rule;
        rule.trigger       = toU32(trigger);
        rule.replacement   = toU32(replacement);
        rule.caseSensitive = entry.value(QStringLiteral("caseSensitive")).toBool(false);
        rule.smartCase     = entry.value(QStringLiteral("smartCase")).toBool(true);
        rule.wordStart     = entry.value(QStringLiteral("wordStart")).toBool(false);
        definition.rules.push_back(std::move(rule));
    }

    if (definition.rules.empty()) {
        if (error != nullptr)
            *error = QStringLiteral("no usable entries in \"rules\"");
        return std::nullopt;
    }
    return definition;
}

void RuleSetLoader::loadRuleSets(const QString& configDir, LoadReport& report)
{
    QDir dir(configDir + QStringLiteral("/rules"));
    if (!dir.exists())
        return;

    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        diagnostics::milestone(QStringLiteral("    rules: ") + name);
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            report.errors << QStringLiteral("%1: cannot open").arg(name);
            continue;
        }
        QString error;
        if (auto definition = parseRuleSet(file.readAll(), &error)) {
            RuleEngineRegistry::instance().registerRuleSet(std::move(*definition));
            ++report.ruleSets;
        } else {
            report.errors << QStringLiteral("%1: %2").arg(name, error);
        }
    }
}

void RuleSetLoader::loadLayouts(const QString& configDir, KeyMapper& mapper, LoadReport& report)
{
    QDir dir(configDir + QStringLiteral("/layouts"));
    if (!dir.exists())
        return;

    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString& name : files) {
        // ".example.json" files are documentation, not configuration.
        if (name.contains(QStringLiteral(".example.")))
            continue;
        diagnostics::milestone(QStringLiteral("    layout: ") + name);

        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            report.errors << QStringLiteral("%1: cannot open").arg(name);
            continue;
        }
        QJsonParseError     parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (!document.isObject()) {
            report.errors << QStringLiteral("%1: %2").arg(name, parseError.errorString());
            continue;
        }
        const QJsonObject keys = document.object().value(QStringLiteral("keys")).toObject();
        for (auto it = keys.begin(); it != keys.end(); ++it) {
            const std::uint16_t keycode = KeyMapper::keycodeFromName(it.key().toStdString());
            if (keycode == 0) {
                report.errors << QStringLiteral("%1: unknown key %2").arg(name, it.key());
                continue;
            }
            const QJsonObject   entry = it.value().toObject();
            const std::u32string plain = toU32(entry.value(QStringLiteral("plain")).toString());
            const std::u32string shift = toU32(entry.value(QStringLiteral("shift")).toString());
            if (plain.empty())
                continue;
            mapper.setOverride(keycode,
                               KeyMapper::KeyChars{plain.front(),
                                                   shift.empty() ? plain.front() : shift.front()});
        }
        ++report.layouts;
    }
}

GermanRuleEngine::ExceptionTable RuleSetLoader::loadGermanExceptions(const QString& configDir,
                                                                    LoadReport&    report)
{
    GermanRuleEngine::ExceptionTable table = GermanRuleEngine::builtinExceptions();

    QFile file(configDir + QStringLiteral("/german-exceptions.txt"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return table;

    const QByteArray                       bytes = file.readAll();
    const GermanRuleEngine::ExceptionTable extra =
        GermanRuleEngine::parseExceptionText(std::string_view(bytes.constData(),
                                                             static_cast<std::size_t>(bytes.size())));
    for (std::size_t i = 0; i < table.size(); ++i) {
        table[i].insert(table[i].end(), extra[i].begin(), extra[i].end());
        report.exceptionWords += static_cast<int>(extra[i].size());
    }
    return table;
}

} // namespace st
