// DiscordPresenceClient.cpp — Discord Social SDK wrapper.
//
// ─────────────────────────────────────────────────────────────────────────────
// HOW TO WIRE THE REAL SDK
// ─────────────────────────────────────────────────────────────────────────────
// Official docs: https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B
//
// 1. Download the SDK from the Developer Portal; place it in third_party/discord_social_sdk/.
// 2. Pass -DOMNIPRESENCE_WITH_DISCORD=ON to CMake.
// 3. Fill in the TODO blocks below with the verified symbol names from the SDK headers.
//
// Rough SDK flow (subject to verification — TODO: verify exact Social SDK symbols):
//
//   discordpp::ClientConfig cfg;                    // TODO: verify exact Social SDK symbol
//   cfg.SetApplicationId(appId);                    // TODO: verify exact Social SDK symbol
//   m_client = discordpp::Client::Create(cfg);      // TODO: verify exact Social SDK symbol
//   m_client->Connect();                            // TODO: verify exact Social SDK symbol
//
//   // In the callback timer (runCallbacks):
//   m_client->RunCallbacks();                       // TODO: verify exact Social SDK symbol
//
//   // To publish presence:
//   discordpp::Activity act;                        // TODO: verify exact Social SDK symbol
//   act.SetType(discordpp::ActivityType::Playing);  // TODO: verify exact Social SDK symbol — enum values
//   act.SetName(payload.name.toStdString());        // TODO: verify exact Social SDK symbol
//   act.SetDetails(payload.details.toStdString());  // TODO: verify exact Social SDK symbol
//   act.SetState(payload.state.toStdString());      // TODO: verify exact Social SDK symbol
//   discordpp::ActivityAssets assets;               // TODO: verify exact Social SDK symbol
//   assets.SetLargeImage(payload.largeImageKey.toStdString()); // TODO: verify
//   assets.SetLargeText(payload.largeImageText.toStdString()); // TODO: verify
//   act.SetAssets(assets);                          // TODO: verify exact Social SDK symbol
//   if (!payload.activityStartedAt.isNull()) {
//       discordpp::ActivityTimestamps ts;           // TODO: verify exact Social SDK symbol
//       ts.SetStart(payload.activityStartedAt.toSecsSinceEpoch()); // TODO: verify
//       act.SetTimestamps(ts);                      // TODO: verify exact Social SDK symbol
//   }
//   // StatusDisplayType::Details — shows details line prominently on profile card.
//   act.SetStatusDisplayType(discordpp::StatusDisplayType::Details); // TODO: verify exact Social SDK symbol
//   m_client->UpdateActivity(act, [](discordpp::Result r) {         // TODO: verify exact Social SDK symbol
//       if (r != discordpp::Result::Success) { /* handle error */ }  // TODO: verify
//   });
// ─────────────────────────────────────────────────────────────────────────────

#include "DiscordPresenceClient.h"
#include <QDebug>

#ifdef OMNIPRESENCE_WITH_DISCORD
// TODO: Replace with the real SDK header path once the SDK is placed under third_party/.
// #include <discordpp.h>   // TODO: verify exact Social SDK symbol (header name)
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

#ifdef OMNIPRESENCE_WITH_DISCORD
    qDebug() << "[DiscordPresenceClient] Connecting to Discord with app ID:" << appId;
    // TODO: Implement using verified SDK symbols (see header comment above).
    m_status = DiscordConnectionStatus::Connecting;
    emit connectionStatusChanged(m_status);
#else
    qDebug() << "[DiscordPresenceClient] Preview mode — Discord SDK not linked."
             << "App ID:" << appId;
    m_status = DiscordConnectionStatus::Connected;   // Pretend connected in preview.
    emit connectionStatusChanged(m_status);
#endif
}

void DiscordPresenceClient::disconnectFromDiscord() {
#ifdef OMNIPRESENCE_WITH_DISCORD
    // TODO: m_client->Disconnect() — verify exact Social SDK symbol
    m_client.reset();
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
    // TODO: Build discordpp::Activity and call m_client->UpdateActivity().
    //       See the comment block at the top of this file for the expected structure.
    qDebug() << "[DiscordPresenceClient] Updating presence (real SDK — TODO implement):"
             << payload.name << "|" << payload.details << "|" << payload.state;
#else
    qDebug() << "[DiscordPresenceClient] [preview] Presence:"
             << payload.name << "|" << payload.details << "|" << payload.state;
#endif

    emit presenceUpdated(payload);
}

void DiscordPresenceClient::clearPresence() {
#ifdef OMNIPRESENCE_WITH_DISCORD
    // TODO: m_client->ClearActivity(callback) — verify exact Social SDK symbol
#else
    qDebug() << "[DiscordPresenceClient] [preview] clearPresence()";
#endif
    m_lastSentPayload = {};
}

void DiscordPresenceClient::runCallbacks() {
#ifdef OMNIPRESENCE_WITH_DISCORD
    if (m_client) {
        // TODO: m_client->RunCallbacks() — verify exact Social SDK symbol
    }
#endif
}

DiscordConnectionStatus DiscordPresenceClient::connectionStatus() const noexcept {
    return m_status;
}

bool DiscordPresenceClient::isConnected() const noexcept {
    return m_status == DiscordConnectionStatus::Connected;
}

} // namespace OmniPresence
