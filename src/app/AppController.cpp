// SPDX-License-Identifier: MIT
#include "app/AppController.hpp"

#include "app/RuleSetLoader.hpp"
#include "core/RuleEngineRegistry.hpp"
#include "core/vietnamese/VietnameseRuleEngine.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QMetaObject>
#include <QUrl>
#include <QVariantMap>

namespace st {

AppController::AppController(QObject* parent)
    : QObject(parent), hook_(std::make_unique<HookService>())
{
    germanExceptions_ = GermanRuleEngine::builtinExceptions();
}

AppController::~AppController()
{
    if (hook_)
        hook_->stop();
}

// ---------------------------------------------------------------------------

void AppController::initialise()
{
    RuleEngineRegistry::instance().registerBuiltins();

    // Restore the persisted state *before* loading the custom rule files: the
    // reload falls back to German when the selected language is missing, and
    // doing that against an empty languageId_ would overwrite the user's
    // stored choice on every launch.
    enabled_    = settings_.enabled();
    languageId_ = settings_.languageId();

    // Restore engine options.
    const EngineOptions vn = settings_.engineOptions(QStringLiteral("vi"));
    if (const auto it = vn.find("method"); it != vn.end())
        vnOptions_.method = QString::fromStdString(it->second);
    auto boolOpt = [](const EngineOptions& map, const char* key, bool fallback) {
        const auto it = map.find(key);
        if (it == map.end())
            return fallback;
        return it->second == "true" || it->second == "1";
    };
    vnOptions_.modernTone  = boolOpt(vn, "modernTone", vnOptions_.modernTone);
    vnOptions_.spellCheck  = boolOpt(vn, "spellCheck", vnOptions_.spellCheck);
    vnOptions_.autoRestore = boolOpt(vn, "autoRestore", vnOptions_.autoRestore);

    const EngineOptions de = settings_.engineOptions(QStringLiteral("de"));
    deOptions_.umlauts         = boolOpt(de, "umlauts", deOptions_.umlauts);
    deOptions_.sharpS          = boolOpt(de, "sharpS", deOptions_.sharpS);
    deOptions_.capitalSharpS   = boolOpt(de, "capitalSharpS", deOptions_.capitalSharpS);
    deOptions_.doubleKeyRevert = boolOpt(de, "doubleKeyRevert", deOptions_.doubleKeyRevert);
    deOptions_.useExceptions   = boolOpt(de, "useExceptions", deOptions_.useExceptions);

    const QString configDir = Settings::configDirectory();
    RuleSetLoader::ensureConfigDirectory(configDir);
    reloadCustomRules();

    // The hook thread calls back from outside the GUI thread; bounce onto it.
    hook_->setStatusCallback([this](HookService::Status status) {
        QMetaObject::invokeMethod(
            this, [this, status = std::move(status)] { onStatus(status); },
            Qt::QueuedConnection);
    });

    applyEngine();
    hook_->setEnabled(enabled_);

    status_ = hook_->status();
    emit statusChanged();

    if (settings_.startHookOnLaunch())
        hook_->start();

    emit enabledChanged();
    emit languageChanged();
    emit trayBadgeChanged();
    emit vnOptionsChanged();
    emit deOptionsChanged();
}

void AppController::onStatus(HookService::Status status)
{
    status_ = std::move(status);
    emit statusChanged();
    emit trayBadgeChanged();
}

// ---------------------------------------------------------------------------

void AppController::rebuildLanguageList()
{
    languages_.clear();
    for (const EngineDescriptor& descriptor : RuleEngineRegistry::instance().descriptors()) {
        QVariantMap entry;
        entry[QStringLiteral("id")]    = QString::fromStdString(descriptor.id);
        entry[QStringLiteral("name")]  = QString::fromStdString(descriptor.displayName);
        entry[QStringLiteral("badge")] = QString::fromStdString(descriptor.badge);
        languages_.append(entry);
    }
    emit languagesChanged();
}

void AppController::applyEngine()
{
    EnginePtr engine = RuleEngineRegistry::instance().create(languageId_.toStdString());
    if (!engine)
        return;

    if (auto* german = dynamic_cast<GermanRuleEngine*>(engine.get())) {
        GermanRuleEngine::Options options;
        options.umlauts         = deOptions_.umlauts;
        options.sharpS          = deOptions_.sharpS;
        options.capitalSharpS   = deOptions_.capitalSharpS;
        options.doubleKeyRevert = deOptions_.doubleKeyRevert;
        options.useExceptions   = deOptions_.useExceptions;
        german->setGermanOptions(options);
        german->setExceptions(germanExceptions_);
    } else if (auto* vietnamese = dynamic_cast<VietnameseRuleEngine*>(engine.get())) {
        VietnameseRuleEngine::Options options;
        options.method      = vnOptions_.method == QStringLiteral("vni") ? VnInputMethod::Vni
                                                                        : VnInputMethod::Telex;
        options.modernTone  = vnOptions_.modernTone;
        options.spellCheck  = vnOptions_.spellCheck;
        options.autoRestore = vnOptions_.autoRestore;
        vietnamese->setVnOptions(options);
    } else {
        engine->applyOptions(settings_.engineOptions(languageId_));
    }

    hook_->setEngine(std::move(engine));
}

void AppController::persist()
{
    settings_.setEnabled(enabled_);
    settings_.setLanguageId(languageId_);

    EngineOptions vn;
    vn["method"]      = vnOptions_.method.toStdString();
    vn["modernTone"]  = vnOptions_.modernTone ? "true" : "false";
    vn["spellCheck"]  = vnOptions_.spellCheck ? "true" : "false";
    vn["autoRestore"] = vnOptions_.autoRestore ? "true" : "false";
    settings_.setEngineOptions(QStringLiteral("vi"), vn);

    EngineOptions de;
    de["umlauts"]         = deOptions_.umlauts ? "true" : "false";
    de["sharpS"]          = deOptions_.sharpS ? "true" : "false";
    de["capitalSharpS"]   = deOptions_.capitalSharpS ? "true" : "false";
    de["doubleKeyRevert"] = deOptions_.doubleKeyRevert ? "true" : "false";
    de["useExceptions"]   = deOptions_.useExceptions ? "true" : "false";
    settings_.setEngineOptions(QStringLiteral("de"), de);

    settings_.sync();
}

// ---------------------------------------------------------------------------

void AppController::setEnabled(bool enabled)
{
    if (enabled_ == enabled)
        return;
    enabled_ = enabled;
    hook_->setEnabled(enabled_);
    persist();
    emit enabledChanged();
    emit trayBadgeChanged();
}

void AppController::toggleEnabled() { setEnabled(!enabled_); }

void AppController::setLanguageId(const QString& id)
{
    if (languageId_ == id || !RuleEngineRegistry::instance().contains(id.toStdString()))
        return;
    languageId_ = id;
    applyEngine();
    persist();
    emit languageChanged();
    emit trayBadgeChanged();
}

void AppController::cycleLanguage()
{
    if (languages_.isEmpty())
        return;
    int index = 0;
    for (int i = 0; i < languages_.size(); ++i) {
        if (languages_.at(i).toMap().value(QStringLiteral("id")).toString() == languageId_) {
            index = i;
            break;
        }
    }
    const int next = (index + 1) % languages_.size();
    setLanguageId(languages_.at(next).toMap().value(QStringLiteral("id")).toString());
}

QString AppController::languageName() const
{
    for (const QVariant& entry : languages_) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == languageId_)
            return map.value(QStringLiteral("name")).toString();
    }
    return languageId_;
}

