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

// --- Version ------------------------------------------------------------
// test_pin_unlock.cpp 0.8.128

// --- Version history ----------------------------------------------------
// v0.8.128: The open vault's PIN set without its password (D-076)
// v0.7.107: A PIN per vault; the lock as a screen lock (D-076)
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.4.71: The PIN page shows only the logo, its line, the field and the link
// v0.4.70: Initial release (D-059)

#include "app_setup.h"
#include "crypto.h"
#include "settings_files.h"
#include "vault_controller.h"

#include <QDataStream>
#include <functional>
#include <QDateTime>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

class TestPinUnlock : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void a_pin_opens_only_after_a_lock_and_never_after_a_restart();
    void wrong_pins_count_toward_the_same_waits();
    void a_pin_needs_the_typed_master_password_and_can_be_removed();
    void nothing_on_disk_opens_with_the_pin_alone();
    void the_reminder_asks_the_master_password_instead();
    void a_changed_master_password_keeps_the_pin();
    void the_old_pin_of_every_vault_is_removed();
    void a_pin_opens_only_its_own_vault();
    void a_lock_gives_everything_back_as_a_screen_lock();
    void another_vault_got_into_is_a_new_login();
    void the_settings_and_the_unlock_page_offer_the_pin();

private:
    // A vault made and left unlocked; its file.
    QString unlockedVault(VaultController& controller, const QString& name, const QString& password = QStringLiteral("right"),
                          bool save = false);
    static bool pinSetTo(VaultController& controller, const QString& vault, const QString& password, const QString& pin);
    static bool settled(VaultController& controller, State state);
    QTemporaryDir* folder = nullptr;
};

void TestPinUnlock::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_pin_unlock"));
    applyStyle();
}

void TestPinUnlock::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestPinUnlock::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
}

void TestPinUnlock::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
    delete folder;
    folder = nullptr;
}

QString TestPinUnlock::unlockedVault(VaultController& controller, const QString& name, const QString& password, bool save)
{
    controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    const QString file = folder->filePath(name + QStringLiteral(".chronovault"));
    controller.createVault(QUrl::fromLocalFile(file), password, password, QUrl(), save);
    const bool made = QTest::qWaitFor([&]() {
        return controller.vaultPath() == file && controller.state() == State::Unlocked && controller.isIdle();
    }, 10000);
    return made ? file : QString();
}

// Set PIN… as the vault's menu does it: its master password typed, then the PIN.
bool TestPinUnlock::pinSetTo(VaultController& controller, const QString& vault, const QString& password, const QString& pin)
{
    QSignalSpy proven(&controller, &VaultController::proven);
    controller.askFor(vault);
    controller.proveWithPassword(password);
    if (!proven.wait(10000)) {
        controller.stopAsking();
        return false;
    }
    controller.setPinFor(vault, pin);
    controller.stopAsking();
    return QTest::qWaitFor([&controller]() { return !controller.pinBusy(); }, 10000);
}

bool TestPinUnlock::settled(VaultController& controller, State state)
{
    return QTest::qWaitFor([&controller, state]() { return controller.state() == state; }, 10000);
}

void TestPinUnlock::a_pin_opens_only_after_a_lock_and_never_after_a_restart()
{
    QString file;
    {
        VaultController controller;
        file = unlockedVault(controller, QStringLiteral("pin"));
        QVERIFY(!file.isEmpty());
        QVERIFY(!controller.hasPin(file));
        QVERIFY(pinSetTo(controller, file, QStringLiteral("right"), QStringLiteral("12 ab")));
        QVERIFY(controller.hasPin(file));
        controller.lock();
        QVERIFY(controller.pinUnlockReady());
        controller.unlockWithPin(QStringLiteral("12 ab"));
        QVERIFY(settled(controller, State::Unlocked));
        // Close in the vault's own menu is no lock of the program: no PIN then.
        controller.closeOpenVault(file);
        QCOMPARE(controller.state(), State::Locked);
        QVERIFY(!controller.pinUnlockReady());
    }
    // After a start the master password comes first; then the PIN again.
    VaultController controller;
    controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    QVERIFY(settled(controller, State::Locked));
    QVERIFY(controller.hasPin(file));
    QVERIFY(!controller.pinUnlockReady());
    controller.unlockWithPin(QStringLiteral("12 ab"));
    QCOMPARE(controller.state(), State::Locked);
    controller.unlock(QStringLiteral("right"));
    QVERIFY(settled(controller, State::Unlocked));
    controller.lock();
    QVERIFY(controller.pinUnlockReady());
    controller.unlockWithPin(QStringLiteral("12 ab"));
    QVERIFY(settled(controller, State::Unlocked));
}

