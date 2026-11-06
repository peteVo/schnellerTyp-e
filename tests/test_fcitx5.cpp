// SPDX-License-Identifier: MIT
#include <fcitx/instance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>

#include <fcitx/inputcontext.h>
#include <fcitx/focusgroup.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/action.h>
#include <fcitx/userinterfacemanager.h>

#include "app/Settings.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

#include <cstdio>
#include <string>

// An in-memory client exercises real Fcitx callbacks without connecting to a
// display, owning the session bus, or sending keys to desktop applications.
class Client final : public fcitx::InputContext {
public:
    explicit Client(fcitx::InputContextManager &manager, const std::string& program = {}) : InputContext(manager, program)
    {
        setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
        created();
    }
    ~Client() override { destroy(); }
    const char *frontend() const override { return "schnellertype-test"; }

    std::string committed;
    std::string preedit;
    int cursor = 0;
    bool commitHadPreedit = false;

private:
    void commitStringImpl(const std::string &text) override
    {
        commitHadPreedit = !preedit.empty();
        committed += text;
    }
    void deleteSurroundingTextImpl(int, unsigned int) override {}
    void forwardKeyImpl(const fcitx::ForwardKeyEvent &) override {}
    void updatePreeditImpl() override
    {
        preedit = inputPanel().clientPreedit().toString();
        cursor = inputPanel().clientPreedit().cursor();
    }
};

