// AppController.cpp — Central controller implementation.
#include "AppController.h"
#include "ActiveWindowWatcher.h"
#include "DiscordPresenceClient.h"
#include "LocalContextServer.h"
#include "NamedPipeInterceptor.h"
#include "InputIdleMonitor.h"
#include "ConfigStore.h"
#include "TemplateEngine.h"
#include <QTimer>
#include <QDebug>
#include <QUrl>
#include <QUuid>
#include <QDir>
#include <QProcess>
#include <QDesktopServices>
#include <QColor>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QHash>

namespace OmniPresence {

// ── Rule <-> QVariantMap mapping helpers ────────────────────────────────────
namespace {

QVariantMap ruleToMap(int index, const Rule& r) {
    return QVariantMap{
        {QStringLiteral("index"),                index},
        {QStringLiteral("id"),                   r.id},
        {QStringLiteral("name"),                 r.name},
        {QStringLiteral("enabled"),              r.enabled},
        {QStringLiteral("priority"),             r.priority},
        {QStringLiteral("matchProcessName"),     r.matchProcessName},
        {QStringLiteral("matchExecutablePath"),  r.matchExecutablePath},
        {QStringLiteral("matchWindowTitle"),     r.matchWindowTitle},
        {QStringLiteral("matchWindowTitleRegex"),r.matchWindowTitleRegex},
        {QStringLiteral("matchBrowserDomain"),   r.matchBrowserDomain},
        {QStringLiteral("matchBrowserCategory"), r.matchBrowserCategory},
        {QStringLiteral("matchIntegrationSource"),r.matchIntegrationSource},
        {QStringLiteral("activityNameTemplate"), r.activityNameTemplate},
        {QStringLiteral("detailsTemplate"),      r.detailsTemplate},
        {QStringLiteral("stateTemplate"),        r.stateTemplate},
        {QStringLiteral("largeImageKey"),        r.largeImageKey},
        {QStringLiteral("largeImageText"),       r.largeImageText},
        {QStringLiteral("smallImageKey"),        r.smallImageKey},
        {QStringLiteral("smallImageText"),       r.smallImageText},
        {QStringLiteral("timestampMode"),        int(r.timestampMode)},
        {QStringLiteral("privacyLevel"),         int(r.privacyLevel)},
    };
}

void applyField(Rule& r, const QString& f, const QVariant& v) {
    if      (f == QLatin1String("name"))                 r.name = v.toString();
    else if (f == QLatin1String("enabled"))              r.enabled = v.toBool();
    else if (f == QLatin1String("priority"))             r.priority = v.toInt();
    else if (f == QLatin1String("matchProcessName"))     r.matchProcessName = v.toString();
    else if (f == QLatin1String("matchExecutablePath"))  r.matchExecutablePath = v.toString();
    else if (f == QLatin1String("matchWindowTitle"))     r.matchWindowTitle = v.toString();
    else if (f == QLatin1String("matchWindowTitleRegex"))r.matchWindowTitleRegex = v.toBool();
    else if (f == QLatin1String("matchBrowserDomain"))   r.matchBrowserDomain = v.toString();
    else if (f == QLatin1String("matchBrowserCategory")) r.matchBrowserCategory = v.toString();
    else if (f == QLatin1String("matchIntegrationSource"))r.matchIntegrationSource = v.toString();
    else if (f == QLatin1String("activityNameTemplate")) r.activityNameTemplate = v.toString();
    else if (f == QLatin1String("detailsTemplate"))      r.detailsTemplate = v.toString();
    else if (f == QLatin1String("stateTemplate"))        r.stateTemplate = v.toString();
    else if (f == QLatin1String("largeImageKey"))        r.largeImageKey = v.toString();
    else if (f == QLatin1String("largeImageText"))       r.largeImageText = v.toString();
    else if (f == QLatin1String("smallImageKey"))        r.smallImageKey = v.toString();
    else if (f == QLatin1String("smallImageText"))       r.smallImageText = v.toString();
    else if (f == QLatin1String("timestampMode"))        r.timestampMode = TimestampMode(v.toInt());
    else if (f == QLatin1String("privacyLevel"))         r.privacyLevel = PrivacyLevel(v.toInt());
}

// A browser rule only wins the integration pass when its source is "browser"
// (the engine requires it to discriminate among same-source domain rules). The
// simplified editor lets users type just a domain, so infer the source for them
// — otherwise the rule is skipped and the Chrome generic fallback takes over.
void normalizeRule(Rule& r) {
    if (r.matchIntegrationSource.isEmpty() &&
        (!r.matchBrowserDomain.isEmpty() || !r.matchBrowserCategory.isEmpty())) {
        r.matchIntegrationSource = QStringLiteral("browser");
    }
}

} // namespace

AppController::AppController(QObject* parent)
    : QObject(parent)
    , m_configStore(std::make_unique<ConfigStore>(this))
    , m_discordClient(std::make_unique<DiscordPresenceClient>(this))
    , m_contextServer(std::make_unique<LocalContextServer>(&m_integrationContext,
                                                           LocalContextServer::DEFAULT_PORT,
                                                           this))
    , m_runeliteInterceptor(std::make_unique<NamedPipeInterceptor>(this))
    , m_inputIdle(std::make_unique<InputIdleMonitor>(this))
{
    m_watcher = createActiveWindowWatcher(this);

    // ── Wire signals ──────────────────────────────────────────────────────────
    connect(m_watcher.get(), &ActiveWindowWatcher::activeWindowChanged,
            this,            &AppController::onActiveWindowChanged);

    connect(m_contextServer.get(), &LocalContextServer::contextUpdated,
            this,                  &AppController::onIntegrationContextUpdated);

    connect(m_runeliteInterceptor.get(), &NamedPipeInterceptor::activityCaptured,
            this,                        &AppController::onRuneliteActivityCaptured);

    connect(m_discordClient.get(), &DiscordPresenceClient::connectionStatusChanged,
            this, [this](DiscordConnectionStatus status) {
                // Clear the stale error once a fresh attempt gets underway or succeeds.
                if (status == DiscordConnectionStatus::Connecting ||
                    status == DiscordConnectionStatus::Connected) {
                    m_discordError.clear();
                }
                // Only claim discord-ipc-0 for RuneLite capture once Discord itself
                // is fully connected.  Starting the interceptor earlier would let it
                // intercept our own SDK's OAuth authorize on ipc-0 (empty code →
                // token exchange 400).  start() is idempotent.
                if (status == DiscordConnectionStatus::Connected) {
                    m_runeliteInterceptor->start();
                }
                emit discordStatusChanged();
            });

    // Before every fresh OAuth authorize (initial or a mid-session re-auth after a
    // rejected token), release discord-ipc-0 so the SDK's authorize prompt reaches
    // the real Discord client instead of our own impersonator.  Synchronous stop()
    // on this thread — the pipe is free before Authorize() goes out.
    connect(m_discordClient.get(), &DiscordPresenceClient::authorizationStarting,
            this, [this]() {
                m_runeliteInterceptor->stop();
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

    // ── RuneLite keep-alive (30 s) ────────────────────────────────────────────
    // Well under the 120 s freshness window, so the last RuneLite reading never
    // decays to stale while you're still in-game (see onRuneliteKeepAliveTick).
    m_runeliteKeepAliveTimer = new QTimer(this);
    m_runeliteKeepAliveTimer->setInterval(30000);
    connect(m_runeliteKeepAliveTimer, &QTimer::timeout,
            this, &AppController::onRuneliteKeepAliveTick);

    // ── Idle-tier tick (5 s) ───────────────────────────────────────────────────
    // Drives time-based AFK/Away transitions without waiting for a window or
    // integration event (see onIdleTick / RuleEngine's idle-tier override).
    m_idleTickTimer = new QTimer(this);
    m_idleTickTimer->setInterval(5000);
    connect(m_idleTickTimer, &QTimer::timeout,
            this, &AppController::onIdleTick);
}

AppController::~AppController() = default;

void AppController::initialise() {
    m_configStore->load();

    const AppSettings& settings = m_configStore->settings();
    if (!settings.discordAppId.isEmpty()) {
        m_discordClient->connectToDiscord(settings.discordAppId);
    }

    m_contextServer->start();
    // NOTE: the RuneLite interceptor is NOT started here.  It is started only once
    // Discord reaches Connected (see the connectionStatusChanged handler) and
    // stopped for the duration of any fresh OAuth authorize (authorizationStarting),
    // so it can never intercept our own SDK's authorize on discord-ipc-0.
    m_watcher->start();
    m_discordCallbackTimer->start();
    m_runeliteKeepAliveTimer->start();
    m_idleTickTimer->start();
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

bool AppController::idleEnabled() const noexcept {
    return m_configStore->idleConfig().enabled;
}

int AppController::idleAfkMinutes() const noexcept {
    return static_cast<int>(m_configStore->idleConfig().afkSeconds / 60);
}

int AppController::idleAwayMinutes() const noexcept {
    return static_cast<int>(m_configStore->idleConfig().awaySeconds / 60);
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

void AppController::onRuneliteActivityCaptured(const QString& activity,
                                               const QString& location)
{
    // Called via queued connection from the NamedPipeInterceptor worker thread;
    // safe to touch IntegrationContext here (main event loop).
    //
    // The built-in plugin only sends SET_ACTIVITY on change, so a refocus or a
    // momentary capture gap can arrive here with an empty activity/location
    // even though the previous non-empty reading is still the best estimate
    // of what's happening in-game. Retain the last non-empty value per field
    // instead of overwriting it with "" (which would otherwise wipe the skill
    // down to a bare "OSRS"). A genuinely new, non-empty capture always wins,
    // so this self-corrects the moment the plugin sends fresh data.
    const IntegrationPayload* prev = m_integrationContext.get(QStringLiteral("runelite"));
    const QString retainedActivity = prev ? prev->field(QStringLiteral("activity")) : QString();
    const QString retainedLocation = prev ? prev->field(QStringLiteral("location")) : QString();

    QJsonObject o;
    o[QStringLiteral("activity")] = activity.isEmpty() ? retainedActivity : activity;
    o[QStringLiteral("location")] = location.isEmpty() ? retainedLocation : location;
    m_integrationContext.update(QStringLiteral("runelite"), o);
    evaluateAndPublish();
}

void AppController::onDiscordCallbackTimer() {
    m_discordClient->runCallbacks();
}

void AppController::onRuneliteKeepAliveTick() {
    // RuneLite's Discord-IPC feed only sends SET_ACTIVITY on change, so a steady
    // session (e.g. training one skill in one place) would otherwise let the
    // runelite payload cross the 120 s freshness window and publish a blank name
    // → Discord shows the bare "OmniPresence". While RuneLite is the FOCUSED
    // window the last reading is still the best estimate of what you're doing, so
    // re-stamp it to "now". Detection mirrors RuleEngine::matchRule (the dev
    // client runs as java.exe, recognised by window title).
    //
    // refresh() below only touches receivedAt, never `data` — so it re-stamps
    // whatever activity/location is currently stored, including any retained
    // (last-non-empty) fields from onRuneliteActivityCaptured() above. Those
    // retained fields are preserved for free; nothing further to do here.
    const QString pn = m_currentWindow.processName.toLower();
    const QString wt = m_currentWindow.windowTitle.toLower();
    const bool runeliteFocused =
        pn.contains(QLatin1String("runelite")) ||
        ((pn.contains(QLatin1String("java")) || pn.contains(QLatin1String("javaw"))) &&
         (wt.contains(QLatin1String("runelite")) || wt.contains(QLatin1String("old school"))));
    if (!runeliteFocused) return;

    if (m_integrationContext.refresh(QStringLiteral("runelite"))) {
        evaluateAndPublish();
    }
}

void AppController::onIdleTick() {
    // Nothing else needs to change here — evaluateAndPublish() always reads
    // the live idle seconds + config-backed IdleConfig, and its isSamePresence
    // gate prevents this from spamming Discord while nothing has crossed a
    // threshold.
    evaluateAndPublish();
}

// ── Core evaluation ───────────────────────────────────────────────────────────

void AppController::evaluateAndPublish() {
    const PresencePayload candidate = m_ruleEngine.evaluate(
        m_currentWindow,
        m_integrationContext,
        m_configStore->ruleSet(),
        m_overrideState,
        m_lastPublishedPresence,
        m_inputIdle->idleSeconds(),
        m_currentWindow.processName,
        m_configStore->idleConfig());

    m_currentPresence = candidate;
    emit presenceChanged();

    // Icon-backlog discovery: record every distinct foreground app (even when
    // the presence itself did not change) so we can see which apps still lack
    // a custom icon. Cheap — dedups internally.
    logAppCoverage(candidate);

    // Skip the Discord API call if nothing changed.
    if (candidate.isSamePresence(m_lastPublishedPresence)) return;

    m_lastPublishedPresence = candidate;
    m_lastUpdateTime        = QDateTime::currentDateTimeUtc();
    logPresenceEvent(candidate);
    m_discordClient->updatePresence(candidate);
}

void AppController::logPresenceEvent(const PresencePayload& p) {
    // Clean, human-readable timeline of what actually published and why — the
    // "window" used to spot misfires (e.g. "Training Prayer" mid-Slayer) and
    // hand corrections back for tuning. Lives next to the debug log; truncated
    // once per launch so it stays a readable session window, not a growing dump.
    static QMutex mutex;
    static const QString path = [] {
        QString base = qEnvironmentVariable("LOCALAPPDATA");
        if (base.isEmpty()) base = QDir::tempPath();
        const QString dir = base + QStringLiteral("/OmniPresence");
        QDir().mkpath(dir);
        const QString f = dir + QStringLiteral("/presence-events.log");
        QFile(f).open(QIODevice::WriteOnly | QIODevice::Truncate); // truncate on launch
        return f;
    }();

    // The "why": when this presence was actually driven by the RuneLite plugin,
    // show its raw signal trail (so a wrong reading shows exactly which signals
    // caused it). Detect that by the presence reflecting the runelite activity /
    // location — NOT mere freshness, since the plugin keeps POSTing in the
    // background while you're on another window. Otherwise name the matched rule.
    QString why;
    if (const IntegrationPayload* rl = m_integrationContext.getFresh(QStringLiteral("runelite"))) {
        const QString activity = rl->field(QStringLiteral("activity"));
        const QString location = rl->field(QStringLiteral("location"));
        const bool runeliteDriven =
            (!activity.isEmpty() && (p.name == activity || p.details == activity)) ||
            (!location.isEmpty() && p.state == location);
        if (runeliteDriven) {
            why = rl->field(QStringLiteral("signals"));
            // Surface the mappable RuneScape values behind this reading, so the log
            // shows exactly what each rule token ({{runelite.activity}} etc.) would
            // render right now — i.e. the options you can build a rule from.
            const QString target = rl->field(QStringLiteral("target"));
            const QString skill  = rl->field(QStringLiteral("skill"));
            QStringList vals;
            if (!activity.isEmpty()) vals << QStringLiteral("activity=\"%1\"").arg(activity);
            if (!target.isEmpty())   vals << QStringLiteral("target=\"%1\"").arg(target);
            if (!skill.isEmpty())    vals << QStringLiteral("skill=\"%1\"").arg(skill);
            if (!location.isEmpty()) vals << QStringLiteral("location=\"%1\"").arg(location);
            if (!vals.isEmpty())
                why += QStringLiteral("  |  ") + vals.join(QStringLiteral("  "));
        }
    }
    if (why.isEmpty()) {
        why = p.matchedRuleName.isEmpty()
            ? QStringLiteral("(generic)")
            : QStringLiteral("rule:%1").arg(p.matchedRuleName);
    }

    QString main = QStringLiteral("\"%1\"").arg(p.name);
    if (!p.state.isEmpty())   main += QStringLiteral(" | %1").arg(p.state);
    if (!p.details.isEmpty() && p.details != p.state)
        main += QStringLiteral(" (%1)").arg(p.details);
    if (p.isPrivateFallback)  main += QStringLiteral(" [private]");

    QMutexLocker lock(&mutex);
    QFile fh(path);
    if (fh.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&fh);
        out << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))
            << "  " << main << "   <- " << why << '\n';
    }
}

void AppController::logAppCoverage(const PresencePayload& p) {
    // One line per distinct foreground app. Unlike presence-events.log this is
    // NOT truncated per launch — it accumulates every app the user has focused,
    // flagged by whether it resolved to an icon. Each app's line is UPGRADED in
    // place when its status changes: if you later add an icon rule for an app,
    // the next time it's focused its "NO ICON / no rule" line flips to
    // "ICON ✓ / rule: …" rather than lying forever (the old behaviour recorded
    // each app once and never refreshed, so e.g. Discord/java kept showing
    // "NO ICON" long after a rule was added).
    const QString app = m_currentWindow.processName;
    if (app.isEmpty()) return;

    struct CovEntry { QString time; QString status; QString note; };

    static QMutex mutex;
    static const QString path = [] {
        QString base = qEnvironmentVariable("LOCALAPPDATA");
        if (base.isEmpty()) base = QDir::tempPath();
        const QString dir = base + QStringLiteral("/OmniPresence");
        QDir().mkpath(dir);
        return dir + QStringLiteral("/app-coverage.log");
    }();
    // Insertion-ordered store primed from the existing file (preserves the
    // original first-seen order on rewrite).
    static QStringList order;
    static QHash<QString, CovEntry> entries = [] {
        QHash<QString, CovEntry> m;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                const QStringList cols = in.readLine().split(QLatin1Char('\t'));
                if (cols.size() < 2) continue;
                const QString a = cols.at(1).trimmed();
                if (!m.contains(a)) order.append(a);
                m.insert(a, CovEntry{
                    cols.value(0).trimmed(),
                    cols.value(2).trimmed(),
                    cols.value(3).trimmed() });
            }
        }
        return m;
    }();

