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
#include "system_activity.h"
#include "vault_controller.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include "settings_files.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_autolock.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.25: Initial release (D-040)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

// Stands in for the system: it sleeps when told, and is inactive for as
// many seconds as it is told (-1: it does not say).
class StandIn : public SystemActivity
{
public:
    qint64 seconds = 0;
    void askIdleTime() override { emit idleTime(seconds); }
    void sleep() { emit sleeping(); }
};

} // namespace

// Autolock when (D-040), and Settings > Security's rows.
class TestAutolock : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void zero_minutes_from_before_becomes_the_idle_box_unticked();
    void the_system_sleeping_locks_only_when_chosen();
    void the_system_inactive_locks_after_its_minutes();
    void closing_the_main_window_locks_only_when_chosen();
    void the_security_settings_show_keep_and_restore_autolock();

private:
    // A controller with an unlocked vault and the stand-in in place.
    std::unique_ptr<VaultController> unlocked(StandIn*& standIn);
    QTemporaryDir* folder = nullptr;
};

void TestAutolock::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_autolock"));
    applyStyle();
}

void TestAutolock::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestAutolock::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
}

void TestAutolock::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
    delete folder;
    folder = nullptr;
}

std::unique_ptr<VaultController> TestAutolock::unlocked(StandIn*& standIn)
{
    auto controller = std::make_unique<VaultController>();
    auto owned = std::make_unique<StandIn>();
    standIn = owned.get();
    controller->setSystemActivityForTests(std::move(owned));
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("lock.chronovault"))),
                            QStringLiteral("pass"), QStringLiteral("pass"));
    if (!QTest::qWaitFor([&controller]() { return controller->state() == State::Unlocked; }, 10000)) {
        return nullptr;
    }
    return controller;
}

void TestAutolock::zero_minutes_from_before_becomes_the_idle_box_unticked()
{
    // Before 0.0.25, 0 minutes meant "never".
    SettingsFile().setValue(QStringLiteral("security/autoLockMinutes"), 0);
    {
        VaultController controller;
        QVERIFY(!controller.idleLockOn());
        QCOMPARE(controller.autoLockMinutes(), 5);
    }
    // A number of minutes stays, ticked.
    SettingsFile().clear();
    VaultsFile().clear();
    SettingsFile().setValue(QStringLiteral("security/autoLockMinutes"), 12);
    VaultController controller;
    QVERIFY(controller.idleLockOn());
    QCOMPARE(controller.autoLockMinutes(), 12);
    // The other three are off by default.
    QVERIFY(!controller.lockWhenClosed());
    QVERIFY(!controller.lockOnSleep());
    QVERIFY(!controller.systemIdleLockOn());
}

void TestAutolock::the_system_sleeping_locks_only_when_chosen()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    standIn->sleep();
    QCOMPARE(controller->state(), State::Unlocked);
    controller->setLockOnSleep(true);
    QVERIFY(SettingsFile().value(QStringLiteral("security/lockOnSleep")).toBool());
    standIn->sleep();
    QCOMPARE(controller->state(), State::Locked);
}

void TestAutolock::the_system_inactive_locks_after_its_minutes()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    QVERIFY(controller->systemIdleAvailable());
    controller->setSystemIdleLockOn(true);
    controller->setSystemIdleMinutes(3);
    // Just unlocked (by finger, no input the system sees): the system's
    // earlier inactive time does not count.
    standIn->seconds = 600;
    controller->checkSystemIdle();
    QCOMPARE(controller->state(), State::Unlocked);
    controller->setUnlockedSecondsAgoForTests(3600);
    // 2 minutes 59 seconds: not yet; off, not at all.
    standIn->seconds = 179;
    controller->checkSystemIdle();
    QCOMPARE(controller->state(), State::Unlocked);
    controller->setSystemIdleLockOn(false);
    standIn->seconds = 600;
    controller->checkSystemIdle();
    QCOMPARE(controller->state(), State::Unlocked);
    // On, and 3 minutes reached: locked.
    controller->setSystemIdleLockOn(true);
    standIn->seconds = 180;
    controller->checkSystemIdle();
    QCOMPARE(controller->state(), State::Locked);
    // A desktop that does not tell: not available.
    standIn->seconds = -1;
    controller->checkSystemIdle();
    QVERIFY(!controller->systemIdleAvailable());
}

void TestAutolock::closing_the_main_window_locks_only_when_chosen()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    controller->mainWindowClosed();
    QCOMPARE(controller->state(), State::Unlocked);
    controller->setLockWhenClosed(true);
    controller->mainWindowClosed();
    QCOMPARE(controller->state(), State::Locked);
}

void TestAutolock::the_security_settings_show_keep_and_restore_autolock()
{
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* window = engine.rootObjects().constFirst();
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(controller && settings);
    QObject* topics = settings->findChild<QObject*>(QStringLiteral("topics"));
    QObject* ok = settings->findChild<QObject*>(QStringLiteral("okButton"));
    QObject* restore = settings->findChild<QObject*>(QStringLiteral("restoreDefaultsButton"));
    QObject* change = settings->findChild<QObject*>(QStringLiteral("changeMasterPasswordButton"));
    QVERIFY(topics && ok && restore && change);
    for (const char* name : {"lockWhenClosedSwitch", "lockOnSleepSwitch", "idleLockBox", "autoLockBox",
                             "systemIdleLockBox", "systemIdleBox"}) {
        QVERIFY2(settings->findChild<QObject*>(QString::fromLatin1(name)), name);
    }
    // No vault unlocked: nothing to change the master password of.
    QVERIFY(!change->property("enabled").toBool());

    // OK keeps every choice.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    topics->setProperty("currentIndex", 1);
    settings->setProperty("lockWhenClosed", true);
    settings->setProperty("lockOnSleep", true);
    settings->setProperty("idleLockOn", false);
    settings->setProperty("systemIdleLockOn", true);
    settings->setProperty("systemIdleMinutes", 25);
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QVERIFY(controller->lockWhenClosed());
    QVERIFY(controller->lockOnSleep());
    QVERIFY(!controller->idleLockOn());
    QVERIFY(controller->systemIdleLockOn());
    QCOMPARE(controller->systemIdleMinutes(), 25);

    // Restore defaults on Security puts them back, kept with OK.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    topics->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(restore, "clicked"));
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QVERIFY(!controller->lockWhenClosed());
    QVERIFY(!controller->lockOnSleep());
    QVERIFY(controller->idleLockOn());
    QCOMPARE(controller->autoLockMinutes(), 5);
    QVERIFY(!controller->systemIdleLockOn());
    QCOMPARE(controller->systemIdleMinutes(), 10);

    // With a vault unlocked, Change opens the master password window.
    QTemporaryDir vaultFolder;
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(vaultFolder.filePath(QStringLiteral("change.chronovault"))),
                            QStringLiteral("pass"), QStringLiteral("pass"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QTRY_VERIFY(change->property("enabled").toBool());
    QObject* masterPassword = window->findChild<QObject*>(QStringLiteral("masterPasswordWindow"));
    QVERIFY(masterPassword);
    QVERIFY(QMetaObject::invokeMethod(change, "clicked"));
    QTRY_VERIFY(masterPassword->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(masterPassword, "close"));
    controller->lock();

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

QTEST_MAIN(TestAutolock)
#include "test_autolock.moc"