int main(int argc, char **argv)
{
    if (argc != 4)
        return 1;
    QTemporaryDir temporary(QDir::currentPath() + QStringLiteral("/fcitx5-test-XXXXXX"));
    if (!temporary.isValid())
        return 1;
    for (const auto *variable : {"XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME"})
        qputenv(variable, temporary.path().toUtf8());
    const auto data = temporary.path() + QStringLiteral("/data");
    QDir().mkpath(data + QStringLiteral("/fcitx5/addon"));
    QDir().mkpath(data + QStringLiteral("/fcitx5/inputmethod"));
    if (!QFile::copy(QString::fromLocal8Bit(argv[1]), data + QStringLiteral("/fcitx5/addon/schnellerTyp-e-fcitx5.conf"))
        || !QFile::copy(QString::fromLocal8Bit(argv[2]), data + QStringLiteral("/fcitx5/inputmethod/schnellerTyp-e.conf")))
        return 1;
    qputenv("XDG_DATA_DIRS", data.toUtf8() + ":/usr/local/share:/usr/share");
    qputenv("FCITX_ADDON_DIRS", argv[3]);
    QCoreApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
    QSettings isolation(QSettings::IniFormat, QSettings::UserScope,
                        QStringLiteral("schnellerTyp-e"), QStringLiteral("schnellerTyp-e"));
    if (!isolation.fileName().startsWith(temporary.path()))
        return 1;

    char program[] = "schnellertype-fcitx5-test";
    char disable[] = "--disable=all";
    char enable[] = "--enable=schnellerTyp-e-fcitx5";
    char *arguments[] = {program, disable, enable, nullptr};
    fcitx::Instance instance(3, arguments);
    instance.addonManager().registerDefaultLoader(nullptr);
    instance.initialize();
    auto *loaded = dynamic_cast<fcitx::InputMethodEngine *>(
        instance.addonManager().addon("schnellerTyp-e-fcitx5", true));
    if (!loaded) {
        std::puts("FAIL loading the built shared addon through Fcitx");
        return 1;
    }
    auto &engine = *loaded;
    const fcitx::InputMethodEntry entry("schnellerTyp-e", "schnellerTyp-e", "en",
                                       "schnellerTyp-e-fcitx5");
    fcitx::FocusGroup group("test", instance.inputContextManager());
    Client client(instance.inputContextManager());
    Client other(instance.inputContextManager());
    client.setFocusGroup(&group);
    other.setFocusGroup(&group);
    const auto activate = [&](Client &target) {
        fcitx::InputContextEvent event(&target, fcitx::EventType::InputContextFocusIn);
        engine.activate(entry, event);
    };
    const auto reset = [&](Client &target) {
        fcitx::InputContextEvent event(&target, fcitx::EventType::InputContextReset);
        engine.reset(entry, event);
        target.committed.clear();
    };
    const auto key = [&](Client &target, const fcitx::Key &value, bool release = false) {
        fcitx::KeyEvent event(&target, value, release);
        engine.keyEvent(entry, event);
        return event.accepted();
    };
    const auto type = [&](Client &target, const std::string &text) {
        for (unsigned char c : text)
            key(target, fcitx::Key(static_cast<fcitx::KeySym>(c)));
    };
    int checks = 0;
    int failures = 0;
    const auto check = [&](const char *name, bool condition) {
        ++checks;
        if (!condition) {
            std::printf("FAIL %s\n", name);
            ++failures;
        }
    };

    activate(client);
    type(client, "hoom");
    check("Vietnamese is held in preedit", client.preedit == "hôm" && client.committed.empty());
    check("cursor follows UTF-8 preedit", client.cursor == static_cast<int>(client.preedit.size()));
    check("key release passes through", !key(client, fcitx::Key(FcitxKey_m), true)
          && client.preedit == "hôm");
    check("Space is consumed", key(client, fcitx::Key(FcitxKey_space)));
    check("commit precedes preedit clearing", client.commitHadPreedit);
    check("Space commits and clears preedit", client.committed == "hôm " && client.preedit.empty());
    reset(client);
    type(client, "tooi ddeens Berlin und moechte coffee ");
    check("mixed-language sentence", client.committed == "tôi đến Berlin und möchte coffee ");
    reset(client);
    type(client, "nuawx ");
    check("delayed Telex marks", client.committed == "nữa ");
    reset(client);
    type(client, "ww ");
    check("literal undo", client.committed == "w ");
    reset(client);
    type(client, "hoom");
    check("Escape is consumed", key(client, fcitx::Key(FcitxKey_Escape)));
    key(client, fcitx::Key(FcitxKey_space));
    check("Escape restores literal input", client.committed == "hoom ");
    reset(client);
    type(client, "hoom");
    key(client, fcitx::Key(FcitxKey_BackSpace));
    check("Backspace edits composition", client.preedit == "hô");
    reset(client);
    check("empty Backspace passes through", !key(client, fcitx::Key(FcitxKey_BackSpace)));
    for (const auto sym : {FcitxKey_Return, FcitxKey_Tab, FcitxKey_Up, FcitxKey_Page_Down}) {
        reset(client);
        type(client, "hoom");
        check("boundary passes through", !key(client, fcitx::Key(sym)));
        check("boundary commits once", client.committed == "hôm" && client.preedit.empty());
    }
    reset(client);
    type(client, "hoom");
    check("shortcut passes through", !key(client, fcitx::Key(FcitxKey_c, fcitx::KeyState::Ctrl)));
    check("shortcut commits pending text", client.committed == "hôm" && client.preedit.empty());
    reset(client);
    type(client, "hoom");
    check("Left edits uncommitted word", key(client, fcitx::Key(FcitxKey_Left))
          && client.preedit == "hôm" && client.committed.empty() && client.cursor == 3);
    key(client, fcitx::Key(FcitxKey_BackSpace));
    check("Backspace at composition caret", client.preedit == "hm" && client.cursor == 1 && client.committed.empty());
    check("undo composition shortcut consumed", key(client, fcitx::Key(FcitxKey_z,
          fcitx::KeyStates(fcitx::KeyState::Ctrl) | fcitx::KeyState::Alt)));
    check("undo restores text and UTF-8 cursor", client.preedit == "hôm" && client.cursor == 3 && client.committed.empty());
    key(client, fcitx::Key(FcitxKey_Delete));
    check("Delete at composition caret", client.preedit == "hô" && client.committed.empty());
    key(client, fcitx::Key(FcitxKey_space));
    check("edited composition commits once", client.committed == "hô " && client.preedit.empty());
    reset(client);
    type(client, "as");
    check("word override shortcut consumed", key(client, fcitx::Key(FcitxKey_v,
          fcitx::KeyStates(fcitx::KeyState::Ctrl) | fcitx::KeyState::Alt)));
    check("word override previews before commit", client.preedit == "á" && client.committed.empty());
    key(client, fcitx::Key(FcitxKey_space));
    type(client, "as ");
    check("override expires after current word", client.committed == "á as ");
    reset(client);
    type(client, "hoom");
    other.focusIn();
    activate(other);
    type(other, "tooi");
    check("fields have independent preedit", client.preedit == "hôm" && other.preedit == "tôi");
    reset(client);
    check("reset discards without committing", client.preedit.empty() && client.committed.empty()
          && other.preedit == "tôi");
    other.focusOut();
    check("focus out commits original field", other.committed == "tôi" && other.preedit.empty()
          && client.committed.empty());
    for (const auto flag : {fcitx::CapabilityFlag::Password, fcitx::CapabilityFlag::Sensitive, fcitx::CapabilityFlag::Disable}) {
        reset(client);
        type(client, "hoom");
        client.setCapabilityFlags(fcitx::CapabilityFlags(fcitx::CapabilityFlag::Preedit) | flag);
        check("sensitive transition discards composition", client.preedit.empty() && client.committed.empty());
        check("sensitive keys pass through", !key(client, fcitx::Key(FcitxKey_a))
              && client.preedit.empty() && client.committed.empty());
        for (const auto command : {FcitxKey_v, FcitxKey_z})
            check("sensitive composition commands pass through", !key(client, fcitx::Key(command,
                  fcitx::KeyStates(fcitx::KeyState::Ctrl) | fcitx::KeyState::Alt))
                  && client.preedit.empty() && client.committed.empty());
        client.setCapabilityFlags(fcitx::CapabilityFlag::Preedit);
    }
    type(client, "hoom");
    fcitx::InputContextEvent deactivate(&client, fcitx::EventType::InputContextFocusOut);
    engine.deactivate(entry, deactivate);
    check("deactivate commits", client.committed == "hôm" && client.preedit.empty());
    check("inactive keys pass through", !key(client, fcitx::Key(FcitxKey_a)));

    st::Settings settings;
    settings.setEnabled(false);
    settings.sync();
    activate(client);
    check("disabled preference passes through", !key(client, fcitx::Key(FcitxKey_a)));
    settings.setEnabled(true);
    settings.setLanguageId(QStringLiteral("vi"));
    settings.setEngineOptions(QStringLiteral("vi"), {{"method", "vni"}});
    settings.sync();
    reset(client);
    activate(client);
    type(client, "to6i ");
    check("activation reloads saved VNI settings", client.committed == "tôi ");

    fcitx::RawConfig saved;
    check("native configuration is available", engine.getConfig() != nullptr);
    engine.getConfig()->save(saved);
    check("configuration reads shared mode", saved.get("Mode")->value() == "Vietnamese"
          && saved.get("Method")->value() == "VNI");
    fcitx::RawConfig update;
    update.setValueByPath("Mode", "Automatic");
    update.setValueByPath("Method", "Telex");
    update.setValueByPath("LiteralWords", "hoom");
    engine.setConfig(update);
    reset(client);
    type(client, "hoom ");
    check("native configuration applies to existing fields", client.committed == "hoom ");
    check("native configuration persists to shared settings",
          st::Settings().languageId() == QStringLiteral("auto")
          && st::Settings().engineOptions(QStringLiteral("auto")).at("literalWords") == "hoom");
    update.setValueByPath("Mode", "invalid");
    engine.setConfig(update);
    check("invalid mode keeps prior selection", st::Settings().languageId() == QStringLiteral("auto"));
    update.setValueByPath("Mode", "Automatic");
    update.setValueByPath("Enabled", "False");
    engine.setConfig(update);
    check("native disable takes effect immediately", !key(client, fcitx::Key(FcitxKey_a)));

    auto* status = instance.userInterfaceManager().lookupAction("schnellertype-status");
    auto* pause = instance.userInterfaceManager().lookupAction("schnellertype-enabled");
    auto* learn = instance.userInterfaceManager().lookupAction("schnellertype-learn-literal");
    check("native actions registered", status && pause && learn
          && instance.userInterfaceManager().lookupAction("schnellertype-setup"));
    if (!status || !pause || !learn) return 1;
    check("paused state visible", status->shortText(&client) == "Paused");
    pause->activate(&client);
    check("native enable action persists", st::Settings().enabled()
          && status->shortText(&client) == "Active — Automatic");
    auto* forceWord = instance.userInterfaceManager().lookupAction("schnellertype-word-vietnamese");
    auto* undoWord = instance.userInterfaceManager().lookupAction("schnellertype-word-undo");
    check("native word commands registered", forceWord && undoWord);
    if (!forceWord || !undoWord) return 1;
    reset(client);
    type(client, "as");
    forceWord->activate(&client);
    check("native action overrides only the word", client.preedit == "á" && client.committed.empty()
          && status->shortText(&client).find("Vietnamese for this word") != std::string::npos
          && st::Settings().languageId() == QStringLiteral("auto"));
    undoWord->activate(&client);
    check("native action undoes override", client.preedit == "as" && client.committed.empty()
          && status->shortText(&client) == "Active — Automatic");
    reset(client);
    update = fcitx::RawConfig();
    update.setValueByPath("LiteralWords", "");
    engine.setConfig(update);
    reset(client);
    type(client, "hoom");
    check("learn shortcut consumes only active word", key(client, fcitx::Key(FcitxKey_l,
          fcitx::KeyStates(fcitx::KeyState::Ctrl) | fcitx::KeyState::Alt)));
    check("learn restores raw word once", client.committed == "hoom" && client.preedit.empty());
    check("learn persists raw spelling", st::Settings().engineOptions(QStringLiteral("auto")).at("literalWords") == "hoom");
    key(client, fcitx::Key(FcitxKey_space));
    type(client, "hoom ");
    check("learned spelling survives next word", client.committed == "hoom hoom ");
    reset(client);
    type(client, "tooi");
    learn->activate(&client);
    check("native learn action uses same path", client.committed == "tooi" && client.preedit.empty());
    const auto literals = st::Settings().engineOptions(QStringLiteral("auto")).at("literalWords");
    client.setCapabilityFlags(fcitx::CapabilityFlags(fcitx::CapabilityFlag::Preedit) | fcitx::CapabilityFlag::Password);
    type(client, "secret");
    learn->activate(&client);
    check("sensitive words cannot be learned", st::Settings().engineOptions(QStringLiteral("auto")).at("literalWords") == literals);
    check("sensitive status visible", status->shortText(&client).find("Sensitive") == 0);
    client.setCapabilityFlags(fcitx::CapabilityFlag::Preedit);

    reset(client);
    type(client, "test@example.com");
    learn->activate(&client);
    check("address-like input cannot be learned", st::Settings().engineOptions(QStringLiteral("auto")).at("literalWords") == literals);
    check("learning does not truncate address", client.committed + client.preedit == "test@example.com");
    reset(client);

    Client editor(instance.inputContextManager(), "test.editor");
    Client chat(instance.inputContextManager(), "test.chat");
    update.setValueByPath("ApplicationModes", "test.editor=literal; test.chat=de");
    engine.setConfig(update);
    activate(editor);
    activate(chat);
    check("app ID visible locally for exact preferences", status->longText(&editor).find("test.editor") != std::string::npos);
    check("literal app passes keys through", !key(editor, fcitx::Key(FcitxKey_h))
          && editor.preedit.empty() && status->shortText(&editor) == "Literal for this application");
    type(chat, "moechte ");
    check("app-specific German mode", chat.committed == "möchte "
          && status->shortText(&chat) == "Active — German");
    update.setValueByPath("ApplicationModes", "test.editor=de; test.editor=literal");
    engine.setConfig(update);
    auto modes = st::Settings().applicationModes();
    check("duplicate app preference rejected without losing saved map", modes.contains("test.editor") && modes.at("test.editor") == "literal");
    update.setValueByPath("ApplicationModes", "test.editor=custom");
    engine.setConfig(update);
    modes = st::Settings().applicationModes();
    check("unsupported app mode rejected", modes.contains("test.editor") && modes.at("test.editor") == "literal");
    settings.setLanguageId(QStringLiteral("custom-profile"));
    settings.sync();
    activate(client);
    check("unsupported global profile visible and literal", !key(client, fcitx::Key(FcitxKey_h))
          && status->shortText(&client).find("Unsupported saved profile") == 0);
    update = fcitx::RawConfig();
    update.setValueByPath("Enabled", "True");
    engine.setConfig(update);
    check("unrelated config edit preserves custom profile", st::Settings().languageId() == QStringLiteral("custom-profile"));
    update.setValueByPath("Mode", "invalid");
    engine.setConfig(update);
    check("invalid mode preserves nondefault profile", st::Settings().languageId() == QStringLiteral("custom-profile"));
    update.setValueByPath("Mode", "Automatic");
    update.setValueByPath("LiteralWords", "");
    engine.setConfig(update);
    reset(client);
    type(client, "hoom");
    check("AltGr modifier does not finish composition", !key(client, fcitx::Key(FcitxKey_ISO_Level3_Shift)) && client.preedit == "hôm");
    key(client, fcitx::Key(FcitxKey_EuroSign, fcitx::KeyState::Mod5));
    key(client, fcitx::Key(FcitxKey_space));
    check("AltGr printable symbol survives", client.committed.find("€") != std::string::npos);
    reset(client);
    key(client, fcitx::Key(FcitxKey_udiaeresis));
    key(client, fcitx::Key(FcitxKey_space));
    check("non-US Unicode key survives", client.committed == "ü ");
    reset(client);
    for (const auto sym : {FcitxKey_X, FcitxKey_Y, FcitxKey_Z})
        key(client, fcitx::Key(sym, fcitx::KeyState::CapsLock));
    key(client, fcitx::Key(FcitxKey_space));
    check("Caps Lock symbols retain case", client.committed == "XYZ ");

    std::printf("%d Fcitx adapter checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
