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

    // Regression: the catbox upload path used to POST the raw dragged file, so a
    // non-square/small image (e.g. a 329x360 logo) reached Discord unchanged and
    // rendered as a white box with a "?". Every uploaded image must come out
    // square and >=512px, whatever went in.
    void normalizeSquarePngAlwaysProducesValidDiscordArt() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        struct Case { int w, h; const char* name; };
        const QList<Case> cases = {
            {329, 360, "portrait_small"},   // the senpai icon that failed
            {650, 665, "portrait_large"},   // the sixpaths icon that failed
            {512, 512, "already_square"},
            {2000, 100, "extreme_wide"},
            {40,  40,  "tiny"},
        };

        for (const Case& c : cases) {
            const QString src = tmp.filePath(QString::fromLatin1(c.name) + QStringLiteral(".png"));
            QImage in(c.w, c.h, QImage::Format_RGB32);
            in.fill(Qt::magenta);
            QVERIFY(in.save(src));

            const QString out = tmp.filePath(QString::fromLatin1(c.name) + QStringLiteral("_out.png"));
            QString err;
            QVERIFY2(ArtStore::normalizeSquarePng(src, out, &err), qPrintable(err));

            const QImage got(out);
            QVERIFY2(!got.isNull(), c.name);
            QCOMPARE(got.width(),  1024);
            QCOMPARE(got.height(), 1024);
            QVERIFY2(got.width() >= 512 && got.width() == got.height(), c.name);
        }
    }

    // Padding, not cropping: a wide source must keep its full width, with the
    // spare vertical space left transparent.
    void normalizeSquarePngPadsRatherThanCrops() {
        QTemporaryDir tmp;
        const QString src = tmp.filePath(QStringLiteral("wide.png"));
        QImage in(800, 200, QImage::Format_ARGB32);
        in.fill(Qt::red);
        QVERIFY(in.save(src));

        const QString out = tmp.filePath(QStringLiteral("wide_out.png"));
        QString err;
        QVERIFY2(ArtStore::normalizeSquarePng(src, out, &err), qPrintable(err));

        const QImage got(out);
        // Full width preserved: left/right edges at the vertical centre are red.
        QCOMPARE(got.pixelColor(2, 512).red(), 255);
        QCOMPARE(got.pixelColor(1021, 512).red(), 255);
        // Top and bottom are transparent padding, not cropped-away content.
        QCOMPARE(got.pixelColor(512, 2).alpha(), 0);
        QCOMPARE(got.pixelColor(512, 1021).alpha(), 0);
    }

    void normalizeSquarePngRejectsUnreadable() {
        QTemporaryDir tmp;
        QString err;
        QVERIFY(!ArtStore::normalizeSquarePng(tmp.filePath(QStringLiteral("nope.png")),
                                              tmp.filePath(QStringLiteral("out.png")), &err));
        QVERIFY(!err.isEmpty());
    }

    void importRejectsUnreadable() {
        QTemporaryDir tmp;
        ArtStore store(tmp.filePath(QStringLiteral("art")));
        QString out, err;
        QVERIFY(!store.importImage(tmp.filePath(QStringLiteral("nope.png")),
                                   QStringLiteral("x"), &out, &err));
        QVERIFY(!err.isEmpty());
    }

    void renderMonogramWrites1024Png() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString out = tmp.filePath(QStringLiteral("sub/mono.png"));  // nested → tests mkpath
        QString err;
        QVERIFY2(ArtStore::renderMonogram(out, QStringLiteral("YT"),
                 QColor(QStringLiteral("#ff4444")), QString(), &err), qPrintable(err));
        const QImage got(out);
        QVERIFY(!got.isNull());
        QCOMPARE(got.width(),  1024);
        QCOMPARE(got.height(), 1024);
    }

    void renderMonogramWithLabelWrites1024Png() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString out = tmp.filePath(QStringLiteral("mono_label.png"));
        QString err;
        // Polished tile: big monogram + label band below (the osrs.png look).
        QVERIFY2(ArtStore::renderMonogram(out, QStringLiteral("OSRS"),
                 QColor(QStringLiteral("#22d3ee")), QStringLiteral("RuneScape"), &err),
                 qPrintable(err));
        const QImage got(out);
        QVERIFY(!got.isNull());
        QCOMPARE(got.width(),  1024);
        QCOMPARE(got.height(), 1024);
    }
};

QTEST_MAIN(TestArtStore)
#include "test_art_store.moc"
