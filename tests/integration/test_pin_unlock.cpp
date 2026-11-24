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
// test_pin_unlock.cpp 0.4.70

// --- Version history ----------------------------------------------------
// v0.4.70: Initial release (D-059)

#include "app_setup.h"
#include "crypto.h"
#include "settings_files.h"
#include "vault_controller.h"

#include <QDataStream>
#include <QDateTime>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
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
    void a_pin_unlocks_after_a_lock_and_never_after_a_restart();
    void wrong_pins_count_toward_the_same_waits();
    void a_short_pin_is_refused_and_an_empty_one_removes_it();
    void nothing_on_disk_opens_with_the_pin_alone();
    void the_reminder_asks_the_master_password_instead();
    void a_changed_master_password_keeps_the_pin();
    void the_settings_and_the_unlock_page_offer_the_pin();

private:
    // A vault made and left unlocked; its file.
    QString unlockedVault(VaultController& controller, const QString& name);
    static bool pinSetTo(VaultController& controller, const QString& pin);
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

QString TestPinUnlock::unlockedVault(VaultController& controller, const QString& name)
{
    controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    const QString file = folder->filePath(name + QStringLiteral(".chronovault"));
    controller.createVault(QUrl::fromLocalFile(file), QStringLiteral("right"), QStringLiteral("right"));
    return settled(controller, State::Unlocked) ? file : QString();
}

bool TestPinUnlock::pinSetTo(VaultController& controller, const QString& pin)
{
    controller.setPin(pin);
    return QTest::qWaitFor([&controller]() { return !controller.pinBusy(); }, 10000);
}

bool TestPinUnlock::settled(VaultController& controller, State state)
{
    return QTest::qWaitFor([&controller, state]() { return controller.state() == state; }, 10000);
}

