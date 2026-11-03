// IntegrationContext.h — Holds the latest payload received from each integration.
// Integrations POST to http://127.0.0.1:47831/integrations/<source>/context.
// Designed for single-threaded use on the Qt event loop.
#pragma once

#include <QString>
#include <QJsonObject>
#include <QDateTime>
#include <QMap>

namespace OmniPresence {

/// Per-source integration payload with freshness metadata.
struct IntegrationPayload {
    QString     source;        ///< e.g. "runelite", "browser", "terminal", "vscode"
    QJsonObject data;          ///< Raw JSON fields from the POST body.
    QDateTime   receivedAt;
    int         maxAgeSeconds{120};   ///< Treat payload as stale after this many seconds.

    [[nodiscard]] bool isFresh() const noexcept {
        return receivedAt.secsTo(QDateTime::currentDateTimeUtc()) <= maxAgeSeconds;
    }

    /// Helper accessors — return empty string when field absent.
    [[nodiscard]] QString field(const QString& key) const;
};

/// Container for all integration payloads, keyed by source name.
class IntegrationContext {
public:
    /// Store or replace the payload for `source`.
    void update(const QString& source, const QJsonObject& data);

    /// Re-stamp an existing payload's freshness to "now" without changing its
    /// data, so a non-heartbeating feed (e.g. the RuneLite Discord-IPC capture,
    /// which only sends on change) does not decay to stale while its app is still
    /// active. Returns false if no payload exists for `source`.
    bool refresh(const QString& source);

    /// Retrieve the latest (possibly stale) payload for `source`.
    [[nodiscard]] const IntegrationPayload* get(const QString& source) const;

    /// Retrieve only if the payload is still within its freshness window.
    [[nodiscard]] const IntegrationPayload* getFresh(const QString& source) const;

    // ── Convenience accessors for well-known fields ───────────────────────────
    /// `key` from `source`'s payload as a string, or empty unless that payload is fresh.
    [[nodiscard]] QString freshField(const QString& source, const QString& key) const;
    [[nodiscard]] QString browserDomain()       const;
    [[nodiscard]] QString browserCategory()     const;

private:
    QMap<QString, IntegrationPayload> m_payloads;
};

} // namespace OmniPresence