    const bool hasIcon = !p.largeImageKey.isEmpty();
    const QString status = hasIcon ? QStringLiteral("ICON ✓") : QStringLiteral("NO ICON");
    const QString note   = p.matchedRuleName.isEmpty()
        ? QStringLiteral("(generic — no rule)")
        : QStringLiteral("rule: %1").arg(p.matchedRuleName);

    QMutexLocker lock(&mutex);
    const auto it = entries.constFind(app);
    if (it != entries.constEnd() && it->status == status && it->note == note)
        return;  // unchanged — nothing to write

    if (it == entries.constEnd()) order.append(app);
    entries.insert(app, CovEntry{
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")),
        status, note });

    // Rewrite the whole file (bounded by the number of distinct apps focused).
    QFile fh(path);
    if (fh.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&fh);
        for (const QString& a : order) {
            const CovEntry& e = entries[a];
            out << e.time << '\t' << a << '\t' << e.status << '\t' << e.note << '\n';
        }
    }
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

// ── Idle / AFK settings (Phase 3) ───────────────────────────────────────────

void AppController::setIdleEnabled(bool enabled) {
    IdleConfig cfg = m_configStore->idleConfig();
    if (cfg.enabled == enabled) return;
    cfg.enabled = enabled;
    m_configStore->idleConfig() = cfg;
    m_configStore->save();
    emit idleConfigChanged();
    evaluateAndPublish();
}

