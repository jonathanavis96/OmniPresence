// DiscordPresenceClient.h — Wrapper around the Discord Social SDK.
//
// When OMNIPRESENCE_WITH_DISCORD is NOT defined (the default for dev builds),
// all calls are no-ops that log to qDebug — "preview mode".
//
// Discord Social SDK C++ getting-started reference:
//   https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B
//
// TODO: Verify exact SDK symbol names before enabling real Discord linkage.
//       The comments marked "// TODO: verify exact Social SDK symbol" indicate
//       places where the SDK API surface may differ from what's written here.
#pragma once

#include "PresencePayload.h"
#include <QObject>
#include <QString>
#include <memory>

// Forward-declare SDK types so this header compiles without the SDK present.
#ifdef OMNIPRESENCE_WITH_DISCORD
namespace discordpp {
    class Client;   // TODO: verify exact Social SDK symbol
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

    /// Initialise the SDK and begin the OAuth handshake.
    /// @param appId  The Discord application ID (numeric string from Developer Portal).
    void connectToDiscord(const QString& appId);

    /// Disconnect and release SDK resources.
    void disconnectFromDiscord();

    /// Push a new presence to Discord.  Skips the call if presence is unchanged.
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
    // TODO: verify exact Social SDK symbol — may be discordpp::Client or similar
    std::unique_ptr<discordpp::Client> m_client;
#endif
    DiscordConnectionStatus m_status{DiscordConnectionStatus::Disconnected};
    PresencePayload         m_lastSentPayload;
    QString                 m_appId;
};

} // namespace OmniPresence
