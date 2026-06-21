// AppController.cpp — Central controller implementation.
#include "AppController.h"
#include "ActiveWindowWatcher.h"
#include "DiscordPresenceClient.h"
#include "LocalContextServer.h"
#include "ConfigStore.h"
#include "TemplateEngine.h"
#include <QTimer>
#include <QDebug>
#include <QUrl>
#include <QUuid>
#include <QDir>
#include <QProcess>
#include <QDesktopServices>

namespace OmniPresence {

// ── Rule <-> QVariantMap mapping helpers ────────────────────────────────────
namespace {

QVariantMap ruleToMap(int index, const Rule& r) {
    return QVariantMap{
        {QStringLiteral("index"),                index},
        {QStringLiteral("id"),                   r.id},
        {QStringLiteral("name"),                 r.name},
        {QStringLiteral("enabled"),              r.enabled},
        {QStringLiteral("priority"),             r.priority},
        {QStringLiteral("matchProcessName"),     r.matchProcessName},
        {QStringLiteral("matchExecutablePath"),  r.matchExecutablePath},
        {QStringLiteral("matchWindowTitle"),     r.matchWindowTitle},
        {QStringLiteral("matchWindowTitleRegex"),r.matchWindowTitleRegex},
        {QStringLiteral("matchBrowserDomain"),   r.matchBrowserDomain},
        {QStringLiteral("matchBrowserCategory"), r.matchBrowserCategory},
        {QStringLiteral("matchIntegrationSource"),r.matchIntegrationSource},
        {QStringLiteral("activityNameTemplate"), r.activityNameTemplate},
        {QStringLiteral("detailsTemplate"),      r.detailsTemplate},
        {QStringLiteral("stateTemplate"),        r.stateTemplate},
        {QStringLiteral("largeImageKey"),        r.largeImageKey},
        {QStringLiteral("largeImageText"),       r.largeImageText},
        {QStringLiteral("smallImageKey"),        r.smallImageKey},
        {QStringLiteral("smallImageText"),       r.smallImageText},
        {QStringLiteral("timestampMode"),        int(r.timestampMode)},
        {QStringLiteral("privacyLevel"),         int(r.privacyLevel)},
    };
}

void applyField(Rule& r, const QString& f, const QVariant& v) {
    if      (f == QLatin1String("name"))                 r.name = v.toString();
    else if (f == QLatin1String("enabled"))              r.enabled = v.toBool();
    else if (f == QLatin1String("priority"))             r.priority = v.toInt();
    else if (f == QLatin1String("matchProcessName"))     r.matchProcessName = v.toString();
    else if (f == QLatin1String("matchExecutablePath"))  r.matchExecutablePath = v.toString();
    else if (f == QLatin1String("matchWindowTitle"))     r.matchWindowTitle = v.toString();
    else if (f == QLatin1String("matchWindowTitleRegex"))r.matchWindowTitleRegex = v.toBool();
    else if (f == QLatin1String("matchBrowserDomain"))   r.matchBrowserDomain = v.toString();
    else if (f == QLatin1String("matchBrowserCategory")) r.matchBrowserCategory = v.toString();
    else if (f == QLatin1String("matchIntegrationSource"))r.matchIntegrationSource = v.toString();
    else if (f == QLatin1String("activityNameTemplate")) r.activityNameTemplate = v.toString();
    else if (f == QLatin1String("detailsTemplate"))      r.detailsTemplate = v.toString();
    else if (f == QLatin1String("stateTemplate"))        r.stateTemplate = v.toString();
    else if (f == QLatin1String("largeImageKey"))        r.largeImageKey = v.toString();
    else if (f == QLatin1String("largeImageText"))       r.largeImageText = v.toString();
    else if (f == QLatin1String("smallImageKey"))        r.smallImageKey = v.toString();
    else if (f == QLatin1String("smallImageText"))       r.smallImageText = v.toString();
    else if (f == QLatin1String("timestampMode"))        r.timestampMode = TimestampMode(v.toInt());
    else if (f == QLatin1String("privacyLevel"))         r.privacyLevel = PrivacyLevel(v.toInt());
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , m_configStore(std::make_unique<ConfigStore>(this))
    , m_discordClient(std::make_unique<DiscordPresenceClient>(this))
    , m_contextServer(std::make_unique<LocalContextServer>(&m_integrationContext,
                                                           LocalContextServer::DEFAULT_PORT,
                                                           this))
{
    m_watcher = createActiveWindowWatcher(this);

    // ── Wire signals ──────────────────────────────────────────────────────────
    connect(m_watcher.get(), &ActiveWindowWatcher::activeWindowChanged,
            this,            &AppController::onActiveWindowChanged);

    connect(m_contextServer.get(), &LocalContextServer::contextUpdated,
            this,                  &AppController::onIntegrationContextUpdated);

    connect(m_discordClient.get(), &DiscordPresenceClient::connectionStatusChanged,
            this, [this](DiscordConnectionStatus status) {
                // Clear the stale error once a fresh attempt gets underway or succeeds.
                if (status == DiscordConnectionStatus::Connecting ||
                    status == DiscordConnectionStatus::Connected) {
                    m_discordError.clear();
                }
                emit discordStatusChanged();
            });

    connect(m_discordClient.get(), &DiscordPresenceClient::sdkError,
            this, [this](const QString& message) {
                m_discordError = message;
                emit discordStatusChanged();
            });

    // ── Discord SDK callback timer (100 ms) ───────────────────────────────────
    m_discordCallbackTimer = new QTimer(this);
    m_discordCallbackTimer->setInterval(100);
    connect(m_discordCallbackTimer, &QTimer::timeout,
            this,                   &AppController::onDiscordCallbackTimer);

    // ── "Capture next window" countdown timer (1 s ticks) ─────────────────────
    m_captureTimer = new QTimer(this);
    m_captureTimer->setInterval(1000);
    connect(m_captureTimer, &QTimer::timeout, this, &AppController::onCaptureTick);
}

AppController::~AppController() = default;

void AppController::initialise() {
    m_configStore->load();

    const AppSettings& settings = m_configStore->settings();
    if (!settings.discordAppId.isEmpty()) {
        m_discordClient->connectToDiscord(settings.discordAppId);
    }

    m_contextServer->start();
    m_watcher->start();
    m_discordCallbackTimer->start();
}

// ── Property getters ──────────────────────────────────────────────────────────

QString AppController::currentProcessName() const { return m_currentWindow.processName; }
QString AppController::currentWindowTitle()  const { return m_currentWindow.windowTitle; }
QString AppController::currentExePath()      const { return m_currentWindow.executablePath; }
QString AppController::currentWindowClass()  const { return m_currentWindow.windowClass; }
QString AppController::matchedRuleName()     const { return m_currentPresence.matchedRuleName; }
QString AppController::presenceName()        const { return m_currentPresence.name; }
QString AppController::presenceDetails()     const { return m_currentPresence.details; }
QString AppController::presenceState()       const { return m_currentPresence.state; }
bool    AppController::isPrivateFallback()   const { return m_currentPresence.isPrivateFallback; }
bool    AppController::discordConnected()    const { return m_discordClient->isConnected(); }

QString AppController::discordStatus() const {
    switch (m_discordClient->connectionStatus()) {
    case DiscordConnectionStatus::Connected:   return QStringLiteral("Connected");
    case DiscordConnectionStatus::Connecting:  return QStringLiteral("Connecting…");
    case DiscordConnectionStatus::Error:       return QStringLiteral("Error");
    case DiscordConnectionStatus::Disconnected:
    default:                                    return QStringLiteral("Disconnected");
    }
}

QString AppController::discordAppId() const {
    return m_configStore->settings().discordAppId;
}

QString AppController::lastUpdateTime() const {
    return m_lastUpdateTime.isValid()
        ? m_lastUpdateTime.toLocalTime().toString(QStringLiteral("HH:mm:ss"))
        : QStringLiteral("—");
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AppController::onActiveWindowChanged(const OmniPresence::WindowInfo& info) {
    m_currentWindow = info;
    emit windowChanged();
    evaluateAndPublish();
}

void AppController::onIntegrationContextUpdated(const QString& source) {
    Q_UNUSED(source)
    // Re-evaluate whenever integration data arrives.
    evaluateAndPublish();
}

void AppController::onDiscordCallbackTimer() {
    m_discordClient->runCallbacks();
}

// ── Core evaluation ───────────────────────────────────────────────────────────

void AppController::evaluateAndPublish() {
    const PresencePayload candidate = m_ruleEngine.evaluate(
        m_currentWindow,
        m_integrationContext,
        m_configStore->ruleSet(),
        m_overrideState,
        m_lastPublishedPresence);

    m_currentPresence = candidate;
    emit presenceChanged();

    // Skip the Discord API call if nothing changed.
    if (candidate.isSamePresence(m_lastPublishedPresence)) return;

    m_lastPublishedPresence = candidate;
    m_lastUpdateTime        = QDateTime::currentDateTimeUtc();
    m_discordClient->updatePresence(candidate);
}

// ── QML-invokable actions ─────────────────────────────────────────────────────

void AppController::captureCurrentWindow() {
    // Re-emit current window so the UI can populate a "create rule" form.
    emit windowChanged();
}

void AppController::beginCapture() {
    // A direct snapshot would only ever capture OmniPresence itself (it is the
    // focused window while you click the button). Instead, count down a few
    // seconds so the user can alt-tab / click the window they actually want,
    // then snapshot whatever is in the foreground at that moment.
    if (m_capturing) return;
    m_capturing = true;
    m_captureCountdown = 4;
    emit captureStateChanged();
    m_captureTimer->start();
}

void AppController::onCaptureTick() {
    --m_captureCountdown;
    if (m_captureCountdown > 0) {
        emit captureStateChanged();
        return;
    }

    // Countdown finished — grab the foreground window now.
    m_captureTimer->stop();
    m_capturing = false;
    emit captureStateChanged();

    WindowInfo snapshot = m_watcher->currentForegroundWindow();
    if (!snapshot.processName.isEmpty()) {
        m_currentWindow = snapshot;
        emit windowChanged();
        evaluateAndPublish();
    }
}

void AppController::connectDiscord() {
    const QString appId = m_configStore->settings().discordAppId;
    if (appId.isEmpty()) {
        m_discordError = QStringLiteral(
            "No Discord application ID configured. Set \"discordAppId\" in "
            "%APPDATA%\\OmniPresence\\config.json, then reload config.");
        emit discordStatusChanged();
        return;
    }
    m_discordClient->connectToDiscord(appId);
}

void AppController::disconnectDiscord() {
    m_discordClient->disconnectFromDiscord();
}

void AppController::publishTest() {
    // Force a presence push, ignoring the "unchanged" guard.
    m_lastPublishedPresence = {};
    evaluateAndPublish();
}

void AppController::setPrivacyMode(bool enabled) {
    if (m_overrideState.privateMode == enabled) return;
    m_overrideState.privateMode = enabled;
    emit privacyModeChanged();
    evaluateAndPublish();
}

void AppController::pause() {
    if (m_overrideState.paused) return;
    m_overrideState.paused = true;
    emit pauseChanged();
    evaluateAndPublish();
}

void AppController::resume() {
    if (!m_overrideState.paused) return;
    m_overrideState.paused = false;
    emit pauseChanged();
    evaluateAndPublish();
}

void AppController::saveConfig() {
    m_configStore->save();
}

void AppController::reloadConfig() {
    m_configStore->load();
    evaluateAndPublish();
}

// ── Art sources + available context (Task 5) ───────────────────────────────────

QString AppController::sourceForKey(const QString& key) const {
    if (key.isEmpty()) return {};
    const QString local = m_artStore.localPathForKey(key);
    if (!local.isEmpty()) return QUrl::fromLocalFile(local).toString();
    return QStringLiteral("qrc:/OmniPresence/resources/assets/") + key + QStringLiteral(".png");
}

QString AppController::presenceLargeImageSource() const { return sourceForKey(m_currentPresence.largeImageKey); }
QString AppController::presenceSmallImageSource() const { return sourceForKey(m_currentPresence.smallImageKey); }

QVariantList AppController::availableContextFields() const {
    const TemplateContext ctx = TemplateEngine::buildContext(m_currentWindow, m_integrationContext);
    static const QVector<QPair<QString, QString>> known = {
        {QStringLiteral("window.title"),      QStringLiteral("The window / tab title")},
        {QStringLiteral("browser.domain"),    QStringLiteral("The website domain")},
        {QStringLiteral("runelite.activity"), QStringLiteral("RuneScape activity")},
        {QStringLiteral("runelite.location"), QStringLiteral("RuneScape location")},
        {QStringLiteral("terminal.repo"),     QStringLiteral("Terminal repository")},
        {QStringLiteral("vscode.workspace"),  QStringLiteral("VS Code workspace")},
    };
    QVariantList out;
    for (const auto& k : known) {
        if (!ctx.value(k.first).isEmpty()) {
            out.append(QVariantMap{
                {QStringLiteral("token"), QStringLiteral("{{") + k.first + QStringLiteral("}}")},
                {QStringLiteral("label"), k.second},
            });
        }
    }
    return out;
}

// ── Rule CRUD bridge (Task 6) ──────────────────────────────────────────────────

QVariantList AppController::rulesList() const {
    QVariantList out;
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    for (int i = 0; i < rules.size(); ++i) {
        const Rule& r = rules[i];
        out.append(QVariantMap{
            {QStringLiteral("index"),    i},
            {QStringLiteral("name"),     r.name.isEmpty() ? QStringLiteral("(unnamed)") : r.name},
            {QStringLiteral("enabled"),  r.enabled},
            {QStringLiteral("priority"), r.priority},
        });
    }
    return out;
}

QVariantMap AppController::ruleAt(int index) const {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (index < 0 || index >= rules.size()) return {};
    return ruleToMap(index, rules[index]);
}

int AppController::addRule(const QVariantMap& draft) {
    Rule r;
    r.id      = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.name    = draft.value(QStringLiteral("name"), QStringLiteral("New Rule")).toString();
    r.enabled = draft.value(QStringLiteral("enabled"), true).toBool();
    r.priority= draft.value(QStringLiteral("priority"), 100).toInt();
    for (auto it = draft.constBegin(); it != draft.constEnd(); ++it)
        applyField(r, it.key(), it.value());
    m_configStore->ruleSet().addRule(r);
    emit rulesChanged();
    return m_configStore->ruleSet().rules().size() - 1;
}

void AppController::updateRuleField(int index, const QString& field, const QVariant& value) {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (index < 0 || index >= rules.size()) return;
    Rule r = rules[index];                 // copy
    applyField(r, field, value);
    m_configStore->ruleSet().updateRule(r);  // matches by id
}

void AppController::deleteRule(int index) {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (index < 0 || index >= rules.size()) return;
    m_configStore->ruleSet().removeRule(rules[index].id);
    emit rulesChanged();
    evaluateAndPublish();
}

void AppController::saveRules() {
    m_configStore->save();
    emit rulesChanged();
    evaluateAndPublish();
}

// ── Capture → draft rule (Task 8) ──────────────────────────────────────────────

int AppController::seedRuleFromCapture() {
    if (m_currentWindow.processName.isEmpty()) return -1;
    Rule r;
    r.id                   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.name                 = RuleEngine::genericPresence(m_currentWindow).name;
    r.enabled              = true;
    r.priority             = 50;
    r.matchProcessName     = m_currentWindow.processName;
    r.matchExecutablePath  = m_currentWindow.executablePath;
    r.activityType         = ActivityType::Playing;
    r.activityNameTemplate = r.name;       // main line pre-filled from the app name
    r.privacyLevel         = PrivacyLevel::Public;
    m_configStore->ruleSet().addRule(r);
    emit rulesChanged();
    return m_configStore->ruleSet().rules().size() - 1;
}

// ── Add photo (Task 9) ─────────────────────────────────────────────────────────

QString AppController::importPhoto(int ruleIndex, const QString& fileUrl) {
    const QString src = QUrl(fileUrl).toLocalFile();
    if (src.isEmpty()) return {};

    QString key = ArtStore::slugify(src);
    const QString base = key;
    int n = 1;
    while (!m_artStore.localPathForKey(key).isEmpty())
        key = base + QString::number(++n);

    QString out, err;
    if (!m_artStore.importImage(src, key, &out, &err)) {
        m_discordError = err;
        emit discordStatusChanged();
        return {};
    }

    updateRuleField(ruleIndex, QStringLiteral("largeImageKey"), key);
    m_configStore->setAssetKey(key, key);
    saveRules();

    // Open the portal Art Assets page + reveal the normalized file. No Playwright:
    // the user does the one drag-drop themselves.
    const QString appId = m_configStore->settings().discordAppId;
    if (!appId.isEmpty()) {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://discord.com/developers/applications/%1/rich-presence/assets").arg(appId)));
    }
    QProcess::startDetached(QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(out)});
    return key;
}

} // namespace OmniPresence
