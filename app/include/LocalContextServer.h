// LocalContextServer.h — Loopback HTTP server for integration context ingestion.
//
// Accepts:  POST http://127.0.0.1:47831/integrations/<source>/context
// Body:     UTF-8 JSON object with integration-specific fields.
// Response: 200 OK {"status":"ok"} or 4xx on bad input.
//
// Bound to 127.0.0.1 ONLY — never exposed externally.
#pragma once

#include "IntegrationContext.h"
#include <QObject>
#include <QTcpServer>
#include <QMap>

namespace OmniPresence {

/// Minimal loopback HTTP/1.1 server.  Only handles the single POST route above.
class LocalContextServer : public QObject {
    Q_OBJECT
public:
    static constexpr quint16 DEFAULT_PORT = 47831;

    explicit LocalContextServer(IntegrationContext* context,
                                quint16 port = DEFAULT_PORT,
                                QObject* parent = nullptr);
    ~LocalContextServer() override;

    bool start();
    void stop();

    [[nodiscard]] bool isListening()  const;
    [[nodiscard]] quint16 listenPort() const noexcept { return m_port; }

signals:
    /// Emitted after IntegrationContext is updated with a new payload.
    void contextUpdated(const QString& source);
    /// Emitted when the server encounters an unrecoverable error.
    void serverError(const QString& message);

private slots:
    void onNewConnection();

private:
    /// Parse and dispatch an HTTP request from the raw byte buffer.
    void handleRequest(class QTcpSocket* socket, const QByteArray& raw);

    /// Send a minimal HTTP response.
    static void sendResponse(class QTcpSocket* socket,
                             int statusCode,
                             const QString& statusText,
                             const QByteArray& body);

    IntegrationContext* m_context{nullptr};
    QTcpServer          m_server;
    quint16             m_port;
};

} // namespace OmniPresence
