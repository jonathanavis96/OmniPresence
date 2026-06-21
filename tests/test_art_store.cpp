// test_art_store.cpp — slugify, normalize-to-1024-PNG, key lookup.
#include <QtTest>
#include <QImage>
#include <QTemporaryDir>
#include "ArtStore.h"

using namespace OmniPresence;

class TestArtStore : public QObject {
    Q_OBJECT
private slots:
    void slugifyLowercasesAndStrips() {
        QCOMPARE(ArtStore::slugify(QStringLiteral("My Photo!.png")), QStringLiteral("my_photo"));
        QCOMPARE(ArtStore::slugify(QStringLiteral("OSRS")),          QStringLiteral("osrs"));
        QCOMPARE(ArtStore::slugify(QStringLiteral("a   b")),         QStringLiteral("a_b"));
    }

    void importNormalisesTo1024Png() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString src = tmp.filePath(QStringLiteral("in.png"));
        QImage img(200, 80, QImage::Format_RGB32);
        img.fill(Qt::cyan);
        QVERIFY(img.save(src));

        ArtStore store(tmp.filePath(QStringLiteral("art")));
        QString out, err;
        QVERIFY2(store.importImage(src, QStringLiteral("test"), &out, &err), qPrintable(err));

        const QImage got(out);
        QCOMPARE(got.width(),  1024);
        QCOMPARE(got.height(), 1024);
        QCOMPARE(store.localPathForKey(QStringLiteral("test")), out);
        QVERIFY(store.localPathForKey(QStringLiteral("missing")).isEmpty());
    }

    void importRejectsUnreadable() {
        QTemporaryDir tmp;
        ArtStore store(tmp.filePath(QStringLiteral("art")));
        QString out, err;
        QVERIFY(!store.importImage(tmp.filePath(QStringLiteral("nope.png")),
                                   QStringLiteral("x"), &out, &err));
        QVERIFY(!err.isEmpty());
    }
};

QTEST_MAIN(TestArtStore)
#include "test_art_store.moc"
