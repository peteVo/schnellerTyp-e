// SPDX-License-Identifier: MIT
#pragma once

#include "core/LanguageRuleEngine.hpp"
#include <optional>
#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>

namespace st::fcitx5 {

FCITX_CONFIG_ENUM(Mode, Automatic, German, Vietnamese, Unsupported);

std::optional<EngineOptions> parseApplicationModes(const std::string& text);
struct ApplicationModesConstrain {
    using Type = std::string;
    bool check(const std::string& text) const { return parseApplicationModes(text).has_value(); }
    void dumpDescription(fcitx::RawConfig&) const {}
};
FCITX_CONFIG_ENUM(Method, Telex, VNI);

FCITX_CONFIGURATION_CLASS(Config,
    fcitx::Option<bool> enabled{this, "Enabled", "Enable typing assistance", true};
    fcitx::Option<Mode> mode{this, "Mode", "Typing mode", Mode::Automatic};
    fcitx::Option<Method> method{this, "Method", "Vietnamese input method", Method::Telex};
    fcitx::Option<bool> modernTone{this, "ModernTone", "Modern Vietnamese tone placement", false};
    fcitx::Option<bool> spellCheck{this, "SpellCheck", "Vietnamese spelling check", true};
    fcitx::Option<bool> autoRestore{this, "AutoRestore", "Restore invalid Vietnamese words", true};
    fcitx::Option<bool> umlauts{this, "Umlauts", "German umlauts (ae, oe, ue)", true};
    fcitx::Option<bool> sharpS{this, "SharpS", "German sharp S (ss)", true};
    fcitx::Option<bool> capitalSharpS{this, "CapitalSharpS", "Use capital sharp S", false};
    fcitx::Option<bool> doubleKeyRevert{this, "DoubleKeyRevert", "German double-key undo", true};
    fcitx::Option<bool> useExceptions{this, "UseExceptions", "Use German spelling exceptions", true};
    fcitx::Option<std::string> literalWords{this, "LiteralWords", "Keep these words literal (one per line or comma-separated)", ""};

    fcitx::Option<std::string, ApplicationModesConstrain> applicationModes{
        this, "ApplicationModes", "App preferences: exact program=auto/de/vi/literal; separated by semicolons", ""};

    void readSettings();
    void writeSettings() const;
);

} // namespace st::fcitx5