void AppController::setIdleAfkMinutes(int minutes) {
    const quint64 seconds = static_cast<quint64>(minutes > 0 ? minutes : 0) * 60;
    IdleConfig cfg = m_configStore->idleConfig();
    if (cfg.afkSeconds == seconds) return;
    cfg.afkSeconds = seconds;
    m_configStore->idleConfig() = cfg;
    m_configStore->save();
    emit idleConfigChanged();
    evaluateAndPublish();
}

void AppController::setIdleAwayMinutes(int minutes) {
    const quint64 seconds = static_cast<quint64>(minutes > 0 ? minutes : 0) * 60;
    IdleConfig cfg = m_configStore->idleConfig();
    if (cfg.awaySeconds == seconds) return;
    cfg.awaySeconds = seconds;
    m_configStore->idleConfig() = cfg;
    m_configStore->save();
    emit idleConfigChanged();
    evaluateAndPublish();
}

// ── Art sources + available context (Task 5) ───────────────────────────────────

QString AppController::sourceForKey(const QString& key) const {
    if (key.isEmpty()) return {};
    // URL icons (the current workflow) resolve to themselves — QML Image loads
    // them directly, and so does Discord. Legacy asset keys still fall back to a
    // bundled/local resource for backward compatibility.
    if (key.startsWith(QLatin1String("http://")) || key.startsWith(QLatin1String("https://")))
        return key;
    const QString local = m_artStore.localPathForKey(key);
    if (!local.isEmpty()) return QUrl::fromLocalFile(local).toString();
    return QStringLiteral("qrc:/OmniPresence/resources/assets/") + key + QStringLiteral(".png");
}