QString AppController::trayBadge() const
{
    if (!enabled_ || !hookRunning())
        return QStringLiteral("OFF");
    for (const QVariant& entry : languages_) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("id")).toString() == languageId_)
            return map.value(QStringLiteral("badge")).toString();
    }
    return QStringLiteral("--");
}

// ---------------------------------------------------------------------------

QString AppController::hookState() const
{
    switch (status_.state) {
    case HookService::State::Running:  return QStringLiteral("running");
    case HookService::State::Starting: return QStringLiteral("starting");
    case HookService::State::Failed:   return QStringLiteral("failed");
    default:                           return QStringLiteral("stopped");
    }
}

bool AppController::hookRunning() const
{
    return status_.state == HookService::State::Running;
}

QString AppController::permissionState() const
{
    switch (status_.permission.state) {
    case platform::PermissionState::Granted:     return QStringLiteral("granted");
    case platform::PermissionState::Denied:      return QStringLiteral("denied");
    case platform::PermissionState::NotRequired: return QStringLiteral("notRequired");
    default:                                     return QStringLiteral("unknown");
    }
}

bool AppController::permissionActionable() const
{
    return status_.permission.state == platform::PermissionState::Denied
           && !status_.permission.settingsUri.empty();
}

QString AppController::platformName() const
{
    return QString::fromStdString(platform::sessionTypeName());
}

