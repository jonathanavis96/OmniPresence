// ArtStore.h — Local store for Rich Presence art the user adds via "Add photo".
//
// Holds normalized 1024x1024 PNGs under %APPDATA%/OmniPresence/art so the in-app
// preview can show real artwork immediately, before/independent of the manual
// upload to the Discord developer portal. Keys are lowercase slugs.
#pragma once

#include <QString>

namespace OmniPresence {

class ArtStore {
public:
    ArtStore();                         ///< Uses %APPDATA%/OmniPresence/art.
    explicit ArtStore(QString dir);     ///< Explicit directory (tests).

    /// Lowercase, [a-z0-9_]-only key derived from a filename or label.
    static QString slugify(const QString& raw);

    QString artDir() const { return m_dir; }

    /// Local PNG path for an existing key, or "" if not stored locally.
    QString localPathForKey(const QString& key) const;

    /// Copy + normalize an image to <artDir>/<key>.png (1024x1024 PNG).
    /// Returns false (and sets *err) on failure.
    bool importImage(const QString& srcPath, const QString& key,
                     QString* outPath, QString* err) const;

private:
    QString m_dir;
};

} // namespace OmniPresence