QString AppController::previewTemplate(const QString& tmpl) const {
    return TemplateEngine::render(tmpl, m_currentWindow, m_integrationContext);
}

static QString readLogTail(const QString& fileName, int maxLines) {
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty()) base = QDir::tempPath();
    QFile f(base + QStringLiteral("/OmniPresence/") + fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QStringLiteral("(no log yet — it appears once activity is recorded)");
    QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) lines.removeLast();
    if (lines.size() > maxLines) lines = lines.mid(lines.size() - maxLines);
    return lines.join(QLatin1Char('\n'));
}

QString AppController::presenceEventsLog() const { return readLogTail(QStringLiteral("presence-events.log"), 300); }
QString AppController::appCoverageLog()    const { return readLogTail(QStringLiteral("app-coverage.log"), 300); }

QStringList AppController::artKeys() const {
    // Bundled defaults that ship as qrc resources, plus any user-added photos.
    QStringList keys{QStringLiteral("osrs"), QStringLiteral("code")};
    for (const QString& k : m_configStore->assetKeys().keys())
        if (!keys.contains(k)) keys.append(k);
    return keys;
}

QString AppController::presenceLargeImageSource() const { return sourceForKey(m_currentPresence.largeImageKey); }
QString AppController::presenceSmallImageSource() const { return sourceForKey(m_currentPresence.smallImageKey); }

