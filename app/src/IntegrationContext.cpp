// IntegrationContext.cpp — Storage and accessor implementation.
#include "IntegrationContext.h"

namespace OmniPresence {

// ── IntegrationPayload ────────────────────────────────────────────────────────

QString IntegrationPayload::field(const QString& key) const {
    const QJsonValue v = data[key];
    if (v.isString()) return v.toString();
    if (v.isDouble()) return QString::number(v.toDouble());
    return {};
}

// ── IntegrationContext ────────────────────────────────────────────────────────

void IntegrationContext::update(const QString& source, const QJsonObject& data) {
    IntegrationPayload& p = m_payloads[source];
    p.source      = source;
    p.data        = data;
    p.receivedAt  = QDateTime::currentDateTimeUtc();
}

const IntegrationPayload* IntegrationContext::get(const QString& source) const {
    const auto it = m_payloads.constFind(source);
    return (it != m_payloads.constEnd()) ? &(*it) : nullptr;
}

const IntegrationPayload* IntegrationContext::getFresh(const QString& source) const {
    const IntegrationPayload* p = get(source);
    return (p && p->isFresh()) ? p : nullptr;
}

void IntegrationContext::purgeStale() {
    for (auto it = m_payloads.begin(); it != m_payloads.end(); ) {
        it->isFresh() ? ++it : it = m_payloads.erase(it);
    }
}

void IntegrationContext::clear(const QString& source) {
    m_payloads.remove(source);
}

void IntegrationContext::clearAll() {
    m_payloads.clear();
}

// ── Convenience accessors ─────────────────────────────────────────────────────

#define FRESH_FIELD(source, key) \
    [&]() -> QString { \
        const auto* p = getFresh(QStringLiteral(source)); \
        return p ? p->field(QStringLiteral(key)) : QString{}; \
    }()

QString IntegrationContext::browserDomain()          const { return FRESH_FIELD("browser", "domain");           }
QString IntegrationContext::browserCategory()        const { return FRESH_FIELD("browser", "category");         }
QString IntegrationContext::browserTitle()           const { return FRESH_FIELD("browser", "safe_title");       }
QString IntegrationContext::terminalCwd()            const { return FRESH_FIELD("terminal", "cwd");             }
QString IntegrationContext::terminalRepo()           const { return FRESH_FIELD("terminal", "repo");            }
QString IntegrationContext::terminalCommandSummary() const { return FRESH_FIELD("terminal", "command_summary"); }
QString IntegrationContext::vscodeWorkspace()        const { return FRESH_FIELD("vscode", "workspace");         }
QString IntegrationContext::runeliteActivity()       const { return FRESH_FIELD("runelite", "activity");        }
QString IntegrationContext::runeliteTarget()         const { return FRESH_FIELD("runelite", "target");          }
QString IntegrationContext::runeliteSkill()          const { return FRESH_FIELD("runelite", "skill");           }
QString IntegrationContext::runeliteLocation()       const { return FRESH_FIELD("runelite", "location");        }
QString IntegrationContext::runeliteConfidence()     const { return FRESH_FIELD("runelite", "confidence");      }

#undef FRESH_FIELD

} // namespace OmniPresence
