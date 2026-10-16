// IntegrationContext.cpp — Storage and accessor implementation.
#include "IntegrationContext.h"

namespace OmniPresence {

// ── IntegrationPayload ────────────────────────────────────────────────────────

QString IntegrationPayload::field(const QString& key) const {
    const QJsonValue v = data[key];
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble());
    if (v.isBool())   return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

// ── IntegrationContext ────────────────────────────────────────────────────────

void IntegrationContext::update(const QString& source, const QJsonObject& data) {
    IntegrationPayload& p = m_payloads[source];
    p.source      = source;
    p.data        = data;
    p.receivedAt  = QDateTime::currentDateTimeUtc();
}

bool IntegrationContext::refresh(const QString& source) {
    const auto it = m_payloads.find(source);
    if (it == m_payloads.end()) return false;
    it->receivedAt = QDateTime::currentDateTimeUtc();
    return true;
}

const IntegrationPayload* IntegrationContext::get(const QString& source) const {
    const auto it = m_payloads.constFind(source);
    return (it != m_payloads.constEnd()) ? &(*it) : nullptr;
}

const IntegrationPayload* IntegrationContext::getFresh(const QString& source) const {
    const IntegrationPayload* p = get(source);
    return (p && p->isFresh()) ? p : nullptr;
}

void IntegrationContext::clear(const QString& source) {
    m_payloads.remove(source);
}

// ── Convenience accessors ─────────────────────────────────────────────────────

QString IntegrationContext::freshField(const QString& source, const QString& key) const {
    const IntegrationPayload* p = getFresh(source);
    return p ? p->field(key) : QString{};
}

QString IntegrationContext::browserDomain() const {
    return freshField(QStringLiteral("browser"), QStringLiteral("domain"));
}
QString IntegrationContext::browserCategory() const {
    return freshField(QStringLiteral("browser"), QStringLiteral("category"));
}

} // namespace OmniPresence
