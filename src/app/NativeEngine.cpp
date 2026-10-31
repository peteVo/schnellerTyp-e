// SPDX-License-Identifier: MIT
#include "app/NativeEngine.hpp"

#include "app/Settings.hpp"
#include "core/RuleEngineRegistry.hpp"
#include "core/auto/AutoRuleEngine.hpp"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace st {

EnginePtr loadNativeEngine()
{
    Settings settings;
    if (!settings.enabled())
        return nullptr;
    auto& registry = RuleEngineRegistry::instance();
    registry.registerBuiltins();
    auto id = settings.languageId();
    auto engine = registry.create(id.toStdString());
    if (!engine) {
        id = QStringLiteral("auto");
        engine = registry.create("auto");
    }

    auto german = GermanRuleEngine::builtinExceptions();
    // Fcitx is the process host, so QStandardPaths::AppConfigLocation would
    // resolve to Fcitx's directory instead of our settings panel's directory.
    const auto config = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                            .filePath(QStringLiteral("schnellerTyp-e"));
    QFile file(QDir(config).filePath(QStringLiteral("german-exceptions.txt")));
    if (file.open(QIODevice::ReadOnly)) {
        const auto extra = GermanRuleEngine::parseExceptionText(file.readAll().toStdString());
        for (std::size_t i = 0; i < german.size(); ++i)
            german[i].insert(german[i].end(), extra[i].begin(), extra[i].end());
    }

    auto options = settings.engineOptions(id);
    if (auto* automatic = dynamic_cast<AutoRuleEngine*>(engine.get())) {
        for (const auto* language : {"vi", "de"})
            for (const auto& [key, value] : settings.engineOptions(QString::fromLatin1(language)))
                options[key] = value;
        automatic->setGermanExceptions(std::move(german));
    } else if (auto* de = dynamic_cast<GermanRuleEngine*>(engine.get())) {
        de->setExceptions(std::move(german));
    }
    engine->applyOptions(options);
    return engine;
}

} // namespace st
