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
#include "updater.h"
#include "vault_controller.h"

#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include "settings_files.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_tray.cpp 0.5.82

// --- Version history ----------------------------------------------------
// v0.5.82: Esc cancels the editor (D-062)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.27: Keep running in the tray is the last of the three (D-042)
// v0.0.10: The tray and start settings survive a new start; their old names carried over
// v0.0.6: The settings window keeps changes only with OK, also with Enter; Restore defaults; a failed
//         start at startup is told (D-022)
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
    void the_tray_and_start_settings_survive_a_new_start();
    void esc_closes_the_main_window_only_when_ticked_and_not_editing();
    void the_settings_window_keeps_changes_only_with_ok();
    void enter_is_ok_and_a_failed_start_at_startup_is_told();
    void keep_running_in_the_tray_is_the_last_of_the_three();

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
    SettingsFile().clear();
    VaultsFile().clear();
    applyStyle();
    autostart::setWindowsKeyForTests(testRunKey);
}

void TestTray::cleanupTestCase()
{
    // The file a failed run of the start-at-startup test may leave in the
    // test folder's place of the autostart folder.
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                  QStringLiteral("/autostart"));
    autostart::setEnabled(false, QString());
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
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

void TestTray::the_settings_window_keeps_changes_only_with_ok()
{
    Loaded loaded = load();
    QVERIFY(loaded.window && loaded.controller);
    loaded.controller->setTrayAvailable(true);
    auto* updater = loaded.engine->singletonInstance<Updater*>("ChronoVault", "Updater");
    QObject* settings = loaded.window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(updater && settings);
    QObject* topics = settings->findChild<QObject*>(QStringLiteral("topics"));
    QObject* ok = settings->findChild<QObject*>(QStringLiteral("okButton"));
    QObject* cancel = settings->findChild<QObject*>(QStringLiteral("cancelButton"));
    QObject* restore = settings->findChild<QObject*>(QStringLiteral("restoreDefaultsButton"));
    QObject* minimizedBox = settings->findChild<QObject*>(QStringLiteral("startMinimizedBox"));
    QVERIFY(topics && ok && cancel && restore && minimizedBox);
    const int lockBefore = loaded.controller->autoLockMinutes();

    // Cancel throws the changes away.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    settings->setProperty("startMinimized", true);
    settings->setProperty("autoLockMinutes", 17);
    QVERIFY(minimizedBox->property("checked").toBool());
    QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QVERIFY(!loaded.controller->startMinimized());
    QCOMPARE(loaded.controller->autoLockMinutes(), lockBefore);

    // Opened again, it shows the settings as they are; OK keeps every
    // topic's changes.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    QVERIFY(!settings->property("startMinimized").toBool());
    settings->setProperty("startMinimized", true);
    settings->setProperty("autoLockMinutes", 17);
    settings->setProperty("checkFrequency", 2);
    settings->setProperty("escapeExits", true);
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QVERIFY(loaded.controller->startMinimized());
    QCOMPARE(loaded.controller->autoLockMinutes(), 17);
    QCOMPARE(updater->frequency(), 2);
    QVERIFY(loaded.controller->escapeExits());

    // Restore defaults puts back the topic shown (Security) and no other,
    // kept with OK.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    topics->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(restore, "clicked"));
    QCOMPARE(settings->property("autoLockMinutes").toInt(), 5);
    QVERIFY(settings->property("startMinimized").toBool());
    QCOMPARE(settings->property("checkFrequency").toInt(), 2);
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QCOMPARE(loaded.controller->autoLockMinutes(), 5);
    QVERIFY(loaded.controller->startMinimized());

    // Restore defaults on General.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    topics->setProperty("currentIndex", 0);
    QVERIFY(QMetaObject::invokeMethod(restore, "clicked"));
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QVERIFY(!loaded.controller->startMinimized());
    QVERIFY(loaded.controller->keepInTray());
    QCOMPARE(updater->frequency(), 0);
    QVERIFY(!loaded.controller->escapeExits());

    // Without a tray its setting cannot be changed here, so Restore defaults
    // leaves it as it is.
    loaded.controller->setKeepInTray(false);
    loaded.controller->setTrayAvailable(false);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QVERIFY(QMetaObject::invokeMethod(restore, "clicked"));
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QVERIFY(!loaded.controller->keepInTray());
    loaded.controller->setKeepInTray(true);
}