void TestPinUnlock::wrong_pins_count_toward_the_same_waits()
{
    VaultController controller;
    const QString file = unlockedVault(controller, QStringLiteral("waits"));
    QVERIFY(pinSetTo(controller, file, QStringLiteral("right"), QStringLiteral("2468")));
    controller.lock();
    // Two wrong PINs and a wrong master password: three, then 10 seconds.
    for (int attempt = 1; attempt <= 2; ++attempt) {
        controller.unlockWithPin(QStringLiteral("1357"));
        QVERIFY(settled(controller, State::Locked));
        QCOMPARE(controller.unlockWaitSeconds(), 0);
    }
    controller.unlock(QStringLiteral("wrong"));
    QVERIFY(QTest::qWaitFor([&controller]() { return controller.state() == State::Locked && controller.unlockWaitSeconds() > 0; }, 10000));
    QCOMPARE(controller.unlockWaitSeconds(), 10);
    // During the wait not even the right PIN is tried.
    controller.unlockWithPin(QStringLiteral("2468"));
    QCOMPARE(controller.state(), State::Locked);
    controller.moveClockForTests(10);
    controller.unlockWithPin(QStringLiteral("2468"));
    QVERIFY(settled(controller, State::Unlocked));
    controller.lock();
    QCOMPARE(controller.unlockWaitSeconds(), 0); // the right one ended the count
}

void TestPinUnlock::a_pin_needs_the_typed_master_password_and_can_be_removed()
{
    VaultController controller;
    const QString file = unlockedVault(controller, QStringLiteral("short"));
    // The open vault's owner sets its PIN without its password (the owner, D-076).
    controller.setPinFor(file, QStringLiteral("0000"));
    QTRY_VERIFY(!controller.pinBusy() && controller.hasPin(file));
    controller.removePinFor(file);
    QVERIFY(!controller.hasPin(file));
    controller.lock();
    QTRY_VERIFY(controller.state() != VaultController::State::Unlocked);
    // A locked vault, nothing proven: no PIN (P79's first way).
    controller.askFor(file);
    controller.setPinFor(file, QStringLiteral("0000"));
    controller.stopAsking();
    QVERIFY(!controller.pinBusy() && !controller.hasPin(file));
    // A wrong master password: no PIN either.
    QVERIFY(!pinSetTo(controller, file, QStringLiteral("wrong"), QStringLiteral("0000")));
    QVERIFY(!controller.hasPin(file));
    QVERIFY(pinSetTo(controller, file, QStringLiteral("right"), QStringLiteral("123")));
    QVERIFY(!controller.hasPin(file));
    QVERIFY(pinSetTo(controller, file, QStringLiteral("right"), QStringLiteral("1234")));
    QVERIFY(controller.hasPin(file));
    controller.removePinFor(file);
    QVERIFY(!controller.hasPin(file));
    VaultsFile settings;
    settings.beginGroup(QStringLiteral("pins"));
    QVERIFY(settings.allKeys().isEmpty());
    settings.endGroup();
    QVERIFY(!controller.pinUnlockReady());
}

void TestPinUnlock::nothing_on_disk_opens_with_the_pin_alone()
{
    VaultController controller;
    const QString file = unlockedVault(controller, QStringLiteral("disk"));
    QVERIFY(pinSetTo(controller, file, QStringLiteral("right"), QStringLiteral("9999")));
    controller.lock();
    VaultsFile settings;
    settings.beginGroup(QStringLiteral("pins"));
    const QStringList groups = settings.childGroups();
    QCOMPARE(groups.size(), 1);
    settings.beginGroup(groups.constFirst());
    const QByteArray salt = settings.value(QStringLiteral("salt")).toByteArray();
    QVERIFY(!salt.isEmpty());
    // The key a thief could make from a guessed PIN, with what is on disk.
    QDataStream in(settings.value(QStringLiteral("derivation")).toByteArray());
    quint8 version = 0;
    crypto::KdfParameters parameters;
    in >> version >> parameters.memoryKiB >> parameters.iterations >> parameters.lanes;
    QCOMPARE(in.status(), QDataStream::Ok);
    const std::optional<QByteArray> guessed = crypto::deriveKey("9999", salt, parameters);
    QVERIFY(guessed);
    QVERIFY(!crypto::open(*guessed, settings.value(QStringLiteral("entry")).toByteArray(), "pin key"));
}

