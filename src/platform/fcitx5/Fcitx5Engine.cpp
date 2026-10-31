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

#include <cstdint>
#include <utility>

namespace st::fcitx5 {
namespace {

bool sensitive(const fcitx::CapabilityFlags &flags)
{
    return flags.test(fcitx::CapabilityFlag::Password)
        || flags.test(fcitx::CapabilityFlag::Sensitive);
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

    if (key.isModifier()) {
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

Engine::Engine(fcitx::Instance *instance) : instance_(instance)
{
    instance_->inputContextManager().registerProperty("schnellerTypEComposition",
                                                      &stateFactory_);
    focusOutHandler_ = instance_->watchEvent(
        fcitx::EventType::InputContextFocusOut,
        fcitx::EventWatcherPhase::PreInputMethod,
        [this](fcitx::Event &baseEvent) {
            auto &event = static_cast<fcitx::FocusOutEvent &>(baseEvent);
            finish(event.inputContext());
        });
    capabilityHandler_ = instance_->watchEvent(
        fcitx::EventType::InputContextCapabilityAboutToChange,
        fcitx::EventWatcherPhase::PreInputMethod,
        [this](fcitx::Event &baseEvent) {
            auto &event = static_cast<fcitx::CapabilityAboutToChangeEvent &>(baseEvent);
            if (sensitive(event.newFlags()))
                discard(event.inputContext());
        });
    capabilityChangedHandler_ = instance_->watchEvent(
        fcitx::EventType::InputContextCapabilityChanged,
        fcitx::EventWatcherPhase::PreInputMethod,
        [this](fcitx::Event &baseEvent) {
            auto &event = static_cast<fcitx::CapabilityChangedEvent &>(baseEvent);
            if (sensitive(event.newFlags()))
                discard(event.inputContext());
        });
}

ContextState *Engine::state(fcitx::InputContext *ic) const
{
    return ic ? ic->propertyFor(&stateFactory_) : nullptr;
}

void Engine::showPreedit(fcitx::InputContext *ic, ContextState &contextState,
                         const std::u32string &text)
{
    contextState.hasPreedit = true;
    ic->inputPanel().setClientPreedit(fcitx::Text(unicode::toUtf8(text)));
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

        auto result = contextState->session->finish();
        clearPreedit(ic, *contextState);
        if (result.commit && !result.text.empty())
            ic->commitString(unicode::toUtf8(result.text));
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
        auto engine = loadNativeEngine();
        contextState->session = std::make_unique<CompositionSession>(std::move(engine));
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
        if (sensitive(ic->capabilityFlags())) {
            discard(ic);
            return;
        }

        auto result = contextState->session->processKey(toCoreKey(event));
        if (result.commit) {
            clearPreedit(ic, *contextState);
            if (!result.text.empty())
                ic->commitString(unicode::toUtf8(result.text));
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
