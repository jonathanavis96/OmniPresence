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
#include "ArtStore.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>
#include <memory>

namespace OmniPresence {

class ActiveWindowWatcher;
class DiscordPresenceClient;
class LocalContextServer;
class NamedPipeInterceptor;
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
    Q_PROPERTY(QString  presenceLargeImageSource READ presenceLargeImageSource NOTIFY presenceChanged)
    Q_PROPERTY(QString  presenceSmallImageSource READ presenceSmallImageSource NOTIFY presenceChanged)
    Q_PROPERTY(QVariantList availableContextFields READ availableContextFields NOTIFY presenceChanged)

    Q_PROPERTY(bool     discordConnected    READ discordConnected    NOTIFY discordStatusChanged)
    Q_PROPERTY(QString  discordStatus       READ discordStatus       NOTIFY discordStatusChanged)
    Q_PROPERTY(QString  discordError        READ discordError        NOTIFY discordStatusChanged)
    Q_PROPERTY(QString  discordAppId        READ discordAppId        NOTIFY discordStatusChanged)
    Q_PROPERTY(QString  lastUpdateTime      READ lastUpdateTime      NOTIFY presenceChanged)
    Q_PROPERTY(bool     privacyMode         READ privacyMode         WRITE setPrivacyMode NOTIFY privacyModeChanged)
    Q_PROPERTY(bool     paused              READ paused              NOTIFY pauseChanged)

    Q_PROPERTY(bool     capturing           READ capturing           NOTIFY captureStateChanged)
    Q_PROPERTY(int      captureCountdown    READ captureCountdown     NOTIFY captureStateChanged)

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
    QString  presenceLargeImageSource() const;
    QString  presenceSmallImageSource() const;
    QVariantList availableContextFields() const;
    bool     discordConnected()   const;
    QString  discordStatus()      const;
    QString  discordError()       const { return m_discordError; }
    QString  discordAppId()       const;
    QString  lastUpdateTime()     const;
    bool     privacyMode()        const noexcept { return m_overrideState.privateMode; }
    bool     paused()             const noexcept { return m_overrideState.paused;      }
    bool     capturing()          const noexcept { return m_capturing;        }
    int      captureCountdown()   const noexcept { return m_captureCountdown; }

    // ── QML-invokable actions ─────────────────────────────────────────────────
    Q_INVOKABLE void captureCurrentWindow();
    /// Start a short countdown, then snapshot whichever window the user focused.
    Q_INVOKABLE void beginCapture();
    /// (Re)start the Discord OAuth handshake using the configured app ID.
    Q_INVOKABLE void connectDiscord();
    /// Drop the Discord connection (does not clear the saved token).
    Q_INVOKABLE void disconnectDiscord();
    Q_INVOKABLE void publishTest();
    Q_INVOKABLE void setPrivacyMode(bool enabled);
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void saveConfig();
    Q_INVOKABLE void reloadConfig();

    // ── Rule CRUD bridge (QML) ────────────────────────────────────────────────
    /// [{index,name,enabled,priority}] in config (insertion) order.
    Q_INVOKABLE QVariantList rulesList() const;
    /// All editable fields of one rule (empty map if index out of range).
    Q_INVOKABLE QVariantMap  ruleAt(int index) const;
    /// Append a rule from a draft map; returns the new index.
    Q_INVOKABLE int          addRule(const QVariantMap& draft);
    /// Update a single field of the rule at `index`.
    Q_INVOKABLE void         updateRuleField(int index, const QString& field, const QVariant& value);
    /// Delete the rule at `index`.
    Q_INVOKABLE void         deleteRule(int index);
    /// Persist the rule set to disk.
    Q_INVOKABLE void         saveRules();
    /// Build a draft rule from the captured window; returns its new index (-1 if none).
    Q_INVOKABLE int          seedRuleFromCapture();
    /// Import a local image as the rule's art, persist, open the portal for upload.
    /// Returns the assigned art key ("" on failure).
    Q_INVOKABLE QString      importPhoto(int ruleIndex, const QString& fileUrl);
    /// Generate a monogram tile for the rule (key = slug of its name), set it as
    /// the rule's art, persist, and open the portal for upload. "" on failure.
    Q_INVOKABLE QString      generateArt(int ruleIndex, const QString& monogram, const QString& accentHex);
    /// Art keys available to pick from (bundled defaults + user-added photos).
    Q_INVOKABLE QStringList  artKeys() const;
    /// Preview source ("file://"/"qrc:") for an arbitrary art key (for thumbnails).
    Q_INVOKABLE QString      artSourceForKey(const QString& key) const { return sourceForKey(key); }

    // Accessors for the tray menu (non-QML)
    ConfigStore*           configStore()    const noexcept { return m_configStore.get(); }
    DiscordPresenceClient* discordClient()  const noexcept { return m_discordClient.get(); }

signals:
    void windowChanged();
    void presenceChanged();
    void discordStatusChanged();
    void privacyModeChanged();
    void pauseChanged();
    void captureStateChanged();
    void rulesChanged();

private slots:
    void onActiveWindowChanged(const OmniPresence::WindowInfo& info);
    void onIntegrationContextUpdated(const QString& source);
    void onRuneliteActivityCaptured(const QString& activity, const QString& location);
    void onDiscordCallbackTimer();
    void onCaptureTick();

private:
    void evaluateAndPublish();

    /// Append one human-readable line to presence-events.log on each real
    /// presence change (what published + the signals behind it). This is the
    /// "clean window" used to spot misfires and tune the inferencer.
    void logPresenceEvent(const PresencePayload& payload);

    QString sourceForKey(const QString& key) const;
    /// Persist a freshly-stored art key onto a rule, then open portal + reveal file.
    QString finishArtImport(int ruleIndex, const QString& key, const QString& outPath);

    ArtStore m_artStore;

    std::unique_ptr<ActiveWindowWatcher>    m_watcher;
    std::unique_ptr<DiscordPresenceClient>  m_discordClient;
    std::unique_ptr<LocalContextServer>     m_contextServer;
    std::unique_ptr<NamedPipeInterceptor>   m_runeliteInterceptor;
    std::unique_ptr<ConfigStore>            m_configStore;

    IntegrationContext   m_integrationContext;
    RuleEngine           m_ruleEngine;
    ManualOverrideState  m_overrideState;

    WindowInfo      m_currentWindow;
    PresencePayload m_currentPresence;
    PresencePayload m_lastPublishedPresence;
    QDateTime       m_lastUpdateTime;
    QString         m_discordError;

    class QTimer*   m_discordCallbackTimer{nullptr};
    class QTimer*   m_captureTimer{nullptr};
    bool            m_capturing{false};
    int             m_captureCountdown{0};
};

} // namespace OmniPresence
