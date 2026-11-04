// SPDX-License-Identifier: MIT
#pragma once

#include "core/CompositionSession.hpp"
#include "Fcitx5Config.hpp"

#include <fcitx/event.h>
#include <fcitx/action.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include <memory>
#include <string>

namespace st::fcitx5 {

class ContextState final : public fcitx::InputContextProperty {
public:
    std::unique_ptr<CompositionSession> session;
    bool hasPreedit = false;
    std::string mode;
    std::u32string rawWord;
    bool wordLearnable = true;
};

class Engine final : public fcitx::InputMethodEngineV2 {
public:
    explicit Engine(fcitx::Instance *instance);
    ~Engine() override;
    std::string statusText(fcitx::InputContext *ic) const;
    std::string subMode(const fcitx::InputMethodEntry&, fcitx::InputContext& ic) override;
    std::string subModeLabelImpl(const fcitx::InputMethodEntry&, fcitx::InputContext& ic) override;
    bool learnCurrentWord(fcitx::InputContext *ic);
    const fcitx::Configuration *getConfig() const override;
    void setConfig(const fcitx::RawConfig &config) override;

    void keyEvent(const fcitx::InputMethodEntry &entry, fcitx::KeyEvent &event) override;
    void activate(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) override;
    void deactivate(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) override;
    void reset(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) override;

private:
    void configureContext(fcitx::InputContext *ic, ContextState& contextState);
    void refreshContexts();
    ContextState *state(fcitx::InputContext *ic) const;
    void showPreedit(fcitx::InputContext *ic, ContextState &state,
                     const std::u32string &text);
    void clearPreedit(fcitx::InputContext *ic, ContextState &state);
    void finish(fcitx::InputContext *ic) noexcept;
    void discard(fcitx::InputContext *ic) noexcept;

    fcitx::Instance *instance_;
    mutable Config config_;
    std::unique_ptr<fcitx::Action> statusAction_;
    fcitx::SimpleAction learnAction_;
    fcitx::SimpleAction pauseAction_;
    fcitx::SimpleAction setupAction_;
    fcitx::ScopedConnection learnConnection_;
    fcitx::ScopedConnection pauseConnection_;
    fcitx::ScopedConnection setupConnection_;
    fcitx::SimpleInputContextPropertyFactory<ContextState> stateFactory_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> focusOutHandler_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> capabilityHandler_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> capabilityChangedHandler_;
};

} // namespace st::fcitx5
