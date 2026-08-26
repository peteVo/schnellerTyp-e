// SPDX-License-Identifier: MIT
//
// schnellerTyp-e — entry point.
//
// QApplication (not QGuiApplication) because QSystemTrayIcon lives in
// QtWidgets. The settings window itself is QML; the only widget in the process
// is the tray icon and its context menu, which is the right trade: native menus
// on all three platforms for about forty lines of C++.

#include "app/AppController.hpp"
#include "app/TrayController.hpp"

#include <QApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>

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

    // The window is a settings panel; closing it must not end the process.
    QApplication::setQuitOnLastWindowClosed(false);
    QQuickStyle::setStyle(QStringLiteral("Basic"));  // we style everything ourselves

    QLocalServer instanceServer;
    if (!claimSingleInstance(instanceServer))
        return 0;

    st::AppController controller;
    QApplication::setWindowIcon(st::TrayController::renderBadge(QStringLiteral("ST"), true, 128));

    QQmlApplicationEngine engine;
    qmlRegisterSingletonInstance("SchnellerTypE.Backend", 1, 0, "App", &controller);
    // The URL form rather than loadFromModule() so the project also builds
    // against Qt 6.4, which is what Debian 12 and Ubuntu 24.04 ship.
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/SchnellerTypE/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    st::TrayController tray(controller);
    tray.show();

    // Bring the window up when a second launch asks for it.
    QObject::connect(&instanceServer, &QLocalServer::newConnection, &controller, [&] {
        if (QLocalSocket* socket = instanceServer.nextPendingConnection()) {
            socket->deleteLater();
            controller.showSettings();
        }
    });

    // Start the hook after the event loop is up so the first status callback
    // has somewhere to land.
    QTimer::singleShot(0, &controller, [&controller, &tray] {
        controller.initialise();
        if (!tray.isAvailable())
            controller.showSettings();  // no tray: keep the window as the only UI
    });

    return QApplication::exec();
}
