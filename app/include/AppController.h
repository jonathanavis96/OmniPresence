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

// Qt classes live in the global namespace — forward-declare here (not inside
// namespace OmniPresence, which would create a distinct incomplete type).
class QNetworkAccessManager;

namespace OmniPresence {

class ActiveWindowWatcher;
class DiscordPresenceClient;
class LocalContextServer;
class NamedPipeInterceptor;
class InputIdleMonitor;
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

    // ── Idle / AFK settings (Phase 3) ─────────────────────────────────────────
    Q_PROPERTY(bool idleEnabled     READ idleEnabled     WRITE setIdleEnabled     NOTIFY idleConfigChanged)
    Q_PROPERTY(int  idleAfkMinutes  READ idleAfkMinutes  WRITE setIdleAfkMinutes  NOTIFY idleConfigChanged)
    Q_PROPERTY(int  idleAwayMinutes READ idleAwayMinutes WRITE setIdleAwayMinutes NOTIFY idleConfigChanged)

    // ── Custom override (the "Custom" tab) ────────────────────────────────────
    Q_PROPERTY(bool    customEnabled         READ customEnabled         WRITE setCustomEnabled         NOTIFY customChanged)
    Q_PROPERTY(QString customMode            READ customMode            WRITE setCustomMode            NOTIFY customChanged)
    Q_PROPERTY(int     customActiveIndex     READ customActiveIndex     WRITE setCustomActiveIndex     NOTIFY customChanged)
    Q_PROPERTY(int     customIntervalSeconds READ customIntervalSeconds WRITE setCustomIntervalSeconds NOTIFY customChanged)

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

    // ── Idle / AFK settings getters (minutes in the UI, seconds in storage) ───
    bool     idleEnabled()        const noexcept;
    int      idleAfkMinutes()     const noexcept;
    int      idleAwayMinutes()    const noexcept;

    // ── Custom-override getters ───────────────────────────────────────────────
    bool     customEnabled()         const noexcept;
    QString  customMode()            const;   ///< "single" | "cycle"
    int      customActiveIndex()     const noexcept;
    int      customIntervalSeconds() const noexcept;

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

    // ── Idle / AFK settings (Phase 3) ─────────────────────────────────────────
    /// Master enable toggle for the idle-tier override. Persists immediately
    /// and applies live — the ~5 s idle tick re-reads the config-backed value.
    Q_INVOKABLE void setIdleEnabled(bool enabled);
    /// AFK threshold in MINUTES (stored as seconds). RuneLite-focused only.
    Q_INVOKABLE void setIdleAfkMinutes(int minutes);
    /// Away-from-computer threshold in MINUTES (stored as seconds). Any app.
    Q_INVOKABLE void setIdleAwayMinutes(int minutes);

