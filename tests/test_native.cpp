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
    const auto check = [&](const char* name, bool condition) {
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
    check("unknown mode recovers", st::loadNativeEngine()->id() == "auto");
    std::printf("6 native settings checks, %d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
