// ConfigStore.cpp — JSON config load/save with %APPDATA% resolution.
#include "ConfigStore.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>

namespace OmniPresence {

// ── AppSettings JSON helpers ──────────────────────────────────────────────────

static QJsonObject settingsToJson(const AppSettings& s) {
    QJsonObject obj;
    obj[QStringLiteral("discordAppId")]       = s.discordAppId;
    obj[QStringLiteral("privacyMode")]        = s.privacyMode;
    obj[QStringLiteral("startWithWindows")]   = s.startWithWindows;
    obj[QStringLiteral("contextServerPort")]  = static_cast<int>(s.contextServerPort);
    obj[QStringLiteral("pollIntervalMs")]     = s.pollIntervalMs;
    obj[QStringLiteral("stabilityWindowMs")]  = s.stabilityWindowMs;
    obj[QStringLiteral("showInTray")]         = s.showInTray;
    return obj;
}

static AppSettings settingsFromJson(const QJsonObject& obj) {
    AppSettings s;
    s.discordAppId       = obj[QStringLiteral("discordAppId")].toString();
    s.privacyMode        = obj[QStringLiteral("privacyMode")].toBool(false);
    s.startWithWindows   = obj[QStringLiteral("startWithWindows")].toBool(false);
    s.contextServerPort  = static_cast<quint16>(obj[QStringLiteral("contextServerPort")].toInt(47831));
    s.pollIntervalMs     = obj[QStringLiteral("pollIntervalMs")].toInt(750);
    s.stabilityWindowMs  = obj[QStringLiteral("stabilityWindowMs")].toInt(2500);
    s.showInTray         = obj[QStringLiteral("showInTray")].toBool(true);
    return s;
}

// ── ConfigStore ───────────────────────────────────────────────────────────────

ConfigStore::ConfigStore(QObject* parent)
    : QObject(parent)
    , m_configPath(resolveConfigPath())
{}

QString ConfigStore::resolveConfigPath() {
#ifdef _WIN32
    // %APPDATA%\OmniPresence\config.json
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(appData).filePath(QStringLiteral("config.json"));
#else
    // Dev fallback: config/omnipresence.json relative to working directory.
    return QStringLiteral("config/omnipresence.json");
#endif
}

QString ConfigStore::configFilePath() const {
    return m_configPath;
}

bool ConfigStore::load() {
    QFile file(m_configPath);
    if (!file.exists()) {
        qDebug() << "[ConfigStore] Config not found at" << m_configPath << "— using defaults.";
        emit configLoaded();
        return true;   // Fresh install — defaults are fine.
    }
    if (!file.open(QIODevice::ReadOnly)) {
        const QString msg = QStringLiteral("ConfigStore: cannot open %1: %2")
                                .arg(m_configPath, file.errorString());
        qWarning() << msg;
        emit configError(msg);
        return false;
    }
    const bool ok = parseJson(file.readAll());
    if (ok) emit configLoaded();
    return ok;
}

bool ConfigStore::save() const {
    // Ensure the directory exists.
    QDir dir = QFileInfo(m_configPath).absoluteDir();
    if (!dir.exists() && !dir.mkpath(dir.absolutePath())) {
        const QString msg = QStringLiteral("ConfigStore: cannot create directory %1")
                                .arg(dir.absolutePath());
        qWarning() << msg;
        const_cast<ConfigStore*>(this)->configError(msg);
        return false;
    }

    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString msg = QStringLiteral("ConfigStore: cannot write %1: %2")
                                .arg(m_configPath, file.errorString());
        qWarning() << msg;
        const_cast<ConfigStore*>(this)->configError(msg);
        return false;
    }
    file.write(serialiseJson());
    const_cast<ConfigStore*>(this)->configSaved();
    return true;
}

bool ConfigStore::parseJson(const QByteArray& data) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        const QString msg = QStringLiteral("ConfigStore: JSON parse error — %1").arg(err.errorString());
        qWarning() << msg;
        emit configError(msg);
        return false;
    }
    const QJsonObject root = doc.object();
    m_settings = settingsFromJson(root[QStringLiteral("settings")].toObject());
    m_ruleSet  = RuleSet::fromJson(root[QStringLiteral("ruleSet")].toObject());
    return true;
}

QByteArray ConfigStore::serialiseJson() const {
    QJsonObject root;
    root[QStringLiteral("settings")] = settingsToJson(m_settings);
    root[QStringLiteral("ruleSet")]  = m_ruleSet.toJson();
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace OmniPresence
