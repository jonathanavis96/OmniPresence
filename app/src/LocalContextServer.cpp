// LocalContextServer.cpp — Minimal loopback HTTP/1.1 server.
// Only handles: POST /integrations/<source>/context
// Bound to 127.0.0.1 only.
#include "LocalContextServer.h"
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace OmniPresence {

LocalContextServer::LocalContextServer(IntegrationContext* context,
                                       quint16 port,
                                       QObject* parent)
    : QObject(parent)
    , m_context(context)
    , m_port(port)
{
    connect(&m_server, &QTcpServer::newConnection,
            this,      &LocalContextServer::onNewConnection);
}

LocalContextServer::~LocalContextServer() {
    stop();
}

bool LocalContextServer::start() {
    const bool ok = m_server.listen(QHostAddress::LocalHost, m_port);
    if (!ok) {
        const QString msg = QStringLiteral("LocalContextServer: failed to listen on 127.0.0.1:%1 — %2")
                                .arg(m_port)
                                .arg(m_server.errorString());
        qWarning() << msg;
        emit serverError(msg);
    } else {
        qDebug() << "[LocalContextServer] Listening on 127.0.0.1:" << m_port;
    }
    return ok;
}

void LocalContextServer::stop() {
    m_server.close();
}

bool LocalContextServer::isListening() const {
    return m_server.isListening();
}

// ── New connection ────────────────────────────────────────────────────────────

void LocalContextServer::onNewConnection() {
    while (m_server.hasPendingConnections()) {
        QTcpSocket* socket = m_server.nextPendingConnection();
        // Read the entire request (simple synchronous read for a loopback connection).
        // In production this could be made async; for a local-only server it's fine.
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            const QByteArray raw = socket->readAll();
            handleRequest(socket, raw);
        });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
    }
}

// ── Request handling ──────────────────────────────────────────────────────────

void LocalContextServer::handleRequest(QTcpSocket* socket, const QByteArray& raw) {
    // ── Parse the request line ────────────────────────────────────────────────
    const int firstLine = raw.indexOf('\n');
    if (firstLine < 0) {
        sendResponse(socket, 400, QStringLiteral("Bad Request"), "{}");
        return;
    }
    const QByteArray requestLine = raw.left(firstLine).trimmed();
    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        sendResponse(socket, 400, QStringLiteral("Bad Request"), "{}");
        return;
    }

    const QString method = QString::fromUtf8(parts[0]);
    const QString path   = QString::fromUtf8(parts[1]);

    // ── Only handle POST /integrations/<source>/context ───────────────────────
    static const QString kPrefix = QStringLiteral("/integrations/");
    static const QString kSuffix = QStringLiteral("/context");

    if (method != QLatin1String("POST")
        || !path.startsWith(kPrefix)
        || !path.endsWith(kSuffix)) {
        sendResponse(socket, 404, QStringLiteral("Not Found"),
                     R"({"error":"Not found"})");
        return;
    }

    const QString source = path.mid(kPrefix.length(),
                                    path.length() - kPrefix.length() - kSuffix.length());
    if (source.isEmpty()) {
        sendResponse(socket, 400, QStringLiteral("Bad Request"),
                     R"({"error":"Missing source"})");
        return;
    }

    // ── Extract the body (after the blank line separating headers and body) ───
    const int bodyStart = raw.indexOf("\r\n\r\n");
    const int altStart  = raw.indexOf("\n\n");
    QByteArray body;
    if (bodyStart >= 0)      body = raw.mid(bodyStart + 4);
    else if (altStart >= 0)  body = raw.mid(altStart + 2);

    if (body.isEmpty()) {
        sendResponse(socket, 400, QStringLiteral("Bad Request"),
                     R"({"error":"Empty body"})");
        return;
    }

    // ── Parse JSON ────────────────────────────────────────────────────────────
    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        sendResponse(socket, 400, QStringLiteral("Bad Request"),
                     QStringLiteral(R"({"error":"%1"})").arg(parseErr.errorString()).toUtf8());
        return;
    }

    qDebug() << "[LocalContextServer] integration POST from" << source
             << "fields:" << doc.object().keys();
    m_context->update(source, doc.object());
    emit contextUpdated(source);

    sendResponse(socket, 200, QStringLiteral("OK"), R"({"status":"ok"})");
}

// ── HTTP response ─────────────────────────────────────────────────────────────

void LocalContextServer::sendResponse(QTcpSocket* socket,
                                      int statusCode,
                                      const QString& statusText,
                                      const QByteArray& body)
{
    const QByteArray response =
        QStringLiteral("HTTP/1.1 %1 %2\r\n"
                        "Content-Type: application/json\r\n"
                        "Content-Length: %3\r\n"
                        "Connection: close\r\n"
                        "\r\n")
        .arg(statusCode)
        .arg(statusText)
        .arg(body.length())
        .toUtf8()
        + body;

    socket->write(response);
    socket->disconnectFromHost();
}

} // namespace OmniPresence
