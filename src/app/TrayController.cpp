// SPDX-License-Identifier: MIT
#include "app/TrayController.hpp"

#include "app/AppController.hpp"

#include <QAction>
#include <QActionGroup>
#include <QFont>
#include <QFontMetricsF>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSignalBlocker>
#include <QVariantMap>

namespace st {
namespace {

// Same palette as the QML side, so the tray badge does not look like it belongs
// to a different application.
const QColor kActiveBackground(0x6D, 0x28, 0xD9);   // violet-700
const QColor kActiveText(0xFA, 0xFA, 0xFA);
const QColor kInactiveBackground(0x3F, 0x3F, 0x46); // zinc-700
const QColor kInactiveText(0xA1, 0xA1, 0xAA);       // zinc-400

} // namespace

// ---------------------------------------------------------------------------

TrayController::TrayController(AppController& controller, QObject* parent)
    : QObject(parent), controller_(controller)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    tray_ = new QSystemTrayIcon(this);
    menu_ = new QMenu();

    auto* settingsAction = menu_->addAction(tr("Open Settings…"));
    connect(settingsAction, &QAction::triggered, &controller_, &AppController::showSettings);

    menu_->addSeparator();

    enabledAction_ = menu_->addAction(tr("Enabled"));
    enabledAction_->setCheckable(true);
    connect(enabledAction_, &QAction::toggled, &controller_, &AppController::setEnabled);

    languageMenu_  = menu_->addMenu(tr("Language"));
    languageGroup_ = new QActionGroup(this);
    languageGroup_->setExclusive(true);

    menu_->addSeparator();

    auto* reloadAction = menu_->addAction(tr("Reload custom rules"));
    connect(reloadAction, &QAction::triggered, &controller_, &AppController::reloadCustomRules);

    auto* configAction = menu_->addAction(tr("Open config folder"));
    connect(configAction, &QAction::triggered, &controller_, &AppController::openConfigDirectory);

    menu_->addSeparator();

    auto* quitAction = menu_->addAction(tr("Quit schnellerTyp-e"));
    connect(quitAction, &QAction::triggered, &controller_, &AppController::quit);

    tray_->setContextMenu(menu_);
    connect(tray_, &QSystemTrayIcon::activated, this, &TrayController::onActivated);

    connect(&controller_, &AppController::enabledChanged, this, &TrayController::refresh);
    connect(&controller_, &AppController::languageChanged, this, &TrayController::refresh);
    connect(&controller_, &AppController::statusChanged, this, &TrayController::refresh);
    connect(&controller_, &AppController::languagesChanged, this,
            &TrayController::rebuildLanguageMenu);

    rebuildLanguageMenu();
    refresh();
}

TrayController::~TrayController()
{
    delete menu_;  // not parented, so that it outlives no one unexpectedly
}

bool TrayController::isAvailable() const { return tray_ != nullptr; }

void TrayController::show()
{
    if (tray_ != nullptr)
        tray_->show();
}

// ---------------------------------------------------------------------------

void TrayController::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:      // left click: the master switch
        controller_.toggleEnabled();
        break;
    case QSystemTrayIcon::MiddleClick:  // middle click: next language
        controller_.cycleLanguage();
        break;
    case QSystemTrayIcon::DoubleClick:
        controller_.showSettings();
        break;
    default:
        break;
    }
}

void TrayController::rebuildLanguageMenu()
{
    if (languageMenu_ == nullptr)
        return;

    const auto actions = languageGroup_->actions();
    for (QAction* action : actions) {
        languageGroup_->removeAction(action);
        languageMenu_->removeAction(action);
        action->deleteLater();
    }

    const QVariantList languages = controller_.languages();
    for (const QVariant& entry : languages) {
        const QVariantMap map  = entry.toMap();
        const QString     id   = map.value(QStringLiteral("id")).toString();
        const QString     name = map.value(QStringLiteral("name")).toString();

        auto* action = languageMenu_->addAction(name);
        action->setCheckable(true);
        action->setData(id);
        languageGroup_->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, id] { controller_.setLanguageId(id); });
    }
    refresh();
}

void TrayController::refresh()
{
    if (tray_ == nullptr)
        return;

    const QString badge  = controller_.trayBadge();
    const bool    active = controller_.enabled() && controller_.hookRunning();

    tray_->setIcon(renderBadge(badge, active));

    QString tip = QStringLiteral("schnellerTyp-e — %1").arg(controller_.languageName());
    if (!active)
        tip += QStringLiteral("\n%1").arg(controller_.hookMessage());
    tray_->setToolTip(tip);

    if (enabledAction_ != nullptr) {
        QSignalBlocker blocker(enabledAction_);
        enabledAction_->setChecked(controller_.enabled());
    }

    const auto actions = languageGroup_->actions();
    for (QAction* action : actions) {
        QSignalBlocker blocker(action);
        action->setChecked(action->data().toString() == controller_.languageId());
    }
}

// ---------------------------------------------------------------------------

QIcon TrayController::renderBadge(const QString& text, bool active, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const qreal      radius = size * 0.24;
    const QRectF     body(size * 0.06, size * 0.06, size * 0.88, size * 0.88);
    QPainterPath     path;
    path.addRoundedRect(body, radius, radius);

    painter.fillPath(path, active ? kActiveBackground : kInactiveBackground);

    QFont font;
    font.setBold(true);
    font.setPixelSize(static_cast<int>(size * (text.size() > 2 ? 0.36 : 0.46)));
    painter.setFont(font);
    painter.setPen(active ? kActiveText : kInactiveText);
    painter.drawText(body, Qt::AlignCenter, text);
    painter.end();

    QIcon icon(pixmap);
    icon.setIsMask(false);
    return icon;
}

} // namespace st
