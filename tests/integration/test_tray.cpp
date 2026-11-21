// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg <RobVandenberg64@gmail.com>
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License version 3 as
// published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
// General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "app_setup.h"
#include "autostart.h"
#include "system_tray.h"
#include "vault_controller.h"

#include <QAction>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_tray.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release; Quit is seen asking the program to end

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {
const QString testRunKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Chrono Vault Tests\\Run");
}

// The system tray and the General settings (D-019).
class TestTray : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void closing_the_window_hides_it_when_there_is_a_tray();
    void the_tray_menu_shows_locks_and_quits();
    void start_at_startup_writes_and_removes_its_entry();
    void start_minimized_waits_in_the_tray_or_starts_minimized();
    void the_general_settings_are_remembered();

private:
    struct Loaded
    {
        std::unique_ptr<QQmlApplicationEngine> engine;
        QWindow* window = nullptr;
        VaultController* controller = nullptr;
    };
    Loaded load();
};

void TestTray::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_tray"));
    QSettings().clear();
    applyStyle();
    autostart::setWindowsKeyForTests(testRunKey);
}

void TestTray::cleanupTestCase()
{
    autostart::setEnabled(false, QString());
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
#ifdef Q_OS_WIN
    QSettings(QStringLiteral("HKEY_CURRENT_USER\\Software"), QSettings::NativeFormat)
        .remove(QStringLiteral("Chrono Vault Tests"));
#endif
}

TestTray::Loaded TestTray::load()
{
    Loaded loaded;
    loaded.engine = std::make_unique<QQmlApplicationEngine>();
    loaded.engine->loadFromModule("ChronoVault", "Main");
    if (!loaded.engine->rootObjects().isEmpty()) {
        loaded.window = qobject_cast<QWindow*>(loaded.engine->rootObjects().constFirst());
    }
    loaded.controller = loaded.engine->singletonInstance<VaultController*>("ChronoVault", "VaultController");
    return loaded;
}

void TestTray::closing_the_window_hides_it_when_there_is_a_tray()
{
    Loaded loaded = load();
    QVERIFY(loaded.window && loaded.controller);
    loaded.controller->setTrayAvailable(true);
    QVERIFY(loaded.controller->keepInTray());
    showMainWindow(loaded.window, false, true);
    QVERIFY(loaded.window->isVisible());
    QSignalSpy quit(loaded.engine.get(), &QQmlEngine::quit);

    loaded.window->close();
    QTRY_VERIFY(!loaded.window->isVisible());
    QCOMPARE(quit.count(), 0); // hidden in the tray, still running
}

void TestTray::the_tray_menu_shows_locks_and_quits()
{
    QTemporaryDir folder;
    Loaded loaded = load();
    QVERIFY(loaded.window && loaded.controller);
    loaded.controller->setTrayAvailable(true);
    loaded.controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    SystemTray tray(loaded.controller, loaded.window);
    QVERIFY(!tray.lockAction()->isEnabled());

    tray.showWindow();
    QVERIFY(loaded.window->isVisible());

    loaded.controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("tray.chronovault"))),
                                   QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(loaded.controller->state(), State::Unlocked);
    QVERIFY(tray.lockAction()->isEnabled());
    tray.lockVault();
    QCOMPARE(loaded.controller->state(), State::Locked);
    QVERIFY(!tray.lockAction()->isEnabled());

    // Quit ends the program (it does not hide to the tray).
    QSignalSpy quit(loaded.engine.get(), &QQmlEngine::quit);
    tray.quit();
    QTRY_COMPARE(quit.count(), 1);
    QVERIFY(!loaded.window->isVisible());
}

void TestTray::start_at_startup_writes_and_removes_its_entry()
{
    const QString program = QStringLiteral("/opt/chrono-vault/bin/chrono-vault");
    QVERIFY(!autostart::isEnabled());
    QVERIFY(autostart::setEnabled(true, program));
    QVERIFY(autostart::isEnabled());
#ifndef Q_OS_WIN
    QFile entry(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                QStringLiteral("/autostart/chrono-vault.desktop"));
    QVERIFY(entry.open(QIODevice::ReadOnly));
    QVERIFY(entry.readAll().contains("Exec=\"/opt/chrono-vault/bin/chrono-vault\""));
#endif
    QVERIFY(autostart::setEnabled(false, program));
    QVERIFY(!autostart::isEnabled());

#ifndef Q_OS_WIN
    // Characters the desktop entry format reads as its own are escaped.
    QVERIFY(autostart::setEnabled(true, QStringLiteral("/opt/100% \"odd\" $dir/chrono-vault")));
    QFile odd(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
              QStringLiteral("/autostart/chrono-vault.desktop"));
    QVERIFY(odd.open(QIODevice::ReadOnly));
    QVERIFY(odd.readAll().contains("Exec=\"/opt/100%% \\\"odd\\\" \\$dir/chrono-vault\""));
    odd.close();
    QVERIFY(autostart::setEnabled(false, program));
#endif
}

void TestTray::start_minimized_waits_in_the_tray_or_starts_minimized()
{
    Loaded withTray = load();
    showMainWindow(withTray.window, true, true);
    QVERIFY(!withTray.window->isVisible());

    Loaded withoutTray = load();
    showMainWindow(withoutTray.window, true, false);
    QVERIFY(withoutTray.window->isVisible());
    QVERIFY(withoutTray.window->windowStates() & Qt::WindowMinimized);
}

void TestTray::the_general_settings_are_remembered()
{
    {
        VaultController controller;
        controller.setKeepInTray(false);
        controller.setStartMinimized(true);
    }
    VaultController again;
    QVERIFY(!again.keepInTray());
    QVERIFY(again.startMinimized());
    again.setKeepInTray(true);
    again.setStartMinimized(false);
}

QTEST_MAIN(TestTray)
#include "test_tray.moc"