    // ── Custom-override settings + preset CRUD (Custom tab) ────────────────────
    Q_INVOKABLE void setCustomEnabled(bool enabled);
    Q_INVOKABLE void setCustomMode(const QString& mode);              ///< "single" | "cycle"
    Q_INVOKABLE void setCustomActiveIndex(int index);                ///< Single-mode selection.
    Q_INVOKABLE void setCustomIntervalSeconds(int seconds);          ///< Cycle step (clamped >=1).
    /// [{index,label,name,includeInCycle}] in list (= cycle) order.
    Q_INVOKABLE QVariantList customPresetsList() const;
    /// All editable fields of one preset (empty map if index out of range).
    Q_INVOKABLE QVariantMap  customPresetAt(int index) const;
    /// Append a preset from a draft map; returns the new index.
    Q_INVOKABLE int          addCustomPreset(const QVariantMap& draft);
    /// Update one field of the preset at `index`.
    Q_INVOKABLE void         updateCustomPresetField(int index, const QString& field, const QVariant& value);
    /// Delete the preset at `index`.
    Q_INVOKABLE void         deleteCustomPreset(int index);
    /// Move the preset at `from` to `to`, redefining both the single-mode picker
    /// order and the cycle sequence.
    Q_INVOKABLE void         reorderCustomPreset(int from, int to);
    /// Reusable image library ([{label,url}]) offered as an icon dropdown.
    Q_INVOKABLE QVariantList customImageLibrary() const;
    /// Upload a local image to catbox.moe (anonymous, no key); on success set it
    /// as preset `presetIndex`'s largeImageKey and append it to the image
    /// library. Async — emits customUploadFinished(ok, message) when done. The
    /// URL text field and library dropdown remain available as fallbacks.
    Q_INVOKABLE void uploadPresetImage(int presetIndex, const QString& localPath);

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
    /// Preview source (URL/"file://"/"qrc:") for an art key or icon URL (thumbnails).
    Q_INVOKABLE QString      artSourceForKey(const QString& key) const { return sourceForKey(key); }
    /// Render a {{template}} against the current live context — for the rule
    /// editor's live "what Discord will show" preview.
    Q_INVOKABLE QString      previewTemplate(const QString& tmpl) const;
    /// Recent presence-events.log (this session's timeline of what published).
    Q_INVOKABLE QString      presenceEventsLog() const;
    /// app-coverage.log — every focused app + whether it resolved to an icon.
    Q_INVOKABLE QString      appCoverageLog() const;

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
    void idleConfigChanged();
    void customChanged();
    /// Emitted when an uploadPresetImage() call finishes. `ok` false => message
    /// carries the error for the UI; the preset's URL field stays editable.
    void customUploadFinished(bool ok, const QString& message);

private slots:
    void onActiveWindowChanged(const OmniPresence::WindowInfo& info);
    void onIntegrationContextUpdated(const QString& source);
    void onRuneliteActivityCaptured(const QString& activity, const QString& location);
    void onDiscordCallbackTimer();
    void onCaptureTick();
    /// Keep the RuneLite integration feed fresh while RuneLite is the focused
    /// window — its Discord-IPC source only sends on change, so a steady session
    /// would otherwise let the payload go stale and publish a blank name.
    void onRuneliteKeepAliveTick();
    /// ~5 s tick driving time-based idle-tier (AFK/Away) transitions — without
    /// this, idle crossing a threshold would never re-publish since nothing
    /// else triggers evaluateAndPublish() while the window/integration state
    /// is otherwise unchanged. The isSamePresence change-gate in
    /// evaluateAndPublish() prevents this from spamming Discord.
    void onIdleTick();
    /// Cycle-mode tick: advance to the next included preset frame and re-publish.
    void onCustomFrameTick();

private:
    void evaluateAndPublish();

    /// Recompute m_overrideState.customOverride from the current custom config +
    /// cycle frame index (nullopt when the override is off or resolves to
    /// nothing). Called at the top of evaluateAndPublish() so the priority-0
    /// override is always current regardless of what triggered the publish.
    void refreshCustomOverride();

    /// Start/stop/re-interval the cycle timer to match the current custom config
    /// (runs only while enabled + Cycle mode + more than one included preset).
    void reconfigureCustomTimer();

    /// Shared tail for every custom-config mutation: persist, notify QML, retune
    /// the cycle timer, and re-publish so the change is seen immediately.
    void commitCustomChange();

    /// Append one human-readable line to presence-events.log on each real
    /// presence change (what published + the signals behind it). This is the
    /// "clean window" used to spot misfires and tune the inferencer.
    void logPresenceEvent(const PresencePayload& payload);

    /// Record each distinct foreground app to app-coverage.log (append-only,
    /// deduped across sessions) noting whether it resolved to an icon/rule.
    /// Builds the backlog of apps that still need a custom icon.
    void logAppCoverage(const PresencePayload& payload);

    QString sourceForKey(const QString& key) const;
    /// Persist a freshly-stored art key onto a rule, then open portal + reveal file.
    QString finishArtImport(int ruleIndex, const QString& key, const QString& outPath);

    ArtStore m_artStore;

    std::unique_ptr<ActiveWindowWatcher>    m_watcher;
    std::unique_ptr<DiscordPresenceClient>  m_discordClient;
    std::unique_ptr<LocalContextServer>     m_contextServer;
    std::unique_ptr<NamedPipeInterceptor>   m_runeliteInterceptor;
    std::unique_ptr<InputIdleMonitor>       m_inputIdle;
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
    class QTimer*   m_runeliteKeepAliveTimer{nullptr};
    class QTimer*   m_idleTickTimer{nullptr};
    class QTimer*   m_customFrameTimer{nullptr};
    int             m_customFrameIndex{0};   ///< Position within cycleIndices().
    QNetworkAccessManager* m_netManager{nullptr};   ///< catbox uploads (global ::type).
    bool            m_capturing{false};
    int             m_captureCountdown{0};
};

} // namespace OmniPresence
