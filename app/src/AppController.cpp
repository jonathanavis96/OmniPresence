// AppController.cpp — Central controller implementation.
#include "AppController.h"
#include "ActiveWindowWatcher.h"
#include "DiscordPresenceClient.h"
#include "LocalContextServer.h"
#include "ConfigStore.h"
#include <QTimer>
#include <QDebug>

namespace OmniPresence {

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
            this, [this](DiscordConnectionStatus) { emit discordStatusChanged(); });

    // ── Discord SDK callback timer (100 ms) ───────────────────────────────────
    m_discordCallbackTimer = new QTimer(this);
    m_discordCallbackTimer->setInterval(100);
    connect(m_discordCallbackTimer, &QTimer::timeout,
            this,                   &AppController::onDiscordCallbackTimer);
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

} // namespace OmniPresence
