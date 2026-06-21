// make-placeholder-art.cpp — thin CLI over ArtStore::renderMonogram.
//   make-placeholder-art terminal.png ">_"
//   make-placeholder-art youtube.png "YT" "#ff4444"
#include <QGuiApplication>
#include <QColor>
#include "ArtStore.h"
#include <cstdio>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);   // needed for font rendering
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <out.png> <MONOGRAM> [#accentHex]\n", argv[0]);
        return 2;
    }
    const QString outPath  = QString::fromLocal8Bit(argv[1]);
    const QString monogram = QString::fromLocal8Bit(argv[2]);
    const QColor  accent   = (argc >= 4) ? QColor(QString::fromLocal8Bit(argv[3]))
                                         : QColor(QStringLiteral("#22d3ee"));
    QString err;
    if (!OmniPresence::ArtStore::renderMonogram(outPath, monogram, accent, &err)) {
        std::fprintf(stderr, "%s\n", qPrintable(err));
        return 1;
    }
    std::printf("wrote %s (1024x1024)\n", qPrintable(outPath));
    return 0;
}
