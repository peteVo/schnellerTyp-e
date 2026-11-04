// SPDX-License-Identifier: MIT
#include "SetupChecks.hpp"
#include "app/Settings.hpp"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QLibraryInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>

namespace st::fcitx5 {
namespace {
QString read(const QString& path) {
    if (!QFileInfo(path).isFile()) return {};
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.read(1024 * 1024)) : QString();
}
QString environmentState(const char* name) {
    const auto value = qEnvironmentVariable(name);
    return value == QStringLiteral("fcitx") ? QStringLiteral("configured")
         : value.isEmpty() ? QStringLiteral("missing") : QStringLiteral("conflicting");
}
QVariant controller(const QString& method) {
    const auto request = QDBusMessage::createMethodCall(QStringLiteral("org.fcitx.Fcitx5"),
        QStringLiteral("/controller"), QStringLiteral("org.fcitx.Fcitx.Controller1"), method);
    const auto reply = QDBusConnection::sessionBus().call(request, QDBus::Block, 750);
    return reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() ? reply.arguments().first() : QVariant();
}
}

QJsonObject SetupReport::toJson() const
{
    // Construct from an explicit allowlist. Never serialize settings, environment,
    // process arguments, paths, client IDs, surrounding text or diagnostic logs.
    QJsonArray entries;
    for (const auto& check : checks)
        entries.append(QJsonObject{{"check", check.title}, {"state", check.state}, {"detail", check.detail}});
    return {{"schemaVersion", 1}, {"appVersion", QStringLiteral(SCHNELLERTYPE_VERSION)},
        {"qtVersion", QString::fromLatin1(qVersion())}, {"os", QSysInfo::productType()},
        {"osVersion", QSysInfo::productVersion()}, {"architecture", QSysInfo::currentCpuArchitecture()},
        {"status", status}, {"checks", entries}};
}

SetupReport collectSetupReport()
{
    SetupReport report;
    const auto current = controller(QStringLiteral("CurrentInputMethod"));
    const auto active = controller(QStringLiteral("State"));
    Settings settings;
    const auto mode = settings.languageId();
    const bool supported = mode == QStringLiteral("auto") || mode == QStringLiteral("de") || mode == QStringLiteral("vi");
    report.status = !current.isValid() || !active.isValid() ? QStringLiteral("Unavailable — Fcitx is not responding")
        : current.toString() != QStringLiteral("schnellerTyp-e") ? QStringLiteral("Not selected in the last focused field")
        : !settings.enabled() || active.toInt() != 2 ? QStringLiteral("Paused")
        : !supported ? QStringLiteral("Unsupported saved profile — literal input")
        : QStringLiteral("Selected — check the Fcitx status label for this application's mode");
    const auto add = [&](QString title, QString state, QString detail) {
        report.checks.append({std::move(title), std::move(state), std::move(detail)});
    };
    add(QStringLiteral("Fcitx connection"), current.isValid() && active.isValid() ? QStringLiteral("ready") : QStringLiteral("unavailable"),
        QStringLiteral("Use the existing Omarchy Fcitx service; select schnellerTyp-e in its input-method menu."));
    QProcess service;
    service.start(QStringLiteral("systemctl"), {QStringLiteral("--user"), QStringLiteral("is-enabled"), QStringLiteral("omarchy-fcitx5.service")});
    const bool finished = service.waitForFinished(750);
    const bool enabled = finished && service.exitCode() == 0;
    if (!finished) { service.kill(); service.waitForFinished(100); }
    add(QStringLiteral("Login startup"), enabled ? QStringLiteral("enabled") : QStringLiteral("check required"),
        QStringLiteral("Run systemctl --user enable --now omarchy-fcitx5.service if disabled. Reuse this service; do not add a second autostart."));
    add(QStringLiteral("Saved profile"), supported ? QStringLiteral("supported") : QStringLiteral("unsupported"),
        QStringLiteral("Native modes are Automatic, German and Vietnamese. Custom JSON profiles pass through literally."));
    for (const auto* name : {"GTK_IM_MODULE", "QT_IM_MODULE"})
        add(QString::fromLatin1(name), environmentState(name), QStringLiteral("Use fcitx. Existing applications must be reopened after environment changes."));
    const auto plugins = QDir(QLibraryInfo::path(QLibraryInfo::PluginsPath) + QStringLiteral("/platforminputcontexts"))
        .entryList({QStringLiteral("*fcitx*.so")}, QDir::Files);
    add(QStringLiteral("Qt integration"), plugins.isEmpty() ? QStringLiteral("missing") : QStringLiteral("installed"),
        QStringLiteral("Install fcitx5-qt if the Qt input-context plugin is missing."));
    const QDir lib(QStringLiteral(SCHNELLERTYPE_SYSTEM_LIBDIR));
    const bool gtk3 = QFileInfo::exists(lib.filePath(QStringLiteral("gtk-3.0/3.0.0/immodules/im-fcitx5.so")));
    const bool gtk4 = QFileInfo::exists(lib.filePath(QStringLiteral("gtk-4.0/4.0.0/immodules/libim-fcitx5.so")));
    add(QStringLiteral("GTK integration"), gtk3 && gtk4 ? QStringLiteral("installed") : QStringLiteral("missing components"),
        QStringLiteral("Install fcitx5-gtk; Chromium's GTK 4 route needs its GTK 4 module."));
    const auto config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    const auto flags = read(QDir(config).filePath(QStringLiteral("chromium-flags.conf"))).split(QLatin1Char('\n'));
    bool x11 = false, gtk = false, conflict = false;
    for (const auto& line : flags) {
        const auto flag = line.trimmed();
        x11 |= flag == QStringLiteral("--ozone-platform=x11");
        gtk |= flag == QStringLiteral("--gtk-version=4");
        conflict |= flag == QStringLiteral("--enable-wayland-ime")
            || flag.startsWith(QStringLiteral("--ozone-platform-hint="))
            || (flag.startsWith(QStringLiteral("--ozone-platform=")) && flag != QStringLiteral("--ozone-platform=x11"))
            || (flag.startsWith(QStringLiteral("--gtk-version=")) && flag != QStringLiteral("--gtk-version=4"))
            || flag.startsWith(QStringLiteral("--wayland-text-input-version="));
    }
    add(QStringLiteral("Chromium launch"), x11 && gtk && !conflict ? QStringLiteral("supported") : QStringLiteral("repair available"),
        QStringLiteral("Use XWayland with --ozone-platform=x11 --gtk-version=4. Native Chromium Wayland remains experimental."));
    bool legacy = false;
    for (const auto& pid : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool numeric = false;
        pid.toLongLong(&numeric);
        if (numeric && read(QStringLiteral("/proc/") + pid + QStringLiteral("/comm")).trimmed() == QStringLiteral("schnellerTyp-e"))
            legacy |= !read(QStringLiteral("/proc/") + pid + QStringLiteral("/cmdline")).contains(QStringLiteral("--no-hook"));
    }
    add(QStringLiteral("Other input processor"), legacy ? QStringLiteral("review required") : QStringLiteral("no legacy app detected"),
        QStringLiteral("Pause or quit the legacy desktop keyboard hook when using the native Fcitx source. Process detection cannot prove that a hook is active."));
    return report;
}

