// SPDX-License-Identifier: MIT
#include "app/TrayController.hpp"

#include "app/AppController.hpp"
#include "app/Diagnostics.hpp"

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

/// How many language actions to create up front. Two built-ins plus room for
/// custom rule sets; a hidden QAction costs a few hundred bytes, so being
/// generous here is free and overflowing is the case we want never to hit.
constexpr qsizetype kLanguageActionPool = 16;

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

    // Every language action this process will ever own is created here, while
    // the menu is still a plain widget. The next line is what turns it into a
    // native menu; after that, adding items is the operation that killed us.
    // See rebuildLanguageMenu().
    growLanguageActions(kLanguageActionPool);

    tray_->setContextMenu(menu_);
    connect(tray_, &QSystemTrayIcon::activated, this, &TrayController::onActivated);

    connect(&controller_, &AppController::enabledChanged, this, &TrayController::refresh);
    connect(&controller_, &AppController::languageChanged, this, &TrayController::refresh);
    connect(&controller_, &AppController::statusChanged, this, &TrayController::refresh);
    connect(&controller_, &AppController::languagesChanged, this,
            &TrayController::rebuildLanguageMenu);
    // The submenu title carries the shortcut, so it has to follow it.
    connect(&controller_, &AppController::cycleChordChanged, this,
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

/// Create language actions until there are at least `count` of them.
///
/// Kept separate from rebuildLanguageMenu() so the constructor can call it at
/// the one moment when adding to this menu is unambiguously safe: before
/// QSystemTrayIcon::setContextMenu() has given the menu a native counterpart.
void TrayController::growLanguageActions(qsizetype count)
{
    while (languageActions_.size() < count) {
        QAction* action = languageMenu_->addAction(QString());
        action->setCheckable(true);
        action->setVisible(false);
        languageGroup_->addAction(action);
        // The id is read back from the action when it fires rather than
        // captured now, so relabelling an action also re-targets it.
        connect(action, &QAction::triggered, this,
                [this, action] { controller_.setLanguageId(action->data().toString()); });
        languageActions_.append(action);
    }
}

void TrayController::rebuildLanguageMenu()
{
    if (languageMenu_ == nullptr)
        return;
    diagnostics::milestone(QStringLiteral("      tray: rebuilding language menu"));

    const QVariantList languages = controller_.languages();
    diagnostics::milestone(
        QStringLiteral("      tray: read %1 language(s)").arg(languages.size()));

    // This function must not add or remove menu items, and the pool built in
    // the constructor is what lets it avoid both.
    //
    // QSystemTrayIcon gives its context menu a *native* platform menu —
    // QWindowsPopupMenu on Windows — and that propagates into submenus. Adding
    // or removing a QAction afterwards makes Qt create or destroy the matching
    // native item from inside the ActionAdded/ActionRemoved event, and that is
    // exactly where this application died. The startup log is unambiguous: at
    // construction the language list was still empty, so this function did
    // nothing and the process was fine; the first time it had three languages
    // to insert, the process vanished mid-function with no structured
    // exception reaching the crash handler at all.
    //
    // Relabelling is a different path — ActionChanged — and the same log proves
    // it safe: refresh() drives it on every status change and always returns.
    // So the actions are made once, up front, and from here on only ever have
    // their text, data and visibility changed.
    if (languages.size() > languageActions_.size()) {
        qWarning("tray: %lld languages exceeds the pool of %lld; growing it",
                 static_cast<long long>(languages.size()),
                 static_cast<long long>(languageActions_.size()));
        growLanguageActions(languages.size());
    }
    diagnostics::milestone(
        QStringLiteral("      tray: %1 action(s) available").arg(languageActions_.size()));

    for (qsizetype i = 0; i < languageActions_.size(); ++i) {
        QAction* action = languageActions_.at(i);
        if (i < languages.size()) {
            const QVariantMap map = languages.at(i).toMap();
            action->setData(map.value(QStringLiteral("id")).toString());
            action->setText(map.value(QStringLiteral("name")).toString());
            action->setVisible(true);
        } else {
            action->setData(QString());
            action->setVisible(false);
        }
    }
    // Advertise the shortcut where people look for it. Empty when the chord is
    // switched off, so the submenu just reads "Language".
    const QString chord = controller_.cycleChordName();
    languageMenu_->setTitle(chord == QStringLiteral("None")
                                ? tr("Language")
                                : tr("Language  (%1)").arg(chord));

    diagnostics::milestone(QStringLiteral("      tray: menu items created"));
    refresh();
    diagnostics::milestone(QStringLiteral("      tray: language menu rebuilt"));
}

void TrayController::refresh()
{
    if (tray_ == nullptr)
        return;

    const QString badge  = controller_.trayBadge();
    const bool    active = controller_.enabled() && controller_.hookRunning();

    diagnostics::milestone(QStringLiteral("      tray: setting icon"));
    tray_->setIcon(renderBadge(badge, active));

    QString tip = QStringLiteral("schnellerTyp-e — %1").arg(controller_.languageName());
    if (!active)
        tip += QStringLiteral("\n%1").arg(controller_.hookMessage());
    tray_->setToolTip(tip);

    if (enabledAction_ != nullptr) {
        QSignalBlocker blocker(enabledAction_);
        enabledAction_->setChecked(controller_.enabled());
    }

    diagnostics::milestone(QStringLiteral("      tray: syncing check states"));
    const auto actions = languageGroup_->actions();
    for (QAction* action : actions) {
        QSignalBlocker blocker(action);
        action->setChecked(action->data().toString() == controller_.languageId());
    }
    diagnostics::milestone(QStringLiteral("      tray: refreshed"));
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
