// ArtStore.h — Local store for Rich Presence art the user adds via "Add photo".
//
// Holds normalized 1024x1024 PNGs under %APPDATA%/OmniPresence/art so the in-app
// preview can show real artwork immediately, before/independent of the manual
// upload to the Discord developer portal. Keys are lowercase slugs.
#pragma once

#include <QString>
#include <QColor>

namespace OmniPresence {

class ArtStore {
public:
    ArtStore();                         ///< Uses %APPDATA%/OmniPresence/art.
    explicit ArtStore(QString dir);     ///< Explicit directory (tests).

    /// Lowercase, [a-z0-9_]-only key derived from a filename or label.
    static QString slugify(const QString& raw);

    /// Render a 1024x1024 art tile to outPath in the polished house style:
    /// dark vertical gradient, rounded accent-bordered panel, a large accent
    /// monogram, and (when label is non-empty) a lighter letter-spaced caption
    /// band below it — matching the bundled code.png / osrs.png look.
    /// Creates parent dirs. Returns false (and sets *err) on write failure.
    static bool renderMonogram(const QString& outPath, const QString& monogram,
                               const QColor& accent, const QString& label,
                               QString* err);

    QString artDir() const { return m_dir; }

    /// Local PNG path for an existing key, or "" if not stored locally.
    QString localPathForKey(const QString& key) const;

    /// Copy + normalize an image to <artDir>/<key>.png (1024x1024 PNG).
    /// Returns false (and sets *err) on failure.
    bool importImage(const QString& srcPath, const QString& key,
                     QString* outPath, QString* err) const;

    /// Rewrite srcPath as a 1024x1024 transparent-padded PNG at outPath.
    ///
    /// Discord rejects Rich Presence art that is not square and at least
    /// 512x512 — such an image resolves to a white box with a question mark
    /// rather than failing loudly. Anything published as external art must go
    /// through here first.
    ///
    /// Unlike importImage()'s cover+centre-crop, this "contains" the source:
    /// the longest edge is scaled to 1024 and the result centred on a
    /// transparent canvas, so a non-square logo keeps its edges.
    /// Returns false (and sets *err) on failure.
    static bool normalizeSquarePng(const QString& srcPath, const QString& outPath,
                                   QString* err);

private:
    QString m_dir;
};

} // namespace OmniPresence
