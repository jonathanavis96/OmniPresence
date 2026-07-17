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

// ── IdleConfig JSON helpers (Phase 3) ───────────────────────────────────────────

// Shipped default — matches config/omnipresence.json / omnipresence.example.json
// and the assets/icons/away.png staged for the "Away from computer" card.
// OmniPresence uses full raw.githubusercontent URLs (not Discord portal asset
// keys) as largeImageKey, same as every other rule's icon.
static const QString kDefaultAwayImageKey = QStringLiteral(
    "https://raw.githubusercontent.com/jonathanavis96/OmniPresence/omnipresence-work/assets/icons/away.png");

static QJsonObject idleConfigToJson(const IdleConfig& cfg) {
    QJsonObject obj;
    obj[QStringLiteral("enabled")]      = cfg.enabled;
    obj[QStringLiteral("afkSeconds")]   = static_cast<double>(cfg.afkSeconds);
    obj[QStringLiteral("awaySeconds")]  = static_cast<double>(cfg.awaySeconds);
    obj[QStringLiteral("afkLabel")]     = cfg.afkLabel;
    obj[QStringLiteral("awayLabel")]    = cfg.awayLabel;
    obj[QStringLiteral("awayImageKey")] = cfg.awayImageKey;
    return obj;
}

// Any field missing from the JSON (including the whole "idle" object being
// absent — obj is then default-constructed/empty) falls back to the shipped
// defaults below, matching config/omnipresence.example.json's "idle" block.
static IdleConfig idleConfigFromJson(const QJsonObject& obj) {
    IdleConfig cfg;
    cfg.enabled      = obj[QStringLiteral("enabled")].toBool(true);
    cfg.afkSeconds   = static_cast<quint64>(obj[QStringLiteral("afkSeconds")].toDouble(120));
    cfg.awaySeconds  = static_cast<quint64>(obj[QStringLiteral("awaySeconds")].toDouble(600));
    cfg.afkLabel     = obj[QStringLiteral("afkLabel")].toString(QStringLiteral("AFK"));
    cfg.awayLabel    = obj[QStringLiteral("awayLabel")].toString(QStringLiteral("Away from computer"));
    cfg.awayImageKey = obj[QStringLiteral("awayImageKey")].toString(kDefaultAwayImageKey);
    return cfg;
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
        // No JSON to parse, but IdleConfig's own struct default has
        // enabled=false (so a bare-evaluate() caller that forgets to load
        // config never accidentally gets idle overrides) — that is NOT the
        // shipped default. Route through idleConfigFromJson(empty) so a
        // fresh install still gets enabled=true, 120 s / 600 s, etc.
        m_idleConfig = idleConfigFromJson(QJsonObject());
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
    // Idle-tier (AFK / Away-from-computer) config. Optional top-level object —
    // any/all missing fields default (see idleConfigFromJson).
    m_idleConfig = idleConfigFromJson(root.value(QStringLiteral("idle")).toObject());
    // Art-asset metadata (key -> hover text). Optional top-level object.
    m_assetKeys.clear();
    const QJsonObject assets = root.value(QStringLiteral("assetKeys")).toObject();
    for (auto it = assets.constBegin(); it != assets.constEnd(); ++it)
        m_assetKeys.insert(it.key(), it.value().toString());
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
        bool touched = false;
        // Add the window.title fallback to the DETAILS line only (so a terminal
        // titled "RAM" shows "Working on RAM"). Deliberately NOT the state line —
        // that would duplicate the title as a redundant third line.
        if (!r.detailsTemplate.contains(QLatin1String("window.title")) &&
            r.detailsTemplate.contains(QLatin1String("{{"))) {
            r.detailsTemplate.replace(QLatin1String("}}"), QLatin1String(" or window.title}}"));
            touched = true;
        }
        // Undo any earlier over-eager migration that added it to the state line.
        if (r.stateTemplate.contains(QLatin1String(" or window.title}}"))) {
            r.stateTemplate.replace(QLatin1String(" or window.title}}"), QLatin1String("}}"));
            touched = true;
        }
        if (touched) { m_ruleSet.updateRule(r); changed = true; }
    }
    if (changed) qDebug() << "[ConfigStore] migrated terminal rule templates (window.title fallback)";
}

QByteArray ConfigStore::serialiseJson() const {
    // Write the documented flat schema: settings at the root + a top-level
    // "rules" array, so a saved file matches config/omnipresence.example.json
    // and reloads cleanly.
    QJsonObject root = settingsToJson(m_settings);
    root[QStringLiteral("rules")] = m_ruleSet.toJson().value(QStringLiteral("rules"));
    root[QStringLiteral("idle")] = idleConfigToJson(m_idleConfig);
    QJsonObject assets;
    for (auto it = m_assetKeys.constBegin(); it != m_assetKeys.constEnd(); ++it)
        assets[it.key()] = it.value();
    root[QStringLiteral("assetKeys")] = assets;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

} // namespace OmniPresence
