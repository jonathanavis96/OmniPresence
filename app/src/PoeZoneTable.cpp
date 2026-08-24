// PoeZoneTable.cpp — see PoeZoneTable.h.
#include "PoeZoneTable.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

namespace OmniPresence {

void PoeZoneTable::loadFromJson(const QJsonObject& obj) {
    m_patterns.clear();
    m_exact.clear();
    m_ambiguous.clear();

    const QJsonArray patterns = obj.value(QStringLiteral("patterns")).toArray();
    for (const QJsonValue& v : patterns) {
        const QJsonObject po    = v.toObject();
        const QString pattern   = po.value(QStringLiteral("match")).toString();
        const QString category  = po.value(QStringLiteral("category")).toString();
        if (pattern.isEmpty() || category.isEmpty()) continue;
        const QRegularExpression re(pattern);
        if (!re.isValid()) continue;
        m_patterns.append(PatternEntry{re, category});
    }

    const QJsonObject exact = obj.value(QStringLiteral("exact")).toObject();
    for (auto it = exact.constBegin(); it != exact.constEnd(); ++it)
        m_exact.insert(it.key(), it.value().toString());

    const QJsonArray ambiguous = obj.value(QStringLiteral("ambiguous")).toArray();
    for (const QJsonValue& v : ambiguous)
        m_ambiguous.insert(v.toString());
}

bool PoeZoneTable::loadFromFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    loadFromJson(doc.object());
    return true;
}

QString PoeZoneTable::classify(const QString& zoneName) const {
    for (const PatternEntry& p : m_patterns) {
        if (p.regex.match(zoneName).hasMatch()) return p.category;
    }
    const auto it = m_exact.constFind(zoneName);
    if (it != m_exact.constEnd()) return it.value();
    return QStringLiteral("unknown");
}

bool PoeZoneTable::isAmbiguous(const QString& zoneName) const {
    return m_ambiguous.contains(zoneName);
}

} // namespace OmniPresence
