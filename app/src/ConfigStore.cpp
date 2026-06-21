// ConfigStore.cpp — JSON config load/save with %APPDATA% resolution.
#include "ConfigStore.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDebug>

namespace OmniPresence {

// ── AppSettings JSON helpers ──────────────────────────────────────────────────

// The on-disk schema is the documented/flat one (see config/omnipresence.example.json
// and docs/DISCORD_SETUP.md): settings live at the JSON root, the Discord app ID
// is "discordApplicationId", the port is "localContextPort", the stability window
// is "focusStableMs", and rules are a top-level "rules" array. Earlier scaffolding
// of this class invented a different nested schema; we read both for safety but
// write the documented one.

static QJsonObject settingsToJson(const AppSettings& s) {
    QJsonObject obj;
    obj[QStringLiteral("discordApplicationId")] = s.discordAppId;
    obj[QStringLiteral("privacyMode")]          = s.privacyMode;
    obj[QStringLiteral("startWithWindows")]     = s.startWithWindows;
    obj[QStringLiteral("localContextPort")]     = static_cast<int>(s.contextServerPort);
    obj[QStringLiteral("pollIntervalMs")]       = s.pollIntervalMs;
    obj[QStringLiteral("focusStableMs")]        = s.stabilityWindowMs;
    obj[QStringLiteral("showInTray")]           = s.showInTray;
    return obj;
}

// Read a string that may be stored as a JSON string or a (large) number.
static QString readId(const QJsonObject& obj, const QString& a, const QString& b) {
    QJsonValue v = obj.contains(a) ? obj.value(a) : obj.value(b);
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(static_cast<qulonglong>(v.toDouble()));
    return {};
}

static AppSettings settingsFromJson(const QJsonObject& obj) {
    AppSettings s;
    s.discordAppId       = readId(obj, QStringLiteral("discordApplicationId"),
                                       QStringLiteral("discordAppId"));
    s.privacyMode        = obj[QStringLiteral("privacyMode")].toBool(false);
    s.startWithWindows   = obj[QStringLiteral("startWithWindows")].toBool(false);
    s.contextServerPort  = static_cast<quint16>(
        obj.contains(QStringLiteral("localContextPort"))
            ? obj[QStringLiteral("localContextPort")].toInt(47831)
            : obj[QStringLiteral("contextServerPort")].toInt(47831));
    s.pollIntervalMs     = obj[QStringLiteral("pollIntervalMs")].toInt(750);
    s.stabilityWindowMs  =
        obj.contains(QStringLiteral("focusStableMs"))
            ? obj[QStringLiteral("focusStableMs")].toInt(2500)
            : obj[QStringLiteral("stabilityWindowMs")].toInt(2500);
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
    // Documented schema keeps settings at the root; the older form nested them
    // under "settings". Pick whichever is present.
    const QJsonObject settingsObj =
        root.contains(QStringLiteral("settings"))
            ? root[QStringLiteral("settings")].toObject()
            : root;
    m_settings = settingsFromJson(settingsObj);
    // Rules: documented schema is a top-level "rules" array (which RuleSet reads
    // off whatever object it's given); older form nested it under "ruleSet".
    m_ruleSet  = root.contains(QStringLiteral("ruleSet"))
            ? RuleSet::fromJson(root[QStringLiteral("ruleSet")].toObject())
            : RuleSet::fromJson(root);
    migrateRuleTemplates();
    return true;
}

void ConfigStore::migrateRuleTemplates() {
    // Ensure terminal rules fall back to the live window title when no
    // integration context is present (fixes "Working on " rendering blank
    // for a terminal whose title is e.g. "RAM"). Idempotent: only touches a
    // templated field that doesn't already reference window.title.
    bool changed = false;
    const QList<Rule> rules = m_ruleSet.rules();   // copy to iterate
    for (Rule r : rules) {
        if (r.matchIntegrationSource != QLatin1String("terminal"))
            continue;
        auto addFallback = [](QString& tmpl) -> bool {
            if (tmpl.contains(QLatin1String("window.title")) ||
                !tmpl.contains(QLatin1String("{{")))
                return false;
            // Insert " or window.title" before the closing braces of each {{...}}.
            tmpl.replace(QLatin1String("}}"), QLatin1String(" or window.title}}"));
            return true;
        };
        bool d = addFallback(r.detailsTemplate);
        bool s = addFallback(r.stateTemplate);
        if (d || s) { m_ruleSet.updateRule(r); changed = true; }
    }
    if (changed) qDebug() << "[ConfigStore] migrated terminal rule templates (window.title fallback)";
}

QByteArray ConfigStore::serialiseJson() const {
    // Write the documented flat schema: settings at the root + a top-level
    // "rules" array, so a saved file matches config/omnipresence.example.json
    // and reloads cleanly.
    QJsonObject root = settingsToJson(m_settings);
    root[QStringLiteral("rules")] = m_ruleSet.toJson().value(QStringLiteral("rules"));
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace OmniPresence
