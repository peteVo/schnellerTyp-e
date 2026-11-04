// SPDX-License-Identifier: MIT
#include "Fcitx5Engine.hpp"

#include "app/NativeEngine.hpp"
#include "core/Types.hpp"
#include "core/Unicode.hpp"

#include <fcitx-utils/key.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx/statusarea.h>
#include <fcitx/userinterfacemanager.h>

#include "app/Settings.hpp"
#include <QChar>
#include <QString>
#include <QProcess>

#include <cstdint>
#include <utility>

namespace st::fcitx5 {
namespace {

bool bypass(const fcitx::CapabilityFlags &flags)
{
    return flags.test(fcitx::CapabilityFlag::Password)
        || flags.test(fcitx::CapabilityFlag::Sensitive)
        || flags.test(fcitx::CapabilityFlag::Disable);
}

KeyEvent toCoreKey(const fcitx::KeyEvent &event)
{
    const auto &key = event.key();
    const auto &states = key.states();
    KeyEvent result;
    result.rawCode = static_cast<std::uint16_t>(key.code());
    result.ctrl = states.test(fcitx::KeyState::Ctrl);
    result.alt = states.test(fcitx::KeyState::Alt);
    result.shift = event.rawKey().states().test(fcitx::KeyState::Shift);
    result.meta = states.test(fcitx::KeyState::Super)
        || states.test(fcitx::KeyState::Super2)
        || states.test(fcitx::KeyState::Hyper)
        || states.test(fcitx::KeyState::Hyper2)
        || states.test(fcitx::KeyState::Meta);

    switch (key.sym()) {
    case FcitxKey_BackSpace: result.kind = KeyKind::Backspace; return result;
    case FcitxKey_Return:
    case FcitxKey_KP_Enter: result.kind = KeyKind::Enter; return result;
    case FcitxKey_Tab:
    case FcitxKey_ISO_Left_Tab: result.kind = KeyKind::Tab; return result;
    case FcitxKey_space: result.kind = KeyKind::Space; return result;
    case FcitxKey_Escape: result.kind = KeyKind::Escape; return result;
    case FcitxKey_Left:
    case FcitxKey_Right:
    case FcitxKey_Up:
    case FcitxKey_Down:
    case FcitxKey_Home:
    case FcitxKey_End:
    case FcitxKey_Page_Up:
    case FcitxKey_Page_Down:
        result.kind = KeyKind::Navigation;
        return result;
    default:
        break;
    }

    if (key.isModifier() || key.sym() == FcitxKey_ISO_Level3_Shift
        || key.sym() == FcitxKey_ISO_Level5_Shift || key.sym() == FcitxKey_Mode_switch) {
        result.kind = KeyKind::Modifier;
        return result;
    }

    const auto codepoint = fcitx::Key::keySymToUnicode(key.sym());
    if (codepoint >= 0x20 && codepoint != 0x7f && codepoint <= 0x10ffff
        && !(codepoint >= 0xd800 && codepoint <= 0xdfff)) {
        result.kind = KeyKind::Character;
        result.character = static_cast<char32_t>(codepoint);
        return result;
    }

    result.kind = KeyKind::Other;
    return result;
}

} // namespace

class StateAction final : public fcitx::Action {
public:
    explicit StateAction(Engine& engine) : engine_(engine) {}
    std::string shortText(fcitx::InputContext *ic) const override { return engine_.statusText(ic); }
    std::string icon(fcitx::InputContext *) const override { return "input-keyboard"; }
    std::string longText(fcitx::InputContext *ic) const override {
        return ic ? "Client ID for app preferences: " + ic->program() : "Select schnellerTyp-e in Fcitx";
    }
private:
    Engine& engine_;
};

Engine::Engine(fcitx::Instance *instance) : instance_(instance)
{
    statusAction_ = std::make_unique<StateAction>(*this);
    statusAction_->registerAction("schnellertype-status", &instance_->userInterfaceManager());
    learnAction_.setShortText("Keep current word literal in Automatic (Ctrl+Alt+L)");
    learnAction_.setIcon("document-save");
    learnAction_.registerAction("schnellertype-learn-literal", &instance_->userInterfaceManager());
    learnConnection_ = learnAction_.connect<fcitx::SimpleAction::Activated>([this](fcitx::InputContext *ic) {
        learnCurrentWord(ic);
    });
    pauseAction_.setShortText("Enable typing assistance");
    pauseAction_.setCheckable(true);
    pauseAction_.registerAction("schnellertype-enabled", &instance_->userInterfaceManager());
    pauseConnection_ = pauseAction_.connect<fcitx::SimpleAction::Activated>([this](fcitx::InputContext *) {
        Settings settings;
        settings.setEnabled(!settings.enabled());
        settings.sync();
        refreshContexts();
    });
    setupAction_.setShortText("Status & Setup…");
    setupAction_.setIcon("preferences-system");
    setupAction_.registerAction("schnellertype-setup", &instance_->userInterfaceManager());
    setupConnection_ = setupAction_.connect<fcitx::SimpleAction::Activated>([](fcitx::InputContext *) {
        QProcess::startDetached(QStringLiteral("schnellertype-setup"), {});
    });
    instance_->inputContextManager().registerProperty("schnellerTypEComposition",
                                                      &stateFactory_);
    focusOutHandler_ = instance_->watchEvent(
        fcitx::EventType::InputContextFocusOut,
        fcitx::EventWatcherPhase::PreInputMethod,
        [this](fcitx::Event &baseEvent) {
            auto &event = static_cast<fcitx::FocusOutEvent &>(baseEvent);
            // Fcitx has already committed client preedit before this event.
            discard(event.inputContext());
        });
    capabilityHandler_ = instance_->watchEvent(
        fcitx::EventType::InputContextCapabilityAboutToChange,
        fcitx::EventWatcherPhase::PreInputMethod,
        [this](fcitx::Event &baseEvent) {
            auto &event = static_cast<fcitx::CapabilityAboutToChangeEvent &>(baseEvent);
            if (bypass(event.newFlags()))
                discard(event.inputContext());
        });
    capabilityChangedHandler_ = instance_->watchEvent(
        fcitx::EventType::InputContextCapabilityChanged,
        fcitx::EventWatcherPhase::PreInputMethod,
        [this](fcitx::Event &baseEvent) {
            auto &event = static_cast<fcitx::CapabilityChangedEvent &>(baseEvent);
            if (bypass(event.newFlags()))
                discard(event.inputContext());
            statusAction_->update(event.inputContext());
            event.inputContext()->updateUserInterface(fcitx::UserInterfaceComponent::StatusArea);
        });
}

const fcitx::Configuration *Engine::getConfig() const
{
    config_.readSettings();
    return &config_;
}

void Engine::setConfig(const fcitx::RawConfig &config)
{
    // Configuration::load resets invalid options to defaults. Reject malformed
    // app maps before loading so a typo cannot silently erase saved preferences.
    if (const auto apps = config.get("ApplicationModes"); apps && !parseApplicationModes(apps->value()))
        return;
    if (const auto mode = config.get("Mode"); mode && mode->value() != "Automatic"
        && mode->value() != "German" && mode->value() != "Vietnamese" && mode->value() != "Unsupported")
        return;
    config_.readSettings();
    config_.load(config, true);
    config_.writeSettings();
    refreshContexts();
}

void Engine::refreshContexts()
{
    instance_->inputContextManager().foreach([this](fcitx::InputContext *ic) {
        auto *contextState = state(ic);
        if (contextState && contextState->session) {
            if (bypass(ic->capabilityFlags()))
                discard(ic);
            else
                finish(ic);
            configureContext(ic, *contextState);
        }
        return true;
    });
}

Engine::~Engine()
{
    instance_->userInterfaceManager().unregisterAction(&pauseAction_);
    instance_->userInterfaceManager().unregisterAction(&setupAction_);
    instance_->userInterfaceManager().unregisterAction(&learnAction_);
    instance_->userInterfaceManager().unregisterAction(statusAction_.get());
}

void Engine::configureContext(fcitx::InputContext *ic, ContextState& contextState)
{
    Settings settings;
    contextState.mode = settings.enabled() ? settings.nativeLanguageId(ic->program()).toStdString() : "off";
    if (contextState.mode != "auto" && contextState.mode != "de" && contextState.mode != "vi"
        && contextState.mode != "literal" && contextState.mode != "off")
        contextState.mode = "unsupported";
    contextState.session = std::make_unique<CompositionSession>(loadNativeEngine(ic->program()));
    contextState.rawWord.clear();
    contextState.wordLearnable = true;
    ic->statusArea().clearGroup(fcitx::StatusGroup::InputMethod);
    ic->statusArea().addAction(fcitx::StatusGroup::InputMethod, statusAction_.get());
    ic->statusArea().addAction(fcitx::StatusGroup::InputMethod, &learnAction_);
    pauseAction_.setChecked(settings.enabled());
    ic->statusArea().addAction(fcitx::StatusGroup::InputMethod, &pauseAction_);
    ic->statusArea().addAction(fcitx::StatusGroup::InputMethod, &setupAction_);
    statusAction_->update(ic);
    ic->updateUserInterface(fcitx::UserInterfaceComponent::StatusArea);
}

std::string Engine::statusText(fcitx::InputContext *ic) const
{
    auto *context = state(ic);
    if (!context || !context->session)
        return "Inactive";
    if (bypass(ic->capabilityFlags()))
        return "Sensitive or disabled field — literal input";
    if (context->mode == "off") return "Paused";
    if (context->mode == "literal") return "Literal for this application";
    if (context->mode == "unsupported") return "Unsupported saved profile — literal input; choose Automatic, German or Vietnamese";
    return "Active — " + std::string(context->mode == "auto" ? "Automatic" : context->mode == "de" ? "German" : "Vietnamese");
}

std::string Engine::subMode(const fcitx::InputMethodEntry&, fcitx::InputContext& ic)
{
    return statusText(&ic);
}

std::string Engine::subModeLabelImpl(const fcitx::InputMethodEntry&, fcitx::InputContext& ic)
{
    auto *context = state(&ic);
    if (!context || context->mode == "off") return "OFF";
    if (bypass(ic.capabilityFlags()) || context->mode == "literal") return "RAW";
    if (context->mode == "unsupported") return "!";
    return context->mode == "auto" ? "AUTO" : context->mode == "de" ? "DE" : "VI";
}

bool Engine::learnCurrentWord(fcitx::InputContext *ic)
{
    auto *context = state(ic);
    if (!context || !context->hasPreedit || !context->session || context->mode != "auto"
        || !context->wordLearnable || context->rawWord.empty() || bypass(ic->capabilityFlags()))
        return false;
    const auto raw = context->rawWord;
    Settings settings;
    if (!settings.addLiteralWord(QString::fromStdString(unicode::toUtf8(raw))))
        return false;
    context->session->reset();
    ic->commitString(unicode::toUtf8(raw));
    clearPreedit(ic, *context);
    context->rawWord.clear();
    refreshContexts();
    return true;
}

ContextState *Engine::state(fcitx::InputContext *ic) const
{
    return ic ? ic->propertyFor(&stateFactory_) : nullptr;
}

void Engine::showPreedit(fcitx::InputContext *ic, ContextState &contextState,
                         const std::u32string &text)
{
    contextState.hasPreedit = true;
    fcitx::Text preedit(unicode::toUtf8(text));
    preedit.setCursor(static_cast<int>(preedit.toString().size()));
    ic->inputPanel().setClientPreedit(preedit);
    ic->updatePreedit();
    contextState.hasPreedit = !text.empty();
}

void Engine::clearPreedit(fcitx::InputContext *ic, ContextState &contextState)
{
    if (!contextState.hasPreedit)
        return;
    ic->inputPanel().setClientPreedit(fcitx::Text());
    contextState.hasPreedit = false;
    ic->updatePreedit();
}

void Engine::finish(fcitx::InputContext *ic) noexcept
{
    try {
        auto *contextState = state(ic);
        if (!contextState || !contextState->session)
            return;

        contextState->rawWord.clear();
        contextState->wordLearnable = true;
        auto result = contextState->session->finish();
        if (result.commit && !result.text.empty())
            ic->commitString(unicode::toUtf8(result.text));
        clearPreedit(ic, *contextState);
    } catch (...) {
        discard(ic);
    }
}

void Engine::discard(fcitx::InputContext *ic) noexcept
{
    ContextState *contextState = nullptr;
    try {
        contextState = state(ic);
    } catch (...) {
        return;
    }
    if (!contextState)
        return;
    contextState->rawWord.clear();
    contextState->wordLearnable = true;
    if (contextState->session) {
        try {
            contextState->session->reset();
        } catch (...) {
        }
    }
    try {
        clearPreedit(ic, *contextState);
    } catch (...) {
        contextState->hasPreedit = false;
    }
}

void Engine::activate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event)
{
    auto *ic = event.inputContext();
    try {
        auto *contextState = state(ic);
        if (!contextState)
            return;
        configureContext(ic, *contextState);
    } catch (...) {
        discard(ic);
        try {
            if (auto *contextState = state(ic))
                contextState->session.reset();
        } catch (...) {
        }
    }
}

