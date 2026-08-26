// SPDX-License-Identifier: MIT
//
// The only class that both halves of the application know about.
//
// QML talks to it through properties and Q_INVOKABLE methods; the hook service
// talks to it through a callback that is marshalled onto the GUI thread with a
// queued connection. Neither side ever touches the other directly, which is
// what keeps the hook off the Qt event loop and the Qt event loop out of the
// OS keyboard callback.

#pragma once

#include "app/Settings.hpp"
#include "core/LanguageRuleEngine.hpp"
#include "core/german/GermanRuleEngine.hpp"
#include "hook/HookService.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

namespace st {

class AppController : public QObject {
    Q_OBJECT

    // --- master state -------------------------------------------------------
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString languageId READ languageId WRITE setLanguageId NOTIFY languageChanged)
    Q_PROPERTY(QVariantList languages READ languages NOTIFY languagesChanged)
    Q_PROPERTY(QString trayBadge READ trayBadge NOTIFY trayBadgeChanged)
    Q_PROPERTY(QString languageName READ languageName NOTIFY languageChanged)

    // --- shortcut -----------------------------------------------------------
    Q_PROPERTY(QString cycleChord READ cycleChord WRITE setCycleChord NOTIFY cycleChordChanged)
    Q_PROPERTY(QString cycleChordName READ cycleChordName NOTIFY cycleChordChanged)
    Q_PROPERTY(QVariantList cycleChordOptions READ cycleChordOptions CONSTANT)

    // --- diagnostics --------------------------------------------------------
    Q_PROPERTY(QString hookState READ hookState NOTIFY statusChanged)
    Q_PROPERTY(QString hookMessage READ hookMessage NOTIFY statusChanged)
    Q_PROPERTY(bool hookRunning READ hookRunning NOTIFY statusChanged)
    Q_PROPERTY(QString permissionState READ permissionState NOTIFY statusChanged)
    Q_PROPERTY(QString permissionDetail READ permissionDetail NOTIFY statusChanged)
    Q_PROPERTY(bool permissionActionable READ permissionActionable NOTIFY statusChanged)
    Q_PROPERTY(QString injectorBackend READ injectorBackend NOTIFY statusChanged)
    Q_PROPERTY(QString platformName READ platformName CONSTANT)
    Q_PROPERTY(bool canSuppress READ canSuppress NOTIFY statusChanged)
    Q_PROPERTY(QString configDirectory READ configDirectory CONSTANT)
    Q_PROPERTY(QString customRulesSummary READ customRulesSummary NOTIFY customRulesChanged)

    // --- Vietnamese ---------------------------------------------------------
    Q_PROPERTY(QString vnMethod READ vnMethod WRITE setVnMethod NOTIFY vnOptionsChanged)
    Q_PROPERTY(bool vnModernTone READ vnModernTone WRITE setVnModernTone NOTIFY vnOptionsChanged)
    Q_PROPERTY(bool vnSpellCheck READ vnSpellCheck WRITE setVnSpellCheck NOTIFY vnOptionsChanged)
    Q_PROPERTY(bool vnAutoRestore READ vnAutoRestore WRITE setVnAutoRestore NOTIFY vnOptionsChanged)

    // --- German -------------------------------------------------------------
    Q_PROPERTY(bool deUmlauts READ deUmlauts WRITE setDeUmlauts NOTIFY deOptionsChanged)
    Q_PROPERTY(bool deSharpS READ deSharpS WRITE setDeSharpS NOTIFY deOptionsChanged)
    Q_PROPERTY(bool deCapitalSharpS READ deCapitalSharpS WRITE setDeCapitalSharpS
                   NOTIFY deOptionsChanged)
    Q_PROPERTY(bool deDoubleKeyRevert READ deDoubleKeyRevert WRITE setDeDoubleKeyRevert
                   NOTIFY deOptionsChanged)
    Q_PROPERTY(bool deUseExceptions READ deUseExceptions WRITE setDeUseExceptions
                   NOTIFY deOptionsChanged)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    /// Wire up the hook and restore the persisted configuration. Called once
    /// from main() after the QML engine exists.
    void initialise();

    /// Override the persisted "start the hook on launch" setting for this run
    /// only, without writing it back. This is what --no-hook drives: it splits
    /// the program in half for diagnosis without changing the user's config.
    void setAutoStartHook(bool value) { autoStartHook_ = value; }

    // --- properties ---------------------------------------------------------
    [[nodiscard]] bool         enabled() const { return enabled_; }
    void                       setEnabled(bool enabled);
    [[nodiscard]] QString      languageId() const { return languageId_; }
    void                       setLanguageId(const QString& id);
    [[nodiscard]] QString      languageName() const;
    [[nodiscard]] QVariantList languages() const { return languages_; }
    [[nodiscard]] QString      trayBadge() const;

