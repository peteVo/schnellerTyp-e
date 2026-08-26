// SPDX-License-Identifier: MIT
//
// schnellerTyp-e — entry point.
//
// QApplication (not QGuiApplication) because QSystemTrayIcon lives in
// QtWidgets. The settings window itself is QML; the only widget in the process
// is the tray icon and its context menu, which is the right trade: native menus
// on all three platforms for about forty lines of C++.
//
// Startup is deliberately narrated through st::diagnostics::milestone(). This
// is a windowless, console-less tray application: when it dies early the only
// thing the host can tell you is that it "terminated abnormally", and the
// milestone log is what turns that into a location.

#include "app/AppController.hpp"
#include "app/Diagnostics.hpp"
#include "app/TrayController.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

#include <memory>

namespace {

constexpr auto kSingleInstanceKey = "schnellerTyp-e.instance";

/// Returns false when another instance is already running; in that case it is
/// asked to show its settings window and this process should exit.
bool claimSingleInstance(QLocalServer& server)
{
    QLocalSocket probe;
    probe.connectToServer(QString::fromLatin1(kSingleInstanceKey));
    if (probe.waitForConnected(200)) {
        probe.write("show");
        probe.waitForBytesWritten(200);
        probe.disconnectFromServer();
        return false;
    }

    // A stale socket file is left behind by an unclean exit on Unix.
    QLocalServer::removeServer(QString::fromLatin1(kSingleInstanceKey));
    server.listen(QString::fromLatin1(kSingleInstanceKey));
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("schnellerTyp-e"));
    QCoreApplication::setApplicationName(QStringLiteral("schnellerTyp-e"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SCHNELLERTYPE_VERSION));

    st::diagnostics::install();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Input method editor: German umlauts and Vietnamese Telex/VNI."));
    const QCommandLineOption noHookOption(
        QStringLiteral("no-hook"),
        QStringLiteral("Start without installing the keyboard hook. Use this to tell a fault in "
                       "the Qt/QML half from one in the hook and injection half."));
    const QCommandLineOption noTrayOption(
        QStringLiteral("no-tray"),
        QStringLiteral("Do not create the system tray icon at all; show the settings window "
                       "instead. Pairs with --no-hook to bisect a startup fault."));
    parser.addOption(noHookOption);
    parser.addOption(noTrayOption);
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    const bool startHook = !parser.isSet(noHookOption);
    const bool useTray   = !parser.isSet(noTrayOption);
    if (!startHook)
        qInfo("--no-hook: the keyboard hook will not be installed");
    if (!useTray)
        qInfo("--no-tray: the tray icon will not be created");

    // The window is a settings panel; closing it must not end the process.
    QApplication::setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle(QStringLiteral("Basic"));  // we style everything ourselves

    st::diagnostics::milestone(QStringLiteral("claiming single instance"));
    QLocalServer instanceServer;
    if (!claimSingleInstance(instanceServer)) {
        qInfo("another instance is already running; asked it to show its window");
        return 0;
    }

    st::diagnostics::milestone(QStringLiteral("constructing AppController"));
    st::AppController controller;

    st::diagnostics::milestone(QStringLiteral("rendering window icon"));
    QApplication::setWindowIcon(st::TrayController::renderBadge(QStringLiteral("ST"), true, 128));

    st::diagnostics::milestone(QStringLiteral("loading QML"));
    QQmlApplicationEngine engine;
    qmlRegisterSingletonInstance("SchnellerTypE.Backend", 1, 0, "App", &controller);
    // The URL form rather than loadFromModule() so the project also builds
    // against Qt 6.4, which is what Debian 12 and Ubuntu 24.04 ship.
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/SchnellerTypE/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical("QML failed to load; see the messages above");
        return 1;
    }

    std::unique_ptr<st::TrayController> tray;
    if (useTray) {
        st::diagnostics::milestone(QStringLiteral("creating tray icon"));
        tray = std::make_unique<st::TrayController>(controller);
        tray->show();
        st::diagnostics::milestone(QStringLiteral("tray icon created"));
    }

    // Bring the window up when a second launch asks for it.
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &controller, [&] {
        if (QLocalSocket* socket = instanceServer.nextPendingConnection()) {
            socket->deleteLater();
            controller.showSettings();
        }
    });

    // Start the hook after the event loop is up so the first status callback
    // has somewhere to land.
    QTimer::singleShot(0, &controller, [&controller, &tray, startHook] {
        st::diagnostics::milestone(QStringLiteral("initialising"));
        controller.setAutoStartHook(startHook);
        controller.initialise();
        st::diagnostics::milestone(QStringLiteral("initialised"));

        if (!tray || !tray->isAvailable())
            controller.showSettings();  // no tray: keep the window as the only UI
    });

    st::diagnostics::milestone(QStringLiteral("entering event loop"));
    return QApplication::exec();
}
