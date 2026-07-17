// ConfigStore.h — Load/save configuration (rules + settings) to JSON.
//
// Config path resolution:
//   Windows:     %APPDATA%/OmniPresence/config.json
//   Other/dev:   config/omnipresence.json  (relative to working directory)
#pragma once

#include "Rule.h"
#include "RuleEngine.h"   // IdleConfig
#include <QObject>
#include <QString>
#include <QMap>

namespace OmniPresence {

/// Application-wide settings (non-rule).
struct AppSettings {
    QString discordAppId;
    bool    privacyMode{false};
    bool    startWithWindows{false};
    quint16 contextServerPort{47831};
    int     pollIntervalMs{750};
    int     stabilityWindowMs{2500};
    bool    showInTray{true};
};

class ConfigStore : public QObject {
    Q_OBJECT
public:
    explicit ConfigStore(QObject* parent = nullptr);

    /// Load from disk (creates defaults if the file doesn't exist).
    bool load();
    /// Save current state to disk.
    bool save() const;

    [[nodiscard]] const RuleSet&    ruleSet()   const noexcept { return m_ruleSet;   }
    [[nodiscard]] const AppSettings& settings() const noexcept { return m_settings; }
    [[nodiscard]] RuleSet&    ruleSet()    noexcept { return m_ruleSet;  }
    [[nodiscard]] AppSettings& settings()  noexcept { return m_settings; }

    /// Idle-tier (AFK / Away-from-computer) config — parsed from the "idle"
    /// object in config JSON, defaulting any missing field (see ConfigStore.cpp).
    [[nodiscard]] const IdleConfig& idleConfig() const noexcept { return m_idleConfig; }
    [[nodiscard]] IdleConfig&       idleConfig()       noexcept { return m_idleConfig; }

    /// Art-asset metadata: image key -> hover text. The local PNG path is
    /// derived from ArtStore::localPathForKey(key), not stored here.
    [[nodiscard]] const QMap<QString, QString>& assetKeys() const noexcept { return m_assetKeys; }
    void setAssetKey(const QString& key, const QString& hoverText) { m_assetKeys.insert(key, hoverText); }

    /// Absolute path of the active config file.
    [[nodiscard]] QString configFilePath() const;

    /// Test seam: override the resolved config path before load()/save().
    void setConfigPathForTest(const QString& path) { m_configPath = path; }

signals:
    void configLoaded();
    void configSaved();
    void configError(const QString& message);

private:
    static QString resolveConfigPath();
    bool parseJson(const QByteArray& data);
    [[nodiscard]] QByteArray serialiseJson() const;
    /// Idempotent in-place upgrades of loaded rules (e.g. terminal title fallback).
    void migrateRuleTemplates();

    RuleSet     m_ruleSet;
    AppSettings m_settings;
    IdleConfig  m_idleConfig;
    QMap<QString, QString> m_assetKeys;
    QString     m_configPath;
};

} // namespace OmniPresence