QVariantList AppController::availableContextFields() const {
    const TemplateContext ctx = TemplateEngine::buildContext(m_currentWindow, m_integrationContext);
    static const QVector<QPair<QString, QString>> known = {
        {QStringLiteral("window.title"),      QStringLiteral("The window / tab title")},
        {QStringLiteral("browser.label"),     QStringLiteral("Show name from URL")},
        {QStringLiteral("browser.title"),     QStringLiteral("Page / video title")},
        {QStringLiteral("browser.site"),      QStringLiteral("Site name")},
        {QStringLiteral("browser.domain"),    QStringLiteral("The website domain")},
        {QStringLiteral("runelite.activity"), QStringLiteral("RuneScape activity")},
        {QStringLiteral("runelite.location"), QStringLiteral("RuneScape location")},
        {QStringLiteral("terminal.repo"),     QStringLiteral("Terminal repository")},
        {QStringLiteral("vscode.workspace"),  QStringLiteral("VS Code workspace")},
    };
    QVariantList out;
    for (const auto& k : known) {
        if (!ctx.value(k.first).isEmpty()) {
            out.append(QVariantMap{
                {QStringLiteral("token"), QStringLiteral("{{") + k.first + QStringLiteral("}}")},
                {QStringLiteral("label"), k.second},
            });
        }
    }
    return out;
}