void Engine::deactivate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event)
{
    auto *ic = event.inputContext();
    finish(ic);
    try {
        if (auto *contextState = state(ic))
            contextState->session.reset();
    } catch (...) {
        discard(ic);
    }
}

void Engine::reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &event)
{
    discard(event.inputContext());
}

void Engine::keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &event)
{
    if (event.isRelease())
        return;

    auto *ic = event.inputContext();
    try {
        auto *contextState = state(ic);
        if (!contextState || !contextState->session)
            return;
        if (bypass(ic->capabilityFlags()) || contextState->mode == "off"
            || contextState->mode == "literal" || contextState->mode == "unsupported") {
            discard(ic);
            return;
        }

        const auto key = toCoreKey(event);
        if (key.ctrl && key.alt && !key.shift && !key.meta
            && unicode::toLowerAscii(key.character) == U'l' && learnCurrentWord(ic)) {
            event.filterAndAccept();
            return;
        }
        if (contextState->mode == "auto" && !key.isChord() && key.kind == KeyKind::Character) {
            if (QChar::isLetter(static_cast<char32_t>(key.character)) && contextState->rawWord.size() < 256)
                contextState->rawWord.push_back(key.character);
            else
                contextState->wordLearnable = false;
        } else if (key.kind == KeyKind::Backspace && !contextState->rawWord.empty()) {
            contextState->rawWord.pop_back();
        }
        auto result = contextState->session->processKey(key);
        if (result.commit) {
            contextState->rawWord.clear();
            contextState->wordLearnable = true;
            // Commit before clearing preedit: separate Wayland serial updates
            // can otherwise invalidate the pending commit at fast typing rates.
            if (!result.text.empty())
                ic->commitString(unicode::toUtf8(result.text));
            clearPreedit(ic, *contextState);
        } else if (result.handled) {
            showPreedit(ic, *contextState, result.text);
        }

        if (result.handled)
            event.filterAndAccept();
    } catch (...) {
        discard(ic);
    }
}

class EngineFactory final : public fcitx::AddonFactory {
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override
    {
        return new Engine(manager->instance());
    }
};

} // namespace st::fcitx5

FCITX_ADDON_FACTORY(st::fcitx5::EngineFactory)
