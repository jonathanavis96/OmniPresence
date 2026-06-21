// test_config_assets.cpp — assetKeys persistence round-trip.
#include <QtTest>
#include <QTemporaryDir>
#include "ConfigStore.h"

using namespace OmniPresence;

class TestConfigAssets : public QObject {
    Q_OBJECT
private slots:
    void assetKeysRoundTrip() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("c.json"));

        ConfigStore a;
        a.setConfigPathForTest(path);
        a.setAssetKey(QStringLiteral("osrs"), QStringLiteral("Playing OSRS"));
        a.setAssetKey(QStringLiteral("code"), QStringLiteral("Writing code"));
        QVERIFY(a.save());

        ConfigStore b;
        b.setConfigPathForTest(path);
        QVERIFY(b.load());
        QCOMPARE(b.assetKeys().value(QStringLiteral("osrs")), QStringLiteral("Playing OSRS"));
        QCOMPARE(b.assetKeys().value(QStringLiteral("code")), QStringLiteral("Writing code"));
    }
};

QTEST_MAIN(TestConfigAssets)
#include "test_config_assets.moc"
