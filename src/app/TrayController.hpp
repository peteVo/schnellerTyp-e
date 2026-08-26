// SPDX-License-Identifier: MIT
//
// System tray integration.
//
// The icon is drawn at runtime rather than shipped as a set of PNGs, so a
// language added through a JSON rule file gets a correct tray badge for free —
// the badge text comes straight from the engine descriptor.

#pragma once

#include <QIcon>
#include <QObject>
#include <QPointer>
#include <QSystemTrayIcon>

class QAction;
class QActionGroup;
class QMenu;

namespace st {

class AppController;

class TrayController : public QObject {
    Q_OBJECT

public:
    TrayController(AppController& controller, QObject* parent = nullptr);
    ~TrayController() override;

    [[nodiscard]] bool isAvailable() const;
    void               show();

    /// Renders the "DE" / "VN" / "OFF" badge. Public because the settings
    /// window shows the same glyph next to the master switch.
    [[nodiscard]] static QIcon renderBadge(const QString& text, bool active, int size = 64);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);
    void refresh();
    void rebuildLanguageMenu();

private:
    AppController&           controller_;
    QSystemTrayIcon*         tray_ = nullptr;
    QMenu*                   menu_ = nullptr;
    QMenu*                   languageMenu_ = nullptr;
    QActionGroup*            languageGroup_ = nullptr;
    QAction*                 enabledAction_ = nullptr;
};

} // namespace st
