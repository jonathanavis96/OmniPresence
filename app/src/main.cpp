// main.cpp — Application entry point.
// Sets up QGuiApplication + QQmlApplicationEngine + system tray + AppController.
//
// TODO (nice-to-have): single-instance guard via QLocalServer.
#include "AppController.h"

#include <QApplication>   // QApplication (not QGuiApplication) for QSystemTrayIcon support.
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QDebug>

int main(int argc, char* argv[]) {
    // High-DPI is on by default in Qt 6.
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("OmniPresence"));
    app.setApplicationName(QStringLiteral("OmniPresence"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    // Quit when the last window is closed only if we're not showing the tray.
    // The main window can be hidden; the tray keeps the process alive.
    app.setQuitOnLastWindowClosed(false);

    // ── AppController ─────────────────────────────────────────────────────────
    OmniPresence::AppController controller;

    // ── QML engine ────────────────────────────────────────────────────────────
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("AppController"), &controller);

    const QUrl mainQml(QStringLiteral("qrc:/OmniPresence/qml/Main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [](const QUrl& url) {
        qCritical() << "Failed to create QML object from" << url;
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);

    engine.load(mainQml);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "No root objects loaded from QML — exiting.";
        return 1;
    }

    // ── System tray ───────────────────────────────────────────────────────────
    QSystemTrayIcon tray;
    // Fallback to a built-in icon when the resource hasn't been added yet.
    QIcon trayIcon = QIcon::fromTheme(QStringLiteral("discord"));
    if (trayIcon.isNull()) {
        trayIcon = app.style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    tray.setIcon(trayIcon);
    tray.setToolTip(QStringLiteral("OmniPresence"));

    QMenu trayMenu;

    QAction* showAction = trayMenu.addAction(QStringLiteral("Show"));
    QObject::connect(showAction, &QAction::triggered, [&engine]() {
        for (QObject* root : engine.rootObjects()) {
            if (auto* window = qobject_cast<QWindow*>(root)) {
                window->show();
                window->raise();
                window->requestActivate();
            }
        }
    });

    trayMenu.addSeparator();

    QAction* pauseAction = trayMenu.addAction(QStringLiteral("Pause"));
    pauseAction->setCheckable(true);
    QObject::connect(pauseAction, &QAction::toggled, [&controller](bool checked) {
        checked ? controller.pause() : controller.resume();
    });
    QObject::connect(&controller, &OmniPresence::AppController::pauseChanged, [&]() {
        pauseAction->setChecked(controller.paused());
    });

    QAction* privacyAction = trayMenu.addAction(QStringLiteral("Private mode"));
    privacyAction->setCheckable(true);
    QObject::connect(privacyAction, &QAction::toggled, [&controller](bool checked) {
        controller.setPrivacyMode(checked);
    });
    QObject::connect(&controller, &OmniPresence::AppController::privacyModeChanged, [&]() {
        privacyAction->setChecked(controller.privacyMode());
    });

    trayMenu.addSeparator();
    trayMenu.addAction(QStringLiteral("Quit"), &app, &QCoreApplication::quit);

    tray.setContextMenu(&trayMenu);
    tray.show();

    // Double-click tray icon → show window.
    QObject::connect(&tray, &QSystemTrayIcon::activated,
                     [&](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) showAction->trigger();
    });

    // ── Start the controller ──────────────────────────────────────────────────
    controller.initialise();

    return app.exec();
}
