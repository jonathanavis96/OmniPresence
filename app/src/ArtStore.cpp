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
                              const QColor& accent, QString* err) {
    const int S = 1024;
    QImage img(S, S, QImage::Format_ARGB32);
    img.fill(QColor(QStringLiteral("#1e1f22")));        // Discord dark

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QPainterPath panel;
    panel.addRoundedRect(96, 96, S - 192, S - 192, 96, 96);
    p.fillPath(panel, QColor(accent.red(), accent.green(), accent.blue(), 38));
    p.setPen(QPen(accent, 10));
    p.drawPath(panel);

    QFont f;
    f.setBold(true);
    f.setPixelSize(monogram.length() <= 2 ? 460 : 300);
    p.setFont(f);
    p.setPen(accent);
    p.drawText(QRect(0, 0, S, S), Qt::AlignCenter, monogram);
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