bool repairSetupFiles(const QString& configDirectory, QString& message)
{
    QDir root(configDirectory);
    if (QFileInfo(root.filePath(QStringLiteral("environment.d"))).isSymLink()) {
        message = QStringLiteral("The environment directory is managed by a symlink. Edit it manually; no files were changed.");
        return false;
    }
    if (!root.mkpath(QStringLiteral("environment.d"))) {
        message = QStringLiteral("Could not create the user environment directory.");
        return false;
    }
    const auto environment = root.filePath(QStringLiteral("environment.d/20-schnellertype.conf"));
    const auto chromium = root.filePath(QStringLiteral("chromium-flags.conf"));
    for (const auto& path : {environment, chromium}) {
        const QFileInfo info(path);
        if (info.isSymLink() || (info.exists() && !info.isFile()) || info.size() > 1024 * 1024) {
            message = QStringLiteral("A setup path is managed by a symlink, is not a regular file, or exceeds the repair size limit. Edit it manually; no files were changed.");
            return false;
        }
    }
    QList<QPair<QString, QString>> updates;
    QStringList lines;
    for (const auto& line : read(environment).split(QLatin1Char('\n')))
        if (!line.trimmed().startsWith(QStringLiteral("GTK_IM_MODULE=")) && !line.trimmed().startsWith(QStringLiteral("QT_IM_MODULE=")) && !line.isEmpty()) lines.append(line);
    lines << QStringLiteral("GTK_IM_MODULE=fcitx") << QStringLiteral("QT_IM_MODULE=fcitx");
    updates.append({environment, lines.join(QLatin1Char('\n')) + QLatin1Char('\n')});
    lines.clear();
    for (const auto& line : read(chromium).split(QLatin1Char('\n'))) {
        const auto flag = line.trimmed();
        if (!flag.startsWith(QStringLiteral("--ozone-platform=")) && !flag.startsWith(QStringLiteral("--ozone-platform-hint="))
            && !flag.startsWith(QStringLiteral("--gtk-version=")) && !flag.startsWith(QStringLiteral("--wayland-text-input-version="))
            && flag != QStringLiteral("--enable-wayland-ime") && !line.isEmpty()) lines.append(line);
    }
    lines << QStringLiteral("--ozone-platform=x11") << QStringLiteral("--gtk-version=4");
    updates.append({chromium, lines.join(QLatin1Char('\n')) + QLatin1Char('\n')});
    const auto suffix = QStringLiteral(".bak.") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    for (const auto& update : updates) {
        QFile existing(update.first);
        if (existing.exists() && (!existing.open(QIODevice::ReadOnly) || !QFile::copy(update.first, update.first + suffix))) {
            message = QStringLiteral("A configuration file could not be read or backed up. No setup files were changed.");
            return false;
        }
    }
    for (const auto& update : updates) {
        QSaveFile file(update.first);
        const auto data = update.second.toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            message = QStringLiteral("Could not save setup files. Existing files were backed up before repair; inspect file permissions.");
            return false;
        }
    }
    message = QStringLiteral("Saved toolkit environment and Chromium flags with backups. Log in again for the environment, then reopen applications.");
    return true;
}
} // namespace st::fcitx5