// ── Rule CRUD bridge (Task 6) ──────────────────────────────────────────────────

QVariantList AppController::rulesList() const {
    QVariantList out;
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    for (int i = 0; i < rules.size(); ++i) {
        const Rule& r = rules[i];
        out.append(QVariantMap{
            {QStringLiteral("index"),    i},
            {QStringLiteral("name"),     r.name.isEmpty() ? QStringLiteral("(unnamed)") : r.name},
            {QStringLiteral("enabled"),  r.enabled},
            {QStringLiteral("priority"), r.priority},
        });
    }
    return out;
}

QVariantMap AppController::ruleAt(int index) const {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (index < 0 || index >= rules.size()) return {};
    return ruleToMap(index, rules[index]);
}

int AppController::addRule(const QVariantMap& draft) {
    Rule r;
    r.id      = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.name    = draft.value(QStringLiteral("name"), QStringLiteral("New Rule")).toString();
    r.enabled = draft.value(QStringLiteral("enabled"), true).toBool();
    r.priority= draft.value(QStringLiteral("priority"), 100).toInt();
    for (auto it = draft.constBegin(); it != draft.constEnd(); ++it)
        applyField(r, it.key(), it.value());
    normalizeRule(r);
    m_configStore->ruleSet().addRule(r);
    m_configStore->save();                 // persist immediately — survive restart
    emit rulesChanged();
    return m_configStore->ruleSet().rules().size() - 1;
}

