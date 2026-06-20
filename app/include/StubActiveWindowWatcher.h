// StubActiveWindowWatcher.h — Cross-platform stub for non-Windows development.
// Emits synthetic WindowInfo events on a timer so UI/logic can be iterated
// without a real Windows environment.
#pragma once

#include "ActiveWindowWatcher.h"
#include <QTimer>

namespace OmniPresence {

class StubActiveWindowWatcher : public ActiveWindowWatcher {
    Q_OBJECT
public:
    explicit StubActiveWindowWatcher(QObject* parent = nullptr);

    void start()  override;
    void stop()   override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] WindowInfo currentForegroundWindow() const override;

private slots:
    void emitNextStub();

private:
    QTimer     m_timer;
    int        m_cycle{0};       ///< Rotates through synthetic scenarios.
    WindowInfo m_lastEmitted;    ///< Last synthetic window, for on-demand capture.
};

} // namespace OmniPresence