void TestTray::enter_is_ok_and_a_failed_start_at_startup_is_told()
{
    Loaded loaded = load();
    QVERIFY(loaded.window && loaded.controller);
    QObject* settings = loaded.window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    auto* settingsWindow = qobject_cast<QWindow*>(settings);
    QVERIFY(settingsWindow);

    // Enter keeps the changes, as OK does.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settingsWindow->isVisible());
    settings->setProperty("clipboardClearSeconds", 45);
    settingsWindow->requestActivate();
    QTRY_VERIFY(settingsWindow->isActive());
    QTest::keyClick(settingsWindow, Qt::Key_Return);
    QTRY_VERIFY(!settingsWindow->isVisible());
    QCOMPARE(loaded.controller->clipboardClearSeconds(), 45);
    loaded.controller->setClipboardClearSeconds(30);

    // Enter on a button that has the keyboard focus presses that button:
    // on Cancel, nothing is kept.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settingsWindow->isVisible());
    settings->setProperty("clipboardClearSeconds", 55);
    settingsWindow->requestActivate();
    QTRY_VERIFY(settingsWindow->isActive());
    QObject* cancel = settings->findChild<QObject*>(QStringLiteral("cancelButton"));
    QVERIFY(cancel && QMetaObject::invokeMethod(cancel, "forceActiveFocus"));
    QTest::keyClick(settingsWindow, Qt::Key_Return);
    QTRY_VERIFY(!settingsWindow->isVisible());
    QCOMPARE(loaded.controller->clipboardClearSeconds(), 30);

#ifndef Q_OS_WIN
    // The autostart folder cannot be made (a file is in its place): the
    // window stays open, says so, and shows the setting as it really is.
    const QString blocker = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                            QStringLiteral("/autostart");
    QVERIFY(blocker.contains(QStringLiteral(".qttest"))); // the test folder, never the person's own
    QDir(blocker).removeRecursively();
    QFile::remove(blocker);
    QVERIFY(QDir().mkpath(QFileInfo(blocker).absolutePath()));
    QFile file(blocker);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settingsWindow->isVisible());
    settings->setProperty("startAtStartup", true);
    settings->setProperty("autoLockMinutes", 9);
    settingsWindow->requestActivate();
    QTRY_VERIFY(settingsWindow->isActive());
    QTest::keyClick(settingsWindow, Qt::Key_Return);
    QVERIFY(settingsWindow->isVisible());
    QVERIFY(!settings->property("problem").toString().isEmpty());
    QVERIFY(!settings->property("startAtStartup").toBool());
    QCOMPARE(loaded.controller->autoLockMinutes(), 9);
    QVERIFY(QFile::remove(blocker));
    loaded.controller->setAutoLockMinutes(5);
    settingsWindow->close();
    QTRY_VERIFY(!settingsWindow->isVisible());
    QVERIFY(settings->property("problem").toString().isEmpty());
#endif
}

void TestTray::the_tray_and_start_settings_survive_a_new_start()
{
    // Written, then read back from the file itself, as the next start does.
    {
        VaultController controller;
        controller.setKeepInTray(false);
        controller.setStartMinimized(true);
    }
    SettingsFile().sync();
    {
        QSettings fromFile(SettingsFile().fileName(), SettingsFile().format());
        QCOMPARE(fromFile.value(QStringLiteral("tray/keepRunning")).toBool(), false);
        QCOMPARE(fromFile.value(QStringLiteral("start/minimized")).toBool(), true);
        for (const QString& key : fromFile.allKeys()) {
            QVERIFY2(!key.startsWith(QStringLiteral("general/"), Qt::CaseInsensitive), qPrintable(key));
        }
    }

    VaultController again;
    again.setKeepInTray(true);
    again.setStartMinimized(false);
}

