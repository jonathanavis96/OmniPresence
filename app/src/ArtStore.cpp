// ArtStore.cpp — see ArtStore.h.
#include "ArtStore.h"
#include <QDir>
#include <QImage>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>

namespace OmniPresence {

ArtStore::ArtStore()
    : m_dir(QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("art"))) {}

ArtStore::ArtStore(QString dir) : m_dir(std::move(dir)) {}

QString ArtStore::slugify(const QString& raw) {
    QString base = QFileInfo(raw).completeBaseName().toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    base.remove(QRegularExpression(QStringLiteral("^_+|_+$")));
    return base.isEmpty() ? QStringLiteral("art") : base;
}

QString ArtStore::localPathForKey(const QString& key) const {
    const QString p = QDir(m_dir).filePath(key + QStringLiteral(".png"));
    return QFileInfo::exists(p) ? p : QString();
}

bool ArtStore::importImage(const QString& srcPath, const QString& key,
                           QString* outPath, QString* err) const {
    QImage img(srcPath);
    if (img.isNull()) {
        if (err) *err = QStringLiteral("Unreadable image: %1").arg(srcPath);
        return false;
    }
    // Fill a 1024x1024 frame (cover), then centre-crop to square.
    const QImage scaled = img.scaled(1024, 1024, Qt::KeepAspectRatioByExpanding,
                                     Qt::SmoothTransformation);
    const int x = (scaled.width()  - 1024) / 2;
    const int y = (scaled.height() - 1024) / 2;
    const QImage square = scaled.copy(x, y, 1024, 1024);

    if (!QDir().mkpath(m_dir)) {
        if (err) *err = QStringLiteral("Cannot create art dir: %1").arg(m_dir);
        return false;
    }
    const QString p = QDir(m_dir).filePath(key + QStringLiteral(".png"));
    if (!square.save(p, "PNG")) {
        if (err) *err = QStringLiteral("Cannot write %1").arg(p);
        return false;
    }
    if (outPath) *outPath = p;
    return true;
}

bool ArtStore::renderMonogram(const QString& outPath, const QString& monogram,
                              const QColor& accent, const QString& label,
                              QString* err) {
    const int S = 1024;
    QImage img(S, S, QImage::Format_ARGB32);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Dark vertical gradient backdrop (matches the bundled code.png / osrs.png).
    QLinearGradient bg(0, 0, 0, S);
    bg.setColorAt(0.0, QColor(QStringLiteral("#15171b")));
    bg.setColorAt(1.0, QColor(QStringLiteral("#0d0f12")));
    p.fillRect(0, 0, S, S, bg);

    // Rounded panel: faint accent fill + accent border.
    QPainterPath panel;
    panel.addRoundedRect(64, 64, S - 128, S - 128, 88, 88);
    p.fillPath(panel, QColor(accent.red(), accent.green(), accent.blue(), 28));
    p.setPen(QPen(accent, 12));
    p.drawPath(panel);

    const bool hasLabel = !label.trimmed().isEmpty();

    // Monogram: large, accent. Shift up when a caption sits below it.
    QFont mf;
    mf.setBold(true);
    const int len = monogram.trimmed().length();
    mf.setPixelSize(len <= 2 ? 460 : (len == 3 ? 340 : 260));
    p.setFont(mf);
    p.setPen(accent);
    const int monoTop = hasLabel ? 150 : 0;
    const int monoH   = hasLabel ? 560 : S;
    p.drawText(QRect(0, monoTop, S, monoH), Qt::AlignCenter, monogram);

    // Caption band: lighter, bold, letter-spaced (the "RUNESCAPE" / "CODE" line).
    if (hasLabel) {
        QFont lf;
        lf.setBold(true);
        QString caption = label.trimmed().toUpper();
        // Scale the caption so long names still fit the panel width.
        int px = 132;
        for (; px >= 60; px -= 6) {
            lf.setPixelSize(px);
            lf.setLetterSpacing(QFont::AbsoluteSpacing, px * 0.12);
            if (QFontMetrics(lf).horizontalAdvance(caption) <= S - 220) break;
        }
        p.setFont(lf);
        p.setPen(QColor(QStringLiteral("#dbe1e8")));
        p.drawText(QRect(0, 700, S, 200), Qt::AlignCenter, caption);
    }
    p.end();

    const QString parent = QFileInfo(outPath).absolutePath();
    if (!parent.isEmpty()) QDir().mkpath(parent);
    if (!img.save(outPath, "PNG")) {
        if (err) *err = QStringLiteral("Cannot write %1").arg(outPath);
        return false;
    }
    return true;
}

} // namespace OmniPresence
