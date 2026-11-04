// SPDX-License-Identifier: MIT
#include "platform/fcitx5/SetupChecks.hpp"
#include "app/Settings.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <cstdio>

namespace {
bool write(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray read(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}
int main(int argc, char** argv)
{
    QTemporaryDir temporary(QDir::currentPath() + QStringLiteral("/setup-test-XXXXXX"));
    if (!temporary.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", temporary.path().toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent-schnellertype-test-bus");
    qputenv("GTK_IM_MODULE", "PRIVATE_ENV_SENTINEL");
    QCoreApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, temporary.path());
    st::Settings settings;
    settings.setLanguageId(QStringLiteral("PRIVATE_PROFILE_SENTINEL"));
    settings.setEngineOptions(QStringLiteral("auto"), {{"literalWords", "PRIVATE_WORD_SENTINEL"}});
    settings.setApplicationModes({{"PRIVATE_APP_SENTINEL", "literal"}});
    settings.sync();
    int checks = 0, failures = 0;
    const auto check = [&](const char* name, bool condition) {
        ++checks;
        if (!condition) { std::printf("FAIL %s\n", name); ++failures; }
    };
    const auto report = st::fcitx5::collectSetupReport();
    const auto json = QJsonDocument(report.toJson()).toJson();
    check("unavailable Fcitx reported", report.status.startsWith(QStringLiteral("Unavailable")));
    check("report excludes settings, env and paths", !json.contains("PRIVATE_") && !json.contains(temporary.path().toUtf8()));
    check("report schema has only allowlisted top-level fields", report.toJson().size() == 8
          && report.toJson().value(QStringLiteral("schemaVersion")).toInt() == 1);
    const auto root = temporary.path() + QStringLiteral("/config");
    QDir().mkpath(root + QStringLiteral("/environment.d"));
    const auto env = root + QStringLiteral("/environment.d/20-schnellertype.conf");
    const auto flags = root + QStringLiteral("/chromium-flags.conf");
    const QByteArray oldEnv = "# user settings\nOTHER=keep-me\nGTK_IM_MODULE=other\nQT_IM_MODULE=other\n";
    const QByteArray oldFlags = "# existing flags\n--force-device-scale-factor=1.2\n--ozone-platform=wayland\n--gtk-version=3\n--enable-wayland-ime\n";
    check("fixture files created", write(env, oldEnv) && write(flags, oldFlags));
    QString message;
    check("setup repair succeeds", st::fcitx5::repairSetupFiles(root, message));
    check("environment preserves unrelated content", read(env) == "# user settings\nOTHER=keep-me\nGTK_IM_MODULE=fcitx\nQT_IM_MODULE=fcitx\n");
    check("flags preserve unrelated content", read(flags) == "# existing flags\n--force-device-scale-factor=1.2\n--ozone-platform=x11\n--gtk-version=4\n");
    const auto envBackups = QDir(root + QStringLiteral("/environment.d")).entryList({QStringLiteral("*.bak.*")}, QDir::Files);
    const auto flagBackups = QDir(root).entryList({QStringLiteral("*.bak.*")}, QDir::Files);
    check("both original files backed up", envBackups.size() == 1 && flagBackups.size() == 1
          && read(QDir(root + QStringLiteral("/environment.d")).filePath(envBackups.first())) == oldEnv
          && read(QDir(root).filePath(flagBackups.first())) == oldFlags);
    const auto repaired = read(flags);
    check("repeat repair preserves content", st::fcitx5::repairSetupFiles(root, message) && read(flags) == repaired);
    QFile::remove(flags);
    const auto target = root + QStringLiteral("/managed-flags");
    check("symlink fixture created", write(target, oldFlags) && QFile::link(target, flags));
    const auto before = read(env);
    check("managed symlink refused without overwriting either file", !st::fcitx5::repairSetupFiles(root, message)
          && read(target) == oldFlags && read(env) == before);
    QFile::remove(flags);
    check("large fixture created", write(flags, QByteArray(1024 * 1024 + 1, 'x')));
    check("oversized file refused before editing", !st::fcitx5::repairSetupFiles(root, message) && read(env) == before);
    QFile::remove(flags);
    QDir().mkpath(flags);
    check("non-file path refused before editing", !st::fcitx5::repairSetupFiles(root, message) && read(env) == before);
    std::printf("%d setup and privacy checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
