// SPDX-License-Identifier: MIT
#include "app/NativeEngine.hpp"
#include "app/Settings.hpp"
#include "core/CompositionSession.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir temporary(QDir::currentPath() + QStringLiteral("/native-settings-XXXXXX"));
    if (!temporary.isValid())
        return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
    QSettings isolation(QSettings::defaultFormat(), QSettings::UserScope,
                        QStringLiteral("schnellerTyp-e"), QStringLiteral("schnellerTyp-e"));
    if (!isolation.fileName().startsWith(temporary.path())) {
        std::printf("Refusing to run outside the temporary settings profile\n");
        return 1;
    }

    int failures = 0;
    int checks = 0;
    const auto check = [&](const char* name, bool condition) {
        ++checks;
        if (!condition) {
            std::printf("FAIL %s\n", name);
            ++failures;
        }
    };
    st::Settings settings;
    auto engine = st::loadNativeEngine();
    check("new-install defaults to Automatic", engine && engine->id() == "auto");

    settings.setEnabled(false);
    settings.sync();
    check("disabled native input passes through", !st::loadNativeEngine());

    settings.setEnabled(true);
    settings.setEngineOptions(QStringLiteral("auto"), {{"literalWords", "hoom"}});
    settings.sync();
    st::CompositionSession session(st::loadNativeEngine());
    for (const auto c : U"hoom")
        if (c != 0)
            (void)session.processKey(st::KeyEvent::character_(c));
    check("personal literal preference shared", session.finish().text == U"hoom");

    settings.setLanguageId(QStringLiteral("vi"));
    settings.setEngineOptions(QStringLiteral("vi"), {{"method", "vni"}});
    settings.sync();
    engine = st::loadNativeEngine();
    check("explicit saved mode preserved", engine && engine->id() == "vi");
    st::CompositionSession vni(std::move(engine));
    for (const auto c : U"to6i")
        if (c != 0)
            (void)vni.processKey(st::KeyEvent::character_(c));
    check("Vietnamese options shared", vni.finish().text == U"tôi");

    settings.setLanguageId(QStringLiteral("missing"));
    settings.sync();
    check("unsupported profile passes through literally", !st::loadNativeEngine());
    check("app preference saves", settings.setApplicationModes({{"editor", "literal"}, {"chat", "de"}}));
    settings.sync();
    check("literal app bypasses unsupported global profile", !st::loadNativeEngine("editor"));
    check("built-in app override works with unsupported global profile", st::loadNativeEngine("chat")->id() == "de");
    check("unknown app does not silently choose Automatic", !st::loadNativeEngine("other"));
    check("invalid app mode rejected", !settings.setApplicationModes({{"editor", "custom"}}));
    check("invalid app ID rejected", !settings.setApplicationModes({{"bad name", "auto"}}));
    check("rejected map preserves preferences", settings.applicationModes().at("editor") == "literal");
    settings.setLanguageId(QStringLiteral("auto"));
    settings.setEngineOptions(QStringLiteral("auto"), {{"literalWords", ""}});
    check("literal word saves", settings.addLiteralWord(QStringLiteral("hoom")));
    check("case-insensitive learning is idempotent", settings.addLiteralWord(QStringLiteral("HOOM"))
          && settings.engineOptions(QStringLiteral("auto")).at("literalWords") == "hoom");
    check("phrases and addresses cannot be learned", !settings.addLiteralWord(QStringLiteral("two words"))
          && !settings.addLiteralWord(QStringLiteral("test@example.com")));
    st::CompositionSession learned(st::loadNativeEngine());
    for (const auto c : U"hoom") if (c) (void)learned.processKey(st::KeyEvent::character_(c));
    check("learned word is literal after reload", learned.finish().text == U"hoom");
    std::printf("%d native settings checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