void TestPinUnlock::the_reminder_asks_the_master_password_instead()
{
    VaultController controller;
    const QString file = unlockedVault(controller, QStringLiteral("reminder"));
    QVERIFY(pinSetTo(controller, file, QStringLiteral("right"), QStringLiteral("4321")));
    controller.setMasterPasswordReminder(0); // once a day
    VaultsFile settings;
    QVariantMap times;
    times.insert(file, QDateTime::currentDateTimeUtc().addDays(-2));
    settings.setValue(QStringLiteral("security/lastMasterPassword"), times);
    controller.lock();
    QVERIFY(controller.reminderDue());
    QVERIFY(!controller.pinUnlockReady());
    controller.unlockWithPin(QStringLiteral("4321"));
    QCOMPARE(controller.state(), State::Locked);
    controller.unlock(QStringLiteral("right"));
    QVERIFY(settled(controller, State::Unlocked));
    controller.lock();
    QVERIFY(!controller.reminderDue());
    QVERIFY(controller.pinUnlockReady());
}

void TestPinUnlock::a_changed_master_password_keeps_the_pin()
{
    VaultController controller;
    const QString file = unlockedVault(controller, QStringLiteral("change"));
    QVERIFY(pinSetTo(controller, file, QStringLiteral("right"), QStringLiteral("5555")));
    QSignalSpy changed(&controller, &VaultController::masterPasswordChanged);
    controller.changeMasterPassword(QStringLiteral("right"), QStringLiteral("new one"), QStringLiteral("new one"),
                                    QUrl());
    QVERIFY(changed.wait(10000));
    QCOMPARE(changed.constFirst().constFirst().toBool(), true);
    QVERIFY(QTest::qWaitFor([&controller]() { return controller.isIdle(); }, 10000));
    controller.lock();
    QVERIFY(controller.pinUnlockReady());
    controller.unlockWithPin(QStringLiteral("5555"));
    QVERIFY(settled(controller, State::Unlocked));
}

void TestPinUnlock::the_old_pin_of_every_vault_is_removed()
{
    {
        VaultsFile settings;
        settings.setValue(QStringLiteral("pin/salt"), QByteArray("salt"));
        settings.setValue(QStringLiteral("pin/vaults/abc"), QByteArray("sealed"));
    }
    VaultController controller;
    VaultsFile settings;
    QVERIFY(!settings.contains(QStringLiteral("pin/salt")));
    QVERIFY(!settings.contains(QStringLiteral("pin/vaults/abc")));
}

// P79: a PIN of one vault never opens another, however it was set.
void TestPinUnlock::a_pin_opens_only_its_own_vault()
{
    VaultController controller;
    const QString home = unlockedVault(controller, QStringLiteral("home"), QStringLiteral("home pass"));
    const QString work = unlockedVault(controller, QStringLiteral("work"), QStringLiteral("work pass"), true);
    QVERIFY(!home.isEmpty() && !work.isEmpty());
    QVERIFY(pinSetTo(controller, work, QStringLiteral("work pass"), QStringLiteral("1111")));
    // Logged in with the primary (Work opened out of it): Work's PIN does not
    // take back what the primary's key store holds.
    controller.lock();
    QCOMPARE(controller.vaultPath(), home);
    QVERIFY(!controller.pinUnlockReady());
    controller.switchToVault(work);
    QVERIFY(!controller.pinUnlockReady());
    controller.unlockWithPin(QStringLiteral("1111"));
    QCOMPARE(controller.state(), State::Locked);
    // The owner's way: the primary skipped, Work typed: its PIN gives Work back,
    // and the primary still asks its own password.
    controller.unlock(QStringLiteral("work pass"));
    QVERIFY(settled(controller, State::Unlocked));
    QVERIFY(!controller.keyStoreOpen());
    controller.lock();
    QCOMPARE(controller.vaultPath(), work);
    QVERIFY(controller.pinUnlockReady());
    controller.switchToVault(home);
    QVERIFY(!controller.pinUnlockReady());
    controller.switchToVault(work);
    controller.unlockWithPin(QStringLiteral("1111"));
    QVERIFY(settled(controller, State::Unlocked));
    QCOMPARE(controller.vaultPath(), work);
    QVERIFY(!controller.keyStoreOpen());
    QCOMPARE(controller.vaultStateOf(home), 0);
}