void TestPinUnlock::a_pin_unlocks_after_a_lock_and_never_after_a_restart()
{
    {
        VaultController controller;
        QVERIFY(!unlockedVault(controller, QStringLiteral("pin")).isEmpty());
        QVERIFY(!controller.pinSet());
        QVERIFY(pinSetTo(controller, QStringLiteral("12 ab")));
        QVERIFY(controller.pinSet());
        controller.lock();
        QVERIFY(controller.pinUnlockReady());
        controller.unlockWithPin(QStringLiteral("12 ab"));
        QVERIFY(settled(controller, State::Unlocked));
        controller.lock();
        QVERIFY(controller.pinUnlockReady());
    }
    // After a start the master password comes first; then the PIN again.
    VaultController controller;
    controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    QVERIFY(settled(controller, State::Locked));
    QVERIFY(controller.pinSet());
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
    QVERIFY(!unlockedVault(controller, QStringLiteral("waits")).isEmpty());
    QVERIFY(pinSetTo(controller, QStringLiteral("2468")));
    controller.lock();
    // Two wrong PINs and a wrong master password: three, then 10 seconds.
    for (int attempt = 1; attempt <= 2; ++attempt) {
        controller.unlockWithPin(QStringLiteral("1357"));
        QVERIFY(QTest::qWaitFor([&controller]() { return controller.state() == State::Locked; }, 10000));
        QCOMPARE(controller.unlockWaitSeconds(), 0);
    }
    controller.unlock(QStringLiteral("wrong"));
    QVERIFY(QTest::qWaitFor([&controller]() { return controller.state() == State::Locked; }, 10000));
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

void TestPinUnlock::a_short_pin_is_refused_and_an_empty_one_removes_it()
{
    VaultController controller;
    QVERIFY(!unlockedVault(controller, QStringLiteral("short")).isEmpty());
    QVERIFY(pinSetTo(controller, QStringLiteral("123")));
    QVERIFY(!controller.pinSet());
    QVERIFY(pinSetTo(controller, QStringLiteral("1234")));
    QVERIFY(controller.pinSet());
    QVERIFY(pinSetTo(controller, QString()));
    QVERIFY(!controller.pinSet());
    VaultsFile settings;
    settings.beginGroup(QStringLiteral("pin"));
    QVERIFY(settings.allKeys().isEmpty());
    settings.endGroup();
    controller.lock();
    QVERIFY(!controller.pinUnlockReady());
}

void TestPinUnlock::nothing_on_disk_opens_with_the_pin_alone()
{
    VaultController controller;
    QVERIFY(!unlockedVault(controller, QStringLiteral("disk")).isEmpty());
    QVERIFY(pinSetTo(controller, QStringLiteral("9999")));
    controller.lock();
    VaultsFile settings;
    const QByteArray salt = settings.value(QStringLiteral("pin/salt")).toByteArray();
    QVERIFY(!salt.isEmpty());
    // The key a thief could make from a guessed PIN, with what is on disk.
    QDataStream in(settings.value(QStringLiteral("pin/derivation")).toByteArray());
    quint8 version = 0;
    crypto::KdfParameters parameters;
    in >> version >> parameters.memoryKiB >> parameters.iterations >> parameters.lanes;
    QCOMPARE(in.status(), QDataStream::Ok);
    const std::optional<QByteArray> guessed = crypto::deriveKey("9999", salt, parameters);
    QVERIFY(guessed);
    settings.beginGroup(QStringLiteral("pin/vaults"));
    const QStringList entries = settings.allKeys();
    QCOMPARE(entries.size(), 1);
    for (const QString& entry : entries) {
        const QByteArray sealed = settings.value(entry).toByteArray();
        QVERIFY(!crypto::open(*guessed, sealed, "pin key"));
        QVERIFY(!crypto::open(*guessed, sealed, "pin locked"));
    }
    settings.endGroup();
}

void TestPinUnlock::the_reminder_asks_the_master_password_instead()
{
    VaultController controller;
    const QString file = unlockedVault(controller, QStringLiteral("reminder"));
    QVERIFY(!file.isEmpty());
    QVERIFY(pinSetTo(controller, QStringLiteral("4321")));
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
    QVERIFY(!unlockedVault(controller, QStringLiteral("change")).isEmpty());
    QVERIFY(pinSetTo(controller, QStringLiteral("5555")));
    QSignalSpy changed(&controller, &VaultController::masterPasswordChanged);
    controller.changeMasterPassword(QStringLiteral("right"), QStringLiteral("new one"), QStringLiteral("new one"),
                                    QUrl());
    QVERIFY(changed.wait(10000));
    QCOMPARE(changed.constFirst().constFirst().toBool(), true);
    QVERIFY(QTest::qWaitFor([&controller]() { return controller.isIdle(); }, 10000));
    controller.lock();
    controller.unlockWithPin(QStringLiteral("5555"));
    QVERIFY(settled(controller, State::Unlocked));
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
    QVERIFY(!unlockedVault(*controller, QStringLiteral("page")).isEmpty());

    // Settings > Security, in the owner's order: the master password, the
    // locking, the PIN, the finger, then sensitive fields.
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    settings->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 1);
    const QStringList order{QStringLiteral("changeMasterPasswordButton"), QStringLiteral("reminderBox"),
                            QStringLiteral("autolockGroup"),              QStringLiteral("pinBox"),
                            QStringLiteral("quickUnlockToggle"),          QStringLiteral("hideSensitiveBox")};
    qreal above = -1;
    for (const QString& name : order) {
        auto* item = settings->findChild<QQuickItem*>(name);
        QVERIFY2(item, qPrintable(name));
        const qreal y = item->mapToScene(QPointF(0, 0)).y();
        QVERIFY2(y > above, qPrintable(name));
        above = y;
    }

    // The PIN: OK waits for 4 characters; the eye only while one is typed.
    auto* pinBox = settings->findChild<QQuickItem*>(QStringLiteral("pinBox"));
    auto* pinField = settings->findChild<QQuickItem*>(QStringLiteral("pinField"));
    auto* okButton = settings->findChild<QQuickItem*>(QStringLiteral("okButton"));
    QVERIFY(pinBox && pinField && okButton);
    QVERIFY(!pinField->property("eyeAllowed").toBool());
    QVERIFY(QMetaObject::invokeMethod(pinBox, "toggle"));
    QVERIFY(QMetaObject::invokeMethod(pinBox, "toggled"));
    QVERIFY(!okButton->isEnabled());
    pinField->setProperty("text", QStringLiteral("12"));
    QVERIFY(pinField->property("eyeAllowed").toBool());
    QVERIFY(!okButton->isEnabled());
    pinField->setProperty("text", QStringLiteral("1234"));
    QVERIFY(okButton->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(okButton, "clicked"));
    QTRY_VERIFY_WITH_TIMEOUT(controller->pinSet() && !controller->pinBusy(), 10000);
    QTRY_VERIFY(!settings->property("visible").toBool());

    // Locked: the field asks the PIN; the master password on request.
    controller->lock();
    auto* field = window->findChild<QQuickItem*>(QStringLiteral("masterPasswordField"));
    auto* instead = window->findChild<QQuickItem*>(QStringLiteral("useMasterPasswordButton"));
    QVERIFY(field && instead);
    QTRY_COMPARE(field->property("placeholderText").toString(), QStringLiteral("PIN"));
    QVERIFY(instead->isVisible());
    QVERIFY(QMetaObject::invokeMethod(instead, "clicked"));
    QCOMPARE(field->property("placeholderText").toString(), QStringLiteral("Master Password"));
    QVERIFY(!instead->isVisible());
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
