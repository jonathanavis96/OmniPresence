// test_local_context_server.cpp — request framing over a real loopback socket.
#include <QtTest>
#include <QTcpSocket>
#include "LocalContextServer.h"
#include "IntegrationContext.h"

using namespace OmniPresence;

class TestLocalContextServer : public QObject {
    Q_OBJECT

    // Send `parts` with a pause between each, then return the full response.
    static QByteArray exchange(quint16 port, const QList<QByteArray>& parts) {
        QTcpSocket s;
        s.connectToHost(QHostAddress::LocalHost, port);
        if (!s.waitForConnected(2000)) return {};
        for (const QByteArray& part : parts) {
            s.write(part);
            s.waitForBytesWritten(2000);
            QTest::qWait(50);   // let the server see this part as its own readyRead
        }
        QByteArray out;
        QElapsedTimer t; t.start();
        while (s.state() != QAbstractSocket::UnconnectedState && t.elapsed() < 2000) {
            QTest::qWait(10);
            out += s.readAll();
        }
        return out + s.readAll();
    }

private slots:
    // Headers and body in separate segments (as fetch() often sends them) are one
    // request, not an "Empty body" 400 on the first segment.
    void bodyInLaterSegmentIsAccepted() {
        IntegrationContext ctx;
        LocalContextServer server(&ctx, 0);
        QVERIFY(server.start());
        const QByteArray body = R"({"domain":"example.com"})";
        const QByteArray head = "POST /integrations/browser/context HTTP/1.1\r\n"
                                "Content-Type: application/json\r\n"
                                "Content-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
        const QByteArray resp = exchange(server.listenPort(), {head, body});
        QVERIFY2(resp.startsWith("HTTP/1.1 200"), resp.constData());
        QVERIFY(ctx.get(QStringLiteral("browser")));
        QCOMPARE(ctx.get(QStringLiteral("browser"))->field(QStringLiteral("domain")),
                 QStringLiteral("example.com"));
    }

    // A request over the cap is refused rather than buffered without bound.
    void oversizedRequestIsRefused() {
        IntegrationContext ctx;
        LocalContextServer server(&ctx, 0);
        QVERIFY(server.start());
        const QByteArray head = "POST /integrations/browser/context HTTP/1.1\r\n"
                                "Content-Length: 999999\r\n\r\n";
        const QByteArray resp = exchange(server.listenPort(),
            {head, QByteArray(LocalContextServer::MAX_REQUEST_BYTES + 1, 'x')});
        // The server may close while the client is still writing, and the reset
        // can drop the 413 before it is read — so accept no response, never a 200.
        QVERIFY2(resp.isEmpty() || resp.startsWith("HTTP/1.1 413"), resp.constData());
        QVERIFY(!ctx.get(QStringLiteral("browser")));
    }

    // Single-segment requests keep working exactly as before.
    void singleSegmentRequestIsAccepted() {
        IntegrationContext ctx;
        LocalContextServer server(&ctx, 0);
        QVERIFY(server.start());
        const QByteArray req = "POST /integrations/vscode/context HTTP/1.1\r\n"
                               "Content-Length: 17\r\n\r\n{\"workspace\":\"w\"}";
        const QByteArray resp = exchange(server.listenPort(), {req});
        QVERIFY2(resp.startsWith("HTTP/1.1 200"), resp.constData());
        QCOMPARE(ctx.freshField(QStringLiteral("vscode"), QStringLiteral("workspace")), QStringLiteral("w"));
    }
};

QTEST_MAIN(TestLocalContextServer)
#include "test_local_context_server.moc"
