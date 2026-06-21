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

    /// Retrieve the latest (possibly stale) payload for `source`.
    [[nodiscard]] const IntegrationPayload* get(const QString& source) const;

    /// Retrieve only if the payload is still within its freshness window.
    [[nodiscard]] const IntegrationPayload* getFresh(const QString& source) const;

    /// Evict all stale payloads.
    void purgeStale();

    /// Clear a specific source.
    void clear(const QString& source);

    /// Clear everything.
    void clearAll();

    // ── Convenience accessors for well-known fields ───────────────────────────
    [[nodiscard]] QString browserDomain()       const;
    [[nodiscard]] QString browserCategory()     const;
    [[nodiscard]] QString browserTitle()        const;  ///< Whitelisted tab title (safe_title), else empty.
    [[nodiscard]] QString browserLabel()        const;  ///< Smart label from the URL path (page_label), else empty.
    [[nodiscard]] QString terminalCwd()         const;
    [[nodiscard]] QString terminalRepo()        const;
    [[nodiscard]] QString terminalCommandSummary() const;
    [[nodiscard]] QString vscodeWorkspace()     const;
    [[nodiscard]] QString runeliteActivity()    const;
    [[nodiscard]] QString runeliteTarget()      const;
    [[nodiscard]] QString runeliteSkill()       const;
    [[nodiscard]] QString runeliteLocation()    const;
    [[nodiscard]] QString runeliteConfidence()  const;

private:
    QMap<QString, IntegrationPayload> m_payloads;
};

} // namespace OmniPresence