void TestTray::esc_closes_the_main_window_only_when_ticked_and_not_editing()
{
    QTemporaryDir folder;
    Loaded loaded = load();
    QVERIFY(loaded.window && loaded.controller);
    loaded.controller->setTrayAvailable(true);
    loaded.controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    QVERIFY(!loaded.controller->escapeExits()); // off by default (D-048)
    QSignalSpy quit(loaded.engine.get(), &QQmlEngine::quit);
    showMainWindow(loaded.window, false, true);
    QVERIFY(QTest::qWaitForWindowExposed(loaded.window));

    // Not ticked: Esc does nothing.
    QTest::keyClick(loaded.window, Qt::Key_Escape);
    QVERIFY(loaded.window->isVisible());

    // Ticked: an open menu takes its Esc first, and closes.
    loaded.controller->setEscapeExits(true);
    QObject* fileMenu = loaded.window->findChild<QObject*>(QStringLiteral("fileMenu"));
    QVERIFY(fileMenu);
    QVERIFY(QMetaObject::invokeMethod(fileMenu, "open"));
    QTRY_VERIFY(fileMenu->property("opened").toBool());
    QTest::keyClick(loaded.window, Qt::Key_Escape);
    QTRY_VERIFY(!fileMenu->property("visible").toBool());
    QVERIFY(loaded.window->isVisible());

    // Ticked, with the tray: to the tray, still running.
    QTest::keyClick(loaded.window, Qt::Key_Escape);
    QTRY_VERIFY(!loaded.window->isVisible());
    QCOMPARE(quit.count(), 0);

    // In edit mode Esc cancels the editor, and only that (D-062).
    showMainWindow(loaded.window, false, true);
    QVERIFY(QTest::qWaitForWindowExposed(loaded.window));
    loaded.controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("esc.chronovault"))),
                                   QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(loaded.controller->state(), State::Unlocked);
    QObject* page = loaded.window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QVERIFY(page);
    QTRY_VERIFY(page->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(page, "newLogin"));
    QTRY_VERIFY(page->property("editing").toBool());
    QTest::keyClick(loaded.window, Qt::Key_Escape);
    QVERIFY(loaded.window->isVisible());
    QTRY_VERIFY(!page->property("editing").toBool());

    // Out of edit mode, without the tray: the program ends.
    QObject* editor = loaded.window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QVERIFY(editor);
    editor->setProperty("itemId", QString());
    QTRY_VERIFY(!page->property("editing").toBool());
    loaded.controller->setKeepInTray(false);
    QTest::keyClick(loaded.window, Qt::Key_Escape);
    QTRY_COMPARE(quit.count(), 1);
    loaded.controller->setKeepInTray(true);
    loaded.controller->setEscapeExits(false);
}

void TestTray::keep_running_in_the_tray_is_the_last_of_the_three()
{
    Loaded loaded = load();
    QVERIFY(loaded.window);
    QObject* settings = loaded.window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    const auto top = [settings](const char* name) {
        auto* box = settings->findChild<QQuickItem*>(QString::fromLatin1(name));
        return box ? box->mapToScene(QPointF(0, 0)).y() : -1.0;
    };
    QTRY_VERIFY(top("startAtStartupBox") > 0);
    QVERIFY(top("startAtStartupBox") < top("startMinimizedBox"));
    QVERIFY(top("startMinimizedBox") < top("keepInTrayBox"));
    QVERIFY(top("keepInTrayBox") < top("escapeExitsBox")); // D-048
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));
}

QTEST_MAIN(TestTray)
#include "test_tray.moc"
