// ConfigStore.h — Load/save configuration (rules + settings) to JSON.
//
// Config path resolution:
//   Windows:     %APPDATA%/OmniPresence/config.json
//   Other/dev:   config/omnipresence.json  (relative to working directory)
#pragma once

#include "Rule.h"
#include <QObject>
#include <QString>

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

    /// Absolute path of the active config file.
    [[nodiscard]] QString configFilePath() const;

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
    QString     m_configPath;
};

} // namespace OmniPresence
