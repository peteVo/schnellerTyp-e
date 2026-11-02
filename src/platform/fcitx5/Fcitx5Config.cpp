// SPDX-License-Identifier: MIT
#include "Fcitx5Config.hpp"
#include "app/Settings.hpp"

namespace st::fcitx5 {
namespace {
const char *boolean(bool value) { return value ? "true" : "false"; }
bool option(const EngineOptions &options, const char *key, bool fallback)
{
    const auto it = options.find(key);
    return it == options.end() ? fallback : it->second == "true" || it->second == "1";
}
}

void Config::readSettings()
{
    Settings settings;
    enabled.setValue(settings.enabled());
    const auto id = settings.languageId();
    mode.setValue(id == QStringLiteral("de") ? Mode::German
                  : id == QStringLiteral("vi") ? Mode::Vietnamese : Mode::Automatic);
    const auto vi = settings.engineOptions(QStringLiteral("vi"));
    const auto de = settings.engineOptions(QStringLiteral("de"));
    const auto automatic = settings.engineOptions(QStringLiteral("auto"));
    const auto methodIt = vi.find("method");
    method.setValue(methodIt != vi.end() && methodIt->second == "vni" ? Method::VNI : Method::Telex);
    modernTone.setValue(option(vi, "modernTone", false));
    spellCheck.setValue(option(vi, "spellCheck", true));
    autoRestore.setValue(option(vi, "autoRestore", true));
    umlauts.setValue(option(de, "umlauts", true));
    sharpS.setValue(option(de, "sharpS", true));
    capitalSharpS.setValue(option(de, "capitalSharpS", false));
    doubleKeyRevert.setValue(option(de, "doubleKeyRevert", true));
    useExceptions.setValue(option(de, "useExceptions", true));
    const auto literals = automatic.find("literalWords");
    literalWords.setValue(literals == automatic.end() ? "" : literals->second);
}

void Config::writeSettings() const
{
    Settings settings;
    settings.setEnabled(*enabled);
    settings.setLanguageId(*mode == Mode::German ? QStringLiteral("de")
                           : *mode == Mode::Vietnamese ? QStringLiteral("vi") : QStringLiteral("auto"));
    settings.setEngineOptions(QStringLiteral("vi"), {
        {"method", *method == Method::VNI ? "vni" : "telex"},
        {"modernTone", boolean(*modernTone)}, {"spellCheck", boolean(*spellCheck)},
        {"autoRestore", boolean(*autoRestore)}});
    settings.setEngineOptions(QStringLiteral("de"), {
        {"umlauts", boolean(*umlauts)}, {"sharpS", boolean(*sharpS)},
        {"capitalSharpS", boolean(*capitalSharpS)}, {"doubleKeyRevert", boolean(*doubleKeyRevert)},
        {"useExceptions", boolean(*useExceptions)}});
    settings.setEngineOptions(QStringLiteral("auto"), {{"literalWords", *literalWords}});
    settings.sync();
}
} // namespace st::fcitx5
