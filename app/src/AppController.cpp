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

} // namespace OmniPresence