void AppController::updateRuleField(int index, const QString& field, const QVariant& value) {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (index < 0 || index >= rules.size()) return;
    Rule r = rules[index];                 // copy
    applyField(r, field, value);
    normalizeRule(r);
    m_configStore->ruleSet().updateRule(r);  // matches by id
    m_configStore->save();                 // persist every edit — no lost rules
}

void AppController::deleteRule(int index) {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (index < 0 || index >= rules.size()) return;
    m_configStore->ruleSet().removeRule(rules[index].id);
    m_configStore->save();
    emit rulesChanged();
    evaluateAndPublish();
}

void AppController::saveRules() {
    m_configStore->save();
    emit rulesChanged();
    evaluateAndPublish();
}

// ── Capture → draft rule (Task 8) ──────────────────────────────────────────────

int AppController::seedRuleFromCapture() {
    if (m_currentWindow.processName.isEmpty()) return -1;
    Rule r;
    r.id                   = QUuid::createUuid().toString(QUuid::WithoutBraces);
    r.name                 = RuleEngine::genericPresence(m_currentWindow).name;
    r.enabled              = true;
    r.priority             = 50;
    r.matchProcessName     = m_currentWindow.processName;
    r.matchExecutablePath  = m_currentWindow.executablePath;
    r.activityType         = ActivityType::Playing;
    r.activityNameTemplate = r.name;       // main line pre-filled from the app name
    r.privacyLevel         = PrivacyLevel::Public;
    m_configStore->ruleSet().addRule(r);
    emit rulesChanged();
    return m_configStore->ruleSet().rules().size() - 1;
}

// ── Add photo (Task 9) ─────────────────────────────────────────────────────────

QString AppController::importPhoto(int ruleIndex, const QString& fileUrl) {
    const QString src = QUrl(fileUrl).toLocalFile();
    if (src.isEmpty()) return {};

    QString key = ArtStore::slugify(src);
    const QString base = key;
    int n = 1;
    while (!m_artStore.localPathForKey(key).isEmpty())
        key = base + QString::number(++n);

    QString out, err;
    if (!m_artStore.importImage(src, key, &out, &err)) {
        m_discordError = err;
        emit discordStatusChanged();
        return {};
    }

    return finishArtImport(ruleIndex, key, out);
}

QString AppController::finishArtImport(int ruleIndex, const QString& key, const QString& outPath) {
    updateRuleField(ruleIndex, QStringLiteral("largeImageKey"), key);
    m_configStore->setAssetKey(key, key);
    saveRules();

    // Open the portal Art Assets page + reveal the normalized file. No Playwright:
    // the user does the one drag-drop themselves.
    const QString appId = m_configStore->settings().discordAppId;
    if (!appId.isEmpty()) {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://discord.com/developers/applications/%1/rich-presence/assets").arg(appId)));
    }
    QProcess::startDetached(QStringLiteral("explorer.exe"),
        {QStringLiteral("/select,"), QDir::toNativeSeparators(outPath)});
    return key;
}

QString AppController::generateArt(int ruleIndex, const QString& monogram, const QString& accentHex) {
    const QList<Rule>& rules = m_configStore->ruleSet().rules();
    if (ruleIndex < 0 || ruleIndex >= rules.size()) return {};

    const QString name = rules[ruleIndex].name;
    const QString key  = ArtStore::slugify(name.isEmpty() ? QStringLiteral("art") : name);
    const QColor  accent = accentHex.isEmpty() ? QColor(QStringLiteral("#22d3ee"))
                                               : QColor(accentHex);
    const QString mono = monogram.isEmpty() ? name.left(2).toUpper() : monogram;

    QDir().mkpath(m_artStore.artDir());
    const QString out = QDir(m_artStore.artDir()).filePath(key + QStringLiteral(".png"));

    QString err;
    if (!ArtStore::renderMonogram(out, mono, accent, name, &err)) {
        m_discordError = err;
        emit discordStatusChanged();
        return {};
    }
    return finishArtImport(ruleIndex, key, out);
}

} // namespace OmniPresence