void TestPinUnlock::a_lock_gives_everything_back_as_a_screen_lock()
{
    VaultController controller;
    const QString home = unlockedVault(controller, QStringLiteral("home"), QStringLiteral("home pass"));
    const QString kept = unlockedVault(controller, QStringLiteral("kept"), QStringLiteral("kept pass"), true);
    const QString own = unlockedVault(controller, QStringLiteral("own"), QStringLiteral("own pass"));
    QVERIFY(pinSetTo(controller, home, QStringLiteral("home pass"), QStringLiteral("2222")));
    controller.switchToVault(home);
    QVERIFY(settled(controller, State::Unlocked));
    // A vault whose password is not kept, typed while logged in with the primary.
    controller.switchToVault(own);
    controller.unlock(QStringLiteral("own pass"));
    QVERIFY(settled(controller, State::Unlocked));
    QVERIFY(controller.keyStoreOpen());
    QCOMPARE(controller.vaultStateOf(kept), 1);
    controller.lock();
    QCOMPARE(controller.vaultPath(), home);
    QCOMPARE(controller.returnVault(), own);
    QVERIFY(controller.pinUnlockReady());
    controller.unlockWithPin(QStringLiteral("2222"));
    QVERIFY(QTest::qWaitFor([&]() { return controller.vaultPath() == own && controller.state() == State::Unlocked; }, 10000));
    QVERIFY(controller.keyStoreOpen());
    QCOMPARE(controller.vaultStateOf(kept), 1);
    // The same with the primary's password: the primary's items never shown.
    controller.lock();
    bool primaryShown = false;
    QObject::connect(&controller, &VaultController::stateChanged, &controller, [&]() {
        primaryShown = primaryShown || (controller.state() == State::Unlocked && controller.vaultPath() == home);
    });
    controller.unlock(QStringLiteral("home pass"));
    QVERIFY(QTest::qWaitFor([&]() { return controller.vaultPath() == own && controller.state() == State::Unlocked; }, 10000));
    QVERIFY(!primaryShown);
    QVERIFY(controller.keyStoreOpen());
    QCOMPARE(controller.vaultStateOf(kept), 1);
}

// The owner's example: the manager locks, an employee logs in with his own vault.
void TestPinUnlock::another_vault_got_into_is_a_new_login()
{
    VaultController controller;
    const QString home = unlockedVault(controller, QStringLiteral("manager"), QStringLiteral("boss"));
    const QString staff = unlockedVault(controller, QStringLiteral("staff"), QStringLiteral("new hire"));
    QVERIFY(pinSetTo(controller, home, QStringLiteral("boss"), QStringLiteral("3333")));
    QVERIFY(pinSetTo(controller, staff, QStringLiteral("new hire"), QStringLiteral("4444")));
    controller.switchToVault(home);
    QVERIFY(settled(controller, State::Unlocked));
    controller.lock();
    QCOMPARE(controller.vaultPath(), home);
    QVERIFY(!controller.keyStoreOpen()); // put away: no "Remember my credentials" (D-076)
    controller.switchToVault(staff);
    controller.unlock(QStringLiteral("new hire"), true);
    QVERIFY(settled(controller, State::Unlocked));
    QVERIFY(!controller.keyStoreOpen());
    QVERIFY(!controller.isKept(staff)); // nothing went into the manager's vault
    controller.lock();
    QCOMPARE(controller.vaultPath(), staff);
    QVERIFY(controller.pinUnlockReady());
    // The manager's PIN no longer opens anything: it was a new login.
    controller.switchToVault(home);
    QVERIFY(!controller.pinUnlockReady());
}

