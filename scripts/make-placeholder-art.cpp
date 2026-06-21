// make-placeholder-art.cpp — generate a 1024x1024 placeholder Rich Presence art
// asset: a cyan-accented monogram on the Discord-dark background. Replaces the
// (uncommitted) Pillow script that produced the original osrs.png / code.png.
//
// Usage: make-placeholder-art <out.png> <MONOGRAM> [#accentHex]
//   make-placeholder-art terminal.png ">_"
//   make-placeholder-art youtube.png "YT" "#ff4444"
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QColor>
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
                                         : QColor(QStringLiteral("#22d3ee")); // cyan

    const int S = 1024;
    QImage img(S, S, QImage::Format_ARGB32);
    img.fill(QColor(QStringLiteral("#1e1f22")));        // Discord dark

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Rounded accent panel.
    QPainterPath panel;
    panel.addRoundedRect(96, 96, S - 192, S - 192, 96, 96);
    p.fillPath(panel, QColor(accent.red(), accent.green(), accent.blue(), 38));
    p.setPen(QPen(accent, 10));
    p.drawPath(panel);

    // Monogram.
    QFont f;
    f.setBold(true);
    f.setPixelSize(monogram.length() <= 2 ? 460 : 300);
    p.setFont(f);
    p.setPen(accent);
    p.drawText(QRect(0, 0, S, S), Qt::AlignCenter, monogram);
    p.end();

    if (!img.save(outPath, "PNG")) {
        std::fprintf(stderr, "failed to write %s\n", qPrintable(outPath));
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", qPrintable(outPath), S, S);
    return 0;
}
