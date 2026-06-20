// DiscordPresenceClient.cpp — Discord Social SDK wrapper.
//
// Real Rich Presence path (OMNIPRESENCE_WITH_DISCORD=ON) uses the official
// Discord Social SDK (v1.9). Flow:
//
//   1. std::make_shared<discordpp::Client>()
//   2. SetStatusChangedCallback(...)                       -> watch for Ready
//   3. No saved token: CreateAuthorizationCodeVerifier ->
//      Authorize(args)  (opens browser, http://127.0.0.1/callback) ->
//      GetToken -> persist refresh+access -> UpdateToken -> Connect
//      Saved token: UpdateToken(saved) -> Connect
//   4. On Status::Ready: UpdateRichPresence(activity, cb)
//   5. discordpp::RunCallbacks() pumped from a QTimer (runCallbacks()).
//
// All SDK callbacks fire inside RunCallbacks() on the main (GUI) thread, so it
// is safe to touch Qt state and emit signals directly from them.

#include "DiscordPresenceClient.h"
#include <QDebug>

#ifdef OMNIPRESENCE_WITH_DISCORD
#include <discordpp.h>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <ctime>
#endif

namespace OmniPresence {

DiscordPresenceClient::DiscordPresenceClient(QObject* parent)
    : QObject(parent)
{}

DiscordPresenceClient::~DiscordPresenceClient() {
    disconnectFromDiscord();
}

void DiscordPresenceClient::connectToDiscord(const QString& appId) {
    m_appId = appId;
    m_appIdNum = appId.toULongLong();

#ifdef OMNIPRESENCE_WITH_DISCORD
    if (m_appIdNum == 0) {
        qWarning() << "[DiscordPresenceClient] Invalid application ID:" << appId;
        m_status = DiscordConnectionStatus::Error;
        emit connectionStatusChanged(m_status);
        emit sdkError(QStringLiteral("Invalid Discord application ID."));
        return;
    }

    m_ready = false;
    m_reauthAttempted = false;
    m_status = DiscordConnectionStatus::Connecting;
    emit connectionStatusChanged(m_status);

    m_client = std::make_shared<discordpp::Client>();

    m_client->SetStatusChangedCallback(
        [this](discordpp::Client::Status status,
               discordpp::Client::Error error,
               int32_t errorDetail) {
            switch (status) {
            case discordpp::Client::Status::Ready:
                m_ready = true;
                m_status = DiscordConnectionStatus::Connected;
                emit connectionStatusChanged(m_status);
                qDebug() << "[DiscordPresenceClient] Client ready.";
                if (m_hasPending) {
                    sendPresenceNow();
                }
                break;
            case discordpp::Client::Status::Connecting:
            case discordpp::Client::Status::Connected:
            case discordpp::Client::Status::Reconnecting:
            case discordpp::Client::Status::HttpWait:
                m_status = DiscordConnectionStatus::Connecting;
                emit connectionStatusChanged(m_status);
                break;
            case discordpp::Client::Status::Disconnecting:
            case discordpp::Client::Status::Disconnected:
            default:
                // A saved token that fails to bring us to Ready is almost
                // certainly expired/revoked — drop it and re-authorize once.
                if (m_usingSavedToken && !m_ready && !m_reauthAttempted) {
                    m_reauthAttempted = true;
                    m_usingSavedToken = false;
                    qDebug() << "[DiscordPresenceClient] Saved token rejected — re-authorizing.";
                    clearSavedTokens();
                    beginAuthorization();
                    break;
                }
                m_ready = false;
                m_status = DiscordConnectionStatus::Disconnected;
                emit connectionStatusChanged(m_status);
                if (static_cast<int>(error) != 0) {
                    emit sdkError(QStringLiteral("Discord error %1 (detail %2).")
                                      .arg(static_cast<int>(error))
                                      .arg(errorDetail));
                }
                break;
            }
        });

    std::string savedToken;
    if (loadSavedAccessToken(savedToken)) {
        qDebug() << "[DiscordPresenceClient] Reconnecting with saved token.";
        m_usingSavedToken = true;
        applyTokenAndConnect(savedToken);
    } else {
        m_usingSavedToken = false;
        beginAuthorization();
    }
#else
    qDebug() << "[DiscordPresenceClient] Preview mode — Discord SDK not linked."
             << "App ID:" << appId;
    m_status = DiscordConnectionStatus::Connected;   // Pretend connected in preview.
    emit connectionStatusChanged(m_status);
#endif
}

#ifdef OMNIPRESENCE_WITH_DISCORD
void DiscordPresenceClient::beginAuthorization() {
    if (!m_client) return;

    auto verifier = m_client->CreateAuthorizationCodeVerifier();

    discordpp::AuthorizationArgs args;
    args.SetClientId(m_appIdNum);
    args.SetScopes(discordpp::Client::GetDefaultPresenceScopes());
    args.SetCodeChallenge(verifier.Challenge());

    // `verifier` must survive until GetToken — capture a copy by value.
    m_client->Authorize(
        args,
        [this, verifier](discordpp::ClientResult result,
                         std::string code,
                         std::string redirectUri) {
            if (!result.Successful()) {
                qWarning() << "[DiscordPresenceClient] Authorize failed.";
                m_status = DiscordConnectionStatus::Error;
                emit connectionStatusChanged(m_status);
                emit sdkError(QStringLiteral("Discord authorization failed or was cancelled."));
                return;
            }
            m_client->GetToken(
                m_appIdNum, code, verifier.Verifier(), redirectUri,
                [this](discordpp::ClientResult tokenResult,
                       std::string accessToken,
                       std::string refreshToken,
                       discordpp::AuthorizationTokenType /*tokenType*/,
                       int32_t /*expiresIn*/,
                       std::string /*scopes*/) {
                    if (!tokenResult.Successful()) {
                        qWarning() << "[DiscordPresenceClient] Token exchange failed.";
                        m_status = DiscordConnectionStatus::Error;
                        emit connectionStatusChanged(m_status);
                        emit sdkError(QStringLiteral("Discord token exchange failed."));
                        return;
                    }
                    saveTokens(accessToken, refreshToken);
                    applyTokenAndConnect(accessToken);
                });
        });
}

void DiscordPresenceClient::applyTokenAndConnect(const std::string& accessToken) {
    if (!m_client) return;
    m_client->UpdateToken(
        discordpp::AuthorizationTokenType::Bearer, accessToken,
        [this](discordpp::ClientResult result) {
            if (!result.Successful()) {
                qWarning() << "[DiscordPresenceClient] UpdateToken failed.";
                emit sdkError(QStringLiteral("Discord token update failed."));
                return;
            }
            m_client->Connect();
        });
}
#endif

void DiscordPresenceClient::disconnectFromDiscord() {
#ifdef OMNIPRESENCE_WITH_DISCORD
    // Destroying the shared client tears down the connection.
    m_client.reset();
    m_ready = false;
    m_hasPending = false;
#endif
    m_status = DiscordConnectionStatus::Disconnected;
    emit connectionStatusChanged(m_status);
}

void DiscordPresenceClient::updatePresence(const PresencePayload& payload) {
    // Skip the call if the presence is unchanged (avoids unnecessary API hits).
    if (payload.isSamePresence(m_lastSentPayload)) {
        return;
    }
    m_lastSentPayload = payload;

#ifdef OMNIPRESENCE_WITH_DISCORD
    m_hasPending = true;
    if (m_ready) {
        sendPresenceNow();
    } else {
        qDebug() << "[DiscordPresenceClient] Presence queued (client not ready):"
                 << payload.name;
    }
#else
    qDebug() << "[DiscordPresenceClient] [preview] Presence:"
             << payload.name << "|" << payload.details << "|" << payload.state;
    emit presenceUpdated(payload);
#endif
}

#ifdef OMNIPRESENCE_WITH_DISCORD
namespace {
discordpp::ActivityTypes toSdkActivityType(ActivityType t) {
    switch (t) {
    case ActivityType::Playing:   return discordpp::ActivityTypes::Playing;
    case ActivityType::Listening: return discordpp::ActivityTypes::Listening;
    case ActivityType::Watching:  return discordpp::ActivityTypes::Watching;
    case ActivityType::Competing: return discordpp::ActivityTypes::Competing;
    case ActivityType::Custom:    return discordpp::ActivityTypes::CustomStatus;
    }
    return discordpp::ActivityTypes::Playing;
}
} // namespace

void DiscordPresenceClient::sendPresenceNow() {
    if (!m_client || !m_ready) return;
    const PresencePayload& p = m_lastSentPayload;

    discordpp::Activity activity;
    activity.SetType(toSdkActivityType(p.activityType));
    activity.SetName(p.name.toStdString());
    if (!p.details.isEmpty()) activity.SetDetails(p.details.toStdString());
    if (!p.state.isEmpty())   activity.SetState(p.state.toStdString());

    // Show the "details" line prominently in the member list / status, e.g.
    // "Training Slayer" instead of "Playing Old School RuneScape".
    activity.SetStatusDisplayType(discordpp::StatusDisplayTypes::Details);

    if (!p.largeImageKey.isEmpty() || !p.smallImageKey.isEmpty()) {
        discordpp::ActivityAssets assets;
        if (!p.largeImageKey.isEmpty())  assets.SetLargeImage(p.largeImageKey.toStdString());
        if (!p.largeImageText.isEmpty()) assets.SetLargeText(p.largeImageText.toStdString());
        if (!p.smallImageKey.isEmpty())  assets.SetSmallImage(p.smallImageKey.toStdString());
        if (!p.smallImageText.isEmpty()) assets.SetSmallText(p.smallImageText.toStdString());
        activity.SetAssets(assets);
    }

    if (p.timestampMode != TimestampMode::None && p.activityStartedAt.isValid()) {
        discordpp::ActivityTimestamps ts;
        ts.SetStart(static_cast<uint64_t>(p.activityStartedAt.toSecsSinceEpoch()));
        activity.SetTimestamps(ts);
    }

    m_client->UpdateRichPresence(
        activity,
        [this, p](discordpp::ClientResult result) {
            if (result.Successful()) {
                m_hasPending = false;
                emit presenceUpdated(p);
            } else {
                qWarning() << "[DiscordPresenceClient] UpdateRichPresence failed for"
                           << p.name;
                emit sdkError(QStringLiteral("Failed to publish Rich Presence."));
            }
        });
}
#endif

void DiscordPresenceClient::clearPresence() {
#ifdef OMNIPRESENCE_WITH_DISCORD
    if (m_client && m_ready) {
        // An empty activity clears the user's Rich Presence.
        discordpp::Activity activity;
        activity.SetType(discordpp::ActivityTypes::Playing);
        activity.SetName(std::string{});
        m_client->UpdateRichPresence(activity, [](discordpp::ClientResult) {});
    }
    m_hasPending = false;
#else
    qDebug() << "[DiscordPresenceClient] [preview] clearPresence()";
#endif
    m_lastSentPayload = {};
}

void DiscordPresenceClient::runCallbacks() {
#ifdef OMNIPRESENCE_WITH_DISCORD
    // The Social SDK uses a single global callback pump.
    discordpp::RunCallbacks();
#endif
}

DiscordConnectionStatus DiscordPresenceClient::connectionStatus() const noexcept {
    return m_status;
}

bool DiscordPresenceClient::isConnected() const noexcept {
    return m_status == DiscordConnectionStatus::Connected;
}

#ifdef OMNIPRESENCE_WITH_DISCORD
// ── Token persistence ────────────────────────────────────────────────────────
// Stored in the per-user app config dir. The refresh token is the sensitive
// long-lived secret; the access token lets us reconnect without a browser prompt
// until it expires, at which point we transparently re-authorize.

QString DiscordPresenceClient::tokenStorePath() const {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (dir.isEmpty()) {
        dir = QDir::homePath() + QStringLiteral("/.omnipresence");
    }
    QDir().mkpath(dir);
    // mkpath does not restrict mode — make the config dir owner-only.
    QFile::setPermissions(dir, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    return dir + QStringLiteral("/discord_tokens.json");
}

void DiscordPresenceClient::saveTokens(const std::string& accessToken,
                                       const std::string& refreshToken) {
    QJsonObject obj;
    obj[QStringLiteral("access_token")]  = QString::fromStdString(accessToken);
    obj[QStringLiteral("refresh_token")] = QString::fromStdString(refreshToken);
    obj[QStringLiteral("application_id")] = m_appId;

    const QString path = tokenStorePath();

    // Pre-create the file and lock it down to owner-only BEFORE writing the
    // secret, so the refresh token is never momentarily world-readable.
    QFile f(path);
    if (!f.exists()) {
        if (f.open(QIODevice::WriteOnly)) f.close();
    }
    if (!QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner)) {
        qWarning() << "[DiscordPresenceClient] Could not restrict token file permissions.";
    }

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        f.close();
    } else {
        qWarning() << "[DiscordPresenceClient] Could not write token store:" << f.errorString();
    }
}

bool DiscordPresenceClient::loadSavedAccessToken(std::string& accessTokenOut) const {
    QFile f(tokenStorePath());
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;
    const QJsonObject obj = doc.object();

    // Only reuse the token if it belongs to this same application.
    if (obj.value(QStringLiteral("application_id")).toString() != m_appId) return false;
    const QString token = obj.value(QStringLiteral("access_token")).toString();
    if (token.isEmpty()) return false;

    accessTokenOut = token.toStdString();
    return true;
}

void DiscordPresenceClient::clearSavedTokens() {
    QFile::remove(tokenStorePath());
}
#endif

} // namespace OmniPresence