// ---------------------------------------------------------------------------

void AppController::requestPermissions()
{
    const platform::PermissionInfo info = platform::requestPermissions();
    status_.permission                  = info;
    emit statusChanged();
    if (info.state != platform::PermissionState::Denied && !hookRunning())
        hook_->start();
}

void AppController::openPermissionSettings()
{
    if (status_.permission.settingsUri.empty())
        return;
    QDesktopServices::openUrl(QUrl(QString::fromStdString(status_.permission.settingsUri)));
}

void AppController::openConfigDirectory()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(Settings::configDirectory()));
}

void AppController::reloadCustomRules()
{
    const QString configDir = Settings::configDirectory();

    LoadReport report;
    RuleEngineRegistry::instance().clearRuleSets();
    RuleEngineRegistry::instance().registerBuiltins();
    RuleSetLoader::loadRuleSets(configDir, report);
    RuleSetLoader::loadLayouts(configDir, hook_->keyMapper(), report);
    germanExceptions_ = RuleSetLoader::loadGermanExceptions(configDir, report);

    customRulesSummary_ =
        report.errors.isEmpty()
            ? tr("%1 rule set(s), %2 layout override(s), %3 extra exception word(s).")
                  .arg(report.ruleSets)
                  .arg(report.layouts)
                  .arg(report.exceptionWords)
            : report.errors.join(QStringLiteral("\n"));

    rebuildLanguageList();
    if (!languageId_.isEmpty()
        && !RuleEngineRegistry::instance().contains(languageId_.toStdString()))
        setLanguageId(QStringLiteral("de"));
    else
        applyEngine();

    emit customRulesChanged();
}

void AppController::showSettings() { emit settingsWindowRequested(); }

void AppController::quit()
{
    persist();
    if (hook_)
        hook_->stop();
    QCoreApplication::quit();
}

// ---------------------------------------------------------------------------

QString AppController::vnMethod() const { return vnOptions_.method; }

void AppController::setVnMethod(const QString& method)
{
    const QString normalised =
        method.compare(QStringLiteral("vni"), Qt::CaseInsensitive) == 0 ? QStringLiteral("vni")
                                                                       : QStringLiteral("telex");
    if (vnOptions_.method == normalised)
        return;
    vnOptions_.method = normalised;
    applyEngine();
    persist();
    emit vnOptionsChanged();
}

#define ST_SIMPLE_SETTER(setter, field, signalName)                                               \
    void AppController::setter(bool value)                                                        \
    {                                                                                             \
        if (field == value)                                                                       \
            return;                                                                               \
        field = value;                                                                            \
        applyEngine();                                                                            \
        persist();                                                                                \
        emit signalName();                                                                        \
    }

ST_SIMPLE_SETTER(setVnModernTone, vnOptions_.modernTone, vnOptionsChanged)
ST_SIMPLE_SETTER(setVnSpellCheck, vnOptions_.spellCheck, vnOptionsChanged)
ST_SIMPLE_SETTER(setVnAutoRestore, vnOptions_.autoRestore, vnOptionsChanged)
ST_SIMPLE_SETTER(setDeUmlauts, deOptions_.umlauts, deOptionsChanged)
ST_SIMPLE_SETTER(setDeSharpS, deOptions_.sharpS, deOptionsChanged)
ST_SIMPLE_SETTER(setDeCapitalSharpS, deOptions_.capitalSharpS, deOptionsChanged)
ST_SIMPLE_SETTER(setDeDoubleKeyRevert, deOptions_.doubleKeyRevert, deOptionsChanged)
ST_SIMPLE_SETTER(setDeUseExceptions, deOptions_.useExceptions, deOptionsChanged)

#undef ST_SIMPLE_SETTER

} // namespace st
