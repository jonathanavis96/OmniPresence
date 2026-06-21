// DiscordPresenceClient.h — Wrapper around the Discord Social SDK.
//
// When OMNIPRESENCE_WITH_DISCORD is NOT defined (the default for dev builds),
// all calls are no-ops that log to qDebug — "preview mode".
//
// Discord Social SDK C++ getting-started reference:
//   https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B
//
// Rich Presence with the Social SDK requires a per-user OAuth2 flow:
//   CreateAuthorizationCodeVerifier -> Authorize (opens browser) -> GetToken
//   -> UpdateToken -> Connect -> (Status::Ready) -> UpdateRichPresence.
// The refresh token is persisted so subsequent launches reconnect silently.
#pragma once

#include "PresencePayload.h"
#include <QObject>
#include <QString>
#include <memory>

// Forward-declare SDK types so this header compiles without the SDK present.
#ifdef OMNIPRESENCE_WITH_DISCORD
namespace discordpp {
    class Client;
}
#endif

namespace OmniPresence {

enum class DiscordConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Error,
};

class DiscordPresenceClient : public QObject {
    Q_OBJECT
public:
    explicit DiscordPresenceClient(QObject* parent = nullptr);
    ~DiscordPresenceClient() override;

    /// Initialise the SDK and begin the OAuth handshake (or reconnect with a
    /// saved token if one is present on disk).
    /// @param appId  The Discord application ID (numeric string from Developer Portal).
    void connectToDiscord(const QString& appId);

    /// Disconnect and release SDK resources.
    void disconnectFromDiscord();

    /// Push a new presence to Discord.  Skips the call if presence is unchanged,
    /// and defers the call until the client reaches Status::Ready.
    void updatePresence(const PresencePayload& payload);

    /// Clear the user's Rich Presence entirely.
    void clearPresence();

    /// Drive the SDK callback loop — call this from a QTimer (e.g. every 100 ms).
    void runCallbacks();

    [[nodiscard]] DiscordConnectionStatus connectionStatus() const noexcept;
    [[nodiscard]] bool isConnected() const noexcept;

signals:
    void connectionStatusChanged(DiscordConnectionStatus status);
    void presenceUpdated(const OmniPresence::PresencePayload& payload);
    void sdkError(const QString& message);

private:
#ifdef OMNIPRESENCE_WITH_DISCORD
    // The Social SDK manages the Client via shared ownership (callbacks capture it).
    std::shared_ptr<discordpp::Client> m_client;

    /// Begin the full browser-based OAuth2 authorization (used when no valid
    /// saved token exists, or after an auth failure).
    void beginAuthorization();
    /// Apply an access token to the client and Connect().
    void applyTokenAndConnect(const std::string& accessToken);
    /// Push the last-resolved payload to Discord (only when Status::Ready).
    void sendPresenceNow();
    /// Build + send a single UpdateRichPresence call. When @p withAssets is true
    /// the large/small image keys are attached; if Discord then rejects the call
    /// because an art asset can't be resolved (ErrorType 6 — key not uploaded to
    /// the portal), it retries once with @p withAssets=false so the text presence
    /// still publishes instead of being dropped entirely.
    void publishActivity(const PresencePayload& payload, bool withAssets);

    // Token persistence (config dir / discord_tokens.json).
    [[nodiscard]] QString tokenStorePath() const;
    void saveTokens(const std::string& accessToken, const std::string& refreshToken);
    [[nodiscard]] bool loadSavedAccessToken(std::string& accessTokenOut) const;
    void clearSavedTokens();

    bool m_ready{false};          ///< True once Status::Ready reached.
    bool m_hasPending{false};     ///< A presence is waiting to be sent on Ready.
    bool m_usingSavedToken{false};///< Current connect attempt used a stored token.
    bool m_reauthAttempted{false};///< Guard so a bad saved token re-auths only once.
#endif
    DiscordConnectionStatus m_status{DiscordConnectionStatus::Disconnected};
    PresencePayload         m_lastSentPayload;
    QString                 m_appId;
    unsigned long long      m_appIdNum{0};
};

} // namespace OmniPresence