void TestPinUnlock::the_settings_and_the_unlock_page_offer_the_pin()
{
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    QVERIFY(controller);
    const QString file = unlockedVault(*controller, QStringLiteral("page"));
    QVERIFY(!file.isEmpty());

    // Settings > Security, in the owner's order, without the PIN now.
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    settings->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 1);
    QVERIFY(!settings->findChild<QQuickItem*>(QStringLiteral("pinBox")));
    const QStringList order{QStringLiteral("changeMasterPasswordButton"), QStringLiteral("reminderBox"),
                            QStringLiteral("autolockGroup"), QStringLiteral("quickUnlockToggle"),
                            QStringLiteral("hideSensitiveBox")};
    qreal above = -1;
    for (const QString& name : order) {
        auto* item = settings->findChild<QQuickItem*>(name);
        QVERIFY2(item, qPrintable(name));
        const qreal y = item->mapToScene(QPointF(0, 0)).y();
        QVERIFY2(y > above, qPrintable(name));
        above = y;
    }

    // Set PIN… in the vault's menu: its master password and the PIN; OK waits
    // for 4 characters.
    settings->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 2);
    QQuickItem* dots = nullptr;
    // Rows are delegates: looked for in the tree of items.
    const std::function<QQuickItem*(QQuickItem*)> find = [&find](QQuickItem* in) -> QQuickItem* {
        if (in->objectName() == QStringLiteral("vaultMenuButton_page")) {
            return in;
        }
        for (QQuickItem* child : in->childItems()) {
            if (QQuickItem* found = find(child)) {
                return found;
            }
        }
        return nullptr;
    };
    QTRY_VERIFY((dots = find(qobject_cast<QQuickWindow*>(settings)->contentItem())));
    QObject* menu = dots->findChild<QObject*>(QStringLiteral("vaultMenu_page"));
    QVERIFY(menu);
    QObject* setPin = nullptr;
    for (int index = 0; index < menu->property("count").toInt() && !setPin; ++index) {
        QQuickItem* one = nullptr;
        QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem*, one), Q_ARG(int, index));
        if (one && one->objectName() == QStringLiteral("setPin_page")) {
            setPin = one;
        }
    }
    QVERIFY(setPin);
    QCOMPARE(setPin->property("text").toString(), QStringLiteral("Set PIN…"));
    QVERIFY(QMetaObject::invokeMethod(setPin, "triggered"));
    auto* pinWindow = settings->findChild<QQuickWindow*>(QStringLiteral("pinWindow"));
    QVERIFY(pinWindow);
    QTRY_VERIFY(pinWindow->isVisible());
    auto* ok = pinWindow->findChild<QQuickItem*>(QStringLiteral("askOk"));
    pinWindow->findChild<QObject*>(QStringLiteral("askField"))->setProperty("text", QStringLiteral("right"));
    pinWindow->findChild<QObject*>(QStringLiteral("askNewPin"))->setProperty("text", QStringLiteral("12"));
    QVERIFY(!ok->isEnabled());
    pinWindow->findChild<QObject*>(QStringLiteral("askNewPin"))->setProperty("text", QStringLiteral("1234"));
    QVERIFY(ok->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(pinWindow, "go"));
    QTRY_VERIFY_WITH_TIMEOUT(controller->hasPin(file) && !controller->pinBusy(), 10000);
    QTRY_VERIFY(!pinWindow->isVisible());
    QTRY_COMPARE(setPin->property("text").toString(), QStringLiteral("Change PIN…"));
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));

    // Locked: the field asks the PIN, with the list; the password on request.
    controller->lock();
    auto* field = window->findChild<QQuickItem*>(QStringLiteral("masterPasswordField"));
    auto* instead = window->findChild<QQuickItem*>(QStringLiteral("useMasterPasswordButton"));
    auto* line = window->findChild<QQuickItem*>(QStringLiteral("unlockLine"));
    auto* vaultBox = window->findChild<QQuickItem*>(QStringLiteral("unlockVaultBox"));
    QVERIFY(field && instead && line && vaultBox);
    QTRY_COMPARE(field->property("placeholderText").toString(), QStringLiteral("PIN"));
    QVERIFY(instead->isVisible() && vaultBox->isVisible());
    QCOMPARE(instead->property("text").toString(), QStringLiteral("Unlock using password instead"));
    QCOMPARE(line->property("text").toString(), QStringLiteral("Enter the PIN to unlock"));
    QVERIFY(QMetaObject::invokeMethod(instead, "clicked"));
    QCOMPARE(field->property("placeholderText").toString(), QStringLiteral("Master Password"));
    QCOMPARE(line->property("text").toString(), QStringLiteral("Enter the Master Password to unlock"));
    controller->unlock(QStringLiteral("right"));
    QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);
    controller->lock();
    QTRY_COMPARE(field->property("placeholderText").toString(), QStringLiteral("PIN"));
    field->setProperty("text", QStringLiteral("1234"));
    QVERIFY(QMetaObject::invokeMethod(field, "accepted"));
    QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);

    QString warningText;
    for (const QQmlError& one : std::as_const(qmlWarnings)) {
        warningText += one.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

QTEST_MAIN(TestPinUnlock)
#include "test_pin_unlock.moc"
