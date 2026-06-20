// AppController.h — Central controller exposed to QML as a singleton context property.
//
// Wires:
//   ActiveWindowWatcher → FocusDebouncer → RuleEngine → DiscordPresenceClient
//   LocalContextServer  → IntegrationContext → RuleEngine
//   ConfigStore (load/save)
//
// Exposed to QML via Q_PROPERTY and Q_INVOKABLE.
#pragma once

#include "WindowInfo.h"
#include "PresencePayload.h"
#include "IntegrationContext.h"
#include "RuleEngine.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <memory>

namespace OmniPresence {

class ActiveWindowWatcher;
class DiscordPresenceClient;
class LocalContextServer;
class ConfigStore;

class AppController : public QObject {
    Q_OBJECT

    // ── QML-visible properties ────────────────────────────────────────────────
    Q_PROPERTY(QString  currentProcessName  READ currentProcessName  NOTIFY windowChanged)
    Q_PROPERTY(QString  currentWindowTitle  READ currentWindowTitle  NOTIFY windowChanged)
    Q_PROPERTY(QString  currentExePath      READ currentExePath      NOTIFY windowChanged)
    Q_PROPERTY(QString  currentWindowClass  READ currentWindowClass  NOTIFY windowChanged)

    Q_PROPERTY(QString  matchedRuleName     READ matchedRuleName     NOTIFY presenceChanged)
    Q_PROPERTY(QString  presenceName        READ presenceName        NOTIFY presenceChanged)
    Q_PROPERTY(QString  presenceDetails     READ presenceDetails     NOTIFY presenceChanged)
    Q_PROPERTY(QString  presenceState       READ presenceState       NOTIFY presenceChanged)
    Q_PROPERTY(bool     isPrivateFallback   READ isPrivateFallback   NOTIFY presenceChanged)

    Q_PROPERTY(bool     discordConnected    READ discordConnected    NOTIFY discordStatusChanged)
    Q_PROPERTY(QString  lastUpdateTime      READ lastUpdateTime      NOTIFY presenceChanged)
    Q_PROPERTY(bool     privacyMode         READ privacyMode         WRITE setPrivacyMode NOTIFY privacyModeChanged)
    Q_PROPERTY(bool     paused              READ paused              NOTIFY pauseChanged)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    void initialise();   ///< Call after QML engine is set up.

    // ── Property getters ──────────────────────────────────────────────────────
    QString  currentProcessName() const;
    QString  currentWindowTitle() const;
    QString  currentExePath()     const;
    QString  currentWindowClass() const;
    QString  matchedRuleName()    const;
    QString  presenceName()       const;
    QString  presenceDetails()    const;
    QString  presenceState()      const;
    bool     isPrivateFallback()  const;
    bool     discordConnected()   const;
    QString  lastUpdateTime()     const;
    bool     privacyMode()        const noexcept { return m_overrideState.privateMode; }
    bool     paused()             const noexcept { return m_overrideState.paused;      }

    // ── QML-invokable actions ─────────────────────────────────────────────────
    Q_INVOKABLE void captureCurrentWindow();
    Q_INVOKABLE void publishTest();
    Q_INVOKABLE void setPrivacyMode(bool enabled);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void reloadConfig();

    // Accessors for the tray menu (non-QML)
    ConfigStore*           configStore()    const noexcept { return m_configStore.get(); }
    DiscordPresenceClient* discordClient()  const noexcept { return m_discordClient.get(); }

signals:
    void windowChanged();
    void presenceChanged();
    void discordStatusChanged();
    void privacyModeChanged();
    void pauseChanged();

private slots:
    void onActiveWindowChanged(const OmniPresence::WindowInfo& info);
    void onIntegrationContextUpdated(const QString& source);
    void onDiscordCallbackTimer();

private:
    void evaluateAndPublish();

    std::unique_ptr<ActiveWindowWatcher>    m_watcher;
    std::unique_ptr<DiscordPresenceClient>  m_discordClient;
    std::unique_ptr<LocalContextServer>     m_contextServer;
    std::unique_ptr<ConfigStore>            m_configStore;

    IntegrationContext   m_integrationContext;
    RuleEngine           m_ruleEngine;
    ManualOverrideState  m_overrideState;

    WindowInfo      m_currentWindow;
    PresencePayload m_currentPresence;
    PresencePayload m_lastPublishedPresence;
    QDateTime       m_lastUpdateTime;

    class QTimer*   m_discordCallbackTimer{nullptr};
};

} // namespace OmniPresence