    /// The cycle shortcut, as "ctrl+shift" / "ctrl+alt" / "alt+shift" / "none".
    [[nodiscard]] QString      cycleChord() const;
    void                       setCycleChord(const QString& chord);
    /// The same thing spelled for humans, e.g. "Ctrl + Shift".
    [[nodiscard]] QString      cycleChordName() const;
    /// [{ value, label }, …] for the settings window.
    [[nodiscard]] QVariantList cycleChordOptions() const;

    [[nodiscard]] QString hookState() const;
    [[nodiscard]] QString hookMessage() const { return QString::fromStdString(status_.message); }
    [[nodiscard]] bool    hookRunning() const;
    [[nodiscard]] QString permissionState() const;
    [[nodiscard]] QString permissionDetail() const
    {
        return QString::fromStdString(status_.permission.detail);
    }
    [[nodiscard]] bool    permissionActionable() const;
    [[nodiscard]] QString injectorBackend() const
    {
        return QString::fromStdString(status_.injectorBackend);
    }
    [[nodiscard]] QString platformName() const;
    [[nodiscard]] bool    canSuppress() const { return status_.canSuppress; }
    [[nodiscard]] QString configDirectory() const { return Settings::configDirectory(); }
    [[nodiscard]] QString customRulesSummary() const { return customRulesSummary_; }

    [[nodiscard]] QString vnMethod() const;
    void                  setVnMethod(const QString& method);
    [[nodiscard]] bool    vnModernTone() const { return vnOptions_.modernTone; }
    void                  setVnModernTone(bool value);
    [[nodiscard]] bool    vnSpellCheck() const { return vnOptions_.spellCheck; }
    void                  setVnSpellCheck(bool value);
    [[nodiscard]] bool    vnAutoRestore() const { return vnOptions_.autoRestore; }
    void                  setVnAutoRestore(bool value);

    [[nodiscard]] bool deUmlauts() const { return deOptions_.umlauts; }
    void               setDeUmlauts(bool value);
    [[nodiscard]] bool deSharpS() const { return deOptions_.sharpS; }
    void               setDeSharpS(bool value);
    [[nodiscard]] bool deCapitalSharpS() const { return deOptions_.capitalSharpS; }
    void               setDeCapitalSharpS(bool value);
    [[nodiscard]] bool deDoubleKeyRevert() const { return deOptions_.doubleKeyRevert; }
    void               setDeDoubleKeyRevert(bool value);
    [[nodiscard]] bool deUseExceptions() const { return deOptions_.useExceptions; }
    void               setDeUseExceptions(bool value);

    // --- actions ------------------------------------------------------------
public slots:
    void toggleEnabled();
    /// Step through the configured languages — the tray's middle-click action.
    void cycleLanguage();
    /// Step through the languages and then Off, wrapping back to the first
    /// language. This is what the keyboard chord drives.
    void cycleLanguageOrOff();
    void requestPermissions();
    void openPermissionSettings();
    void openConfigDirectory();
    void reloadCustomRules();
    void showSettings();
    void quit();

signals:
    void enabledChanged();
    void languageChanged();
    void languagesChanged();
    void trayBadgeChanged();
    void statusChanged();
    void vnOptionsChanged();
    void deOptionsChanged();
    void customRulesChanged();
    void cycleChordChanged();
    /// Emitted when the tray or a second instance asks for the window.
    void settingsWindowRequested();

private:
    void rebuildLanguageList();
    void applyEngine();
    void persist();
    void onStatus(HookService::Status status);

    Settings                         settings_;
    std::unique_ptr<HookService>     hook_;
    HookService::Status              status_;
    QVariantList                     languages_;
    QString                          languageId_;
    QString                          customRulesSummary_;
    bool                             enabled_ = true;
    bool                             autoStartHook_ = true;

    // Mirrors of the engine option structs so QML can bind to them without the
    // engine having to exist yet.
    struct VnOptions {
        QString method      = QStringLiteral("telex");
        bool    modernTone  = false;
        bool    spellCheck  = true;
        bool    autoRestore = true;
    } vnOptions_;

    struct DeOptions {
        bool umlauts         = true;
        bool sharpS          = true;
        bool capitalSharpS   = false;
        bool doubleKeyRevert = true;
        bool useExceptions   = true;
    } deOptions_;

    GermanRuleEngine::ExceptionTable germanExceptions_;
};

} // namespace st
