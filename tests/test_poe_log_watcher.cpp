// test_poe_log_watcher.cpp — PoeLogWatcher: append, partial trailing line,
// and truncation-resets-offset, against a real temp file (per the design's
// testing requirements for this unit).
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include "PoeLogWatcher.h"

using namespace OmniPresence;

class TestPoeLogWatcher : public QObject {
    Q_OBJECT
private slots:
    // start() must seek to end-of-file: a backlog already on disk before the
    // watcher starts must never replay as presence history.
    void startSeeksToEndOfFileWithoutReplayingBacklog() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("LatestClient.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("2026/08/11 18:50:59 1 aaa [INFO Client 1] : You have entered Town.\n");
        f.close();

        PoeLogWatcher watcher;
        watcher.setLogPathForTest(path);
        QSignalSpy spy(&watcher, &PoeLogWatcher::lineRead);
        watcher.start();
        watcher.pollOnceForTest();
        QCOMPARE(spy.count(), 0);
    }

    void appendedLinesAreEmittedOneByOne() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("LatestClient.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        PoeLogWatcher watcher;
        watcher.setLogPathForTest(path);
        QSignalSpy spy(&watcher, &PoeLogWatcher::lineRead);
        watcher.start();
        watcher.pollOnceForTest();
        QCOMPARE(spy.count(), 0);

        QVERIFY(f.open(QIODevice::Append));
        f.write("2026/08/11 18:51:00 2 bbb [INFO Client 1] : You have entered Cartographer's Hideout.\n");
        f.close();
        watcher.pollOnceForTest();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains(QStringLiteral("Cartographer's Hideout")));

        QVERIFY(f.open(QIODevice::Append));
        f.write("2026/08/11 18:51:05 3 bbb [INFO Client 1] : You have entered Lioneye's Watch.\n");
        f.close();
        watcher.pollOnceForTest();
        QCOMPARE(spy.count(), 2);
        QVERIFY(spy.at(1).at(0).toString().contains(QStringLiteral("Lioneye's Watch")));
    }

    // A line written without its trailing newline yet must be withheld until
    // the newline arrives — never emitted as a truncated fragment.
    void partialTrailingLineIsBufferedUntilNewlineArrives() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("LatestClient.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        PoeLogWatcher watcher;
        watcher.setLogPathForTest(path);
        QSignalSpy spy(&watcher, &PoeLogWatcher::lineRead);
        watcher.start();
        watcher.pollOnceForTest();

        QVERIFY(f.open(QIODevice::Append));
        f.write("2026/08/11 18:52:00 3 ccc [INFO Client 1] : You have ent");   // no newline yet
        f.close();
        watcher.pollOnceForTest();
        QCOMPARE(spy.count(), 0);   // withheld until the newline arrives

        QVERIFY(f.open(QIODevice::Append));
        f.write("ered Lioneye's Watch.\n");
        f.close();
        watcher.pollOnceForTest();
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains(QStringLiteral("Lioneye's Watch")));
        // The completed line must be the whole thing, not just the second half.
        QVERIFY(spy.at(0).at(0).toString().contains(QStringLiteral("You have entered Lioneye's Watch")));
    }

    // A shrink (LatestClient.txt truncated fresh at every PoE relaunch, per
    // the design) must reset the stored offset to 0 and replay the new
    // (smaller) file's content from the start — not skip straight to its end.
    void truncationResetsOffsetAndReplaysNewContent() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("LatestClient.txt"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("2026/08/11 18:50:00 1 aaa [INFO Client 1] : You have entered Town.\n"
                "2026/08/11 18:50:05 2 aaa [INFO Client 1] : You have entered Cartographer's Hideout.\n");
        f.close();

        PoeLogWatcher watcher;
        watcher.setLogPathForTest(path);
        watcher.start();          // seeks to EOF of the pre-existing 2-line content
        watcher.pollOnceForTest();

        QSignalSpy spy(&watcher, &PoeLogWatcher::lineRead);

        // Simulate a relaunch: truncated to a fresh, smaller session.
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("2026/08/11 19:00:00 1 bbb ***** LOG FILE OPENING *****\n"
                "2026/08/11 19:00:01 2 bbb [INFO Client 1] : You have entered Lioneye's Watch.\n");
        f.close();
        watcher.pollOnceForTest();

        QCOMPARE(spy.count(), 2);
        QVERIFY(spy.at(0).at(0).toString().contains(QStringLiteral("LOG FILE OPENING")));
        QVERIFY(spy.at(1).at(0).toString().contains(QStringLiteral("Lioneye's Watch")));
    }

    // A restart against a log that does not exist yet: emits logNotFound
    // rather than crashing or silently doing nothing forever, and picks the
    // log up once it appears (simulating the game launching after OmniPresence).
    void missingLogEmitsNotFoundAndRecoversOnceItAppears() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("LatestClient.txt"));   // does not exist

        PoeLogWatcher watcher;
        watcher.setLogPathForTest(path);
        QSignalSpy notFoundSpy(&watcher, &PoeLogWatcher::logNotFound);
        QSignalSpy openedSpy(&watcher, &PoeLogWatcher::logOpened);
        watcher.start();
        QVERIFY(notFoundSpy.count() >= 1);
        QCOMPARE(openedSpy.count(), 0);

        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        watcher.pollOnceForTest();
        QCOMPARE(openedSpy.count(), 1);
    }
};

QTEST_MAIN(TestPoeLogWatcher)
#include "test_poe_log_watcher.moc"
