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
#include "quick_unlock.h"
#include "vault_controller.h"
#include "vault_file.h"

#include <QFile>
#include <QHash>
#include "settings_files.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_quick_unlock.cpp 0.6.103

// --- Version history ----------------------------------------------------
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.4.74: A lost key turns the setting off
// v0.0.39: Tests of the settings tidied, the scroll bars and Esc in the About box (D-049)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.23: Initial release (D-038)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

// Stands in for the fingerprint reader: the finger "says" what next says.
class StandIn : public QuickUnlock
{
public:
    QuickUnlock::Result next = QuickUnlock::Result::Done;
    QHash<QString, QByteArray> kept;
    int stores = 0;
    bool failStore = false;

    [[nodiscard]] bool available() const override { return true; }
    [[nodiscard]] QString name() const override { return QStringLiteral("fingerprint"); }
    void store(const QString& vault, const QByteArray& secret) override
    {
        ++stores;
        if (failStore) {
            remember(vault, false);
            emit stored(QuickUnlock::Result::Failed);
            return;
        }
        kept.insert(vault, secret);
        remember(vault, true);
        emit stored(QuickUnlock::Result::Done);
    }
    void retrieve(const QString& vault) override
    {
        if (next != QuickUnlock::Result::Done) {
            emit retrieved(next, {});
            return;
        }
        emit retrieved(kept.contains(vault) ? QuickUnlock::Result::Done : QuickUnlock::Result::NoKey, kept.value(vault));
    }
    void remove(const QString& vault) override
    {
        kept.remove(vault);
        remember(vault, false);
    }
};

} // namespace

class TestQuickUnlock : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void a_finger_unlocks_once_turned_on();
    void a_cancelled_or_unknown_finger_leaves_it_locked();
    void a_lost_key_turns_it_off_and_says_so();
    void a_new_master_password_is_kept_for_the_finger();
    void a_new_key_not_kept_says_so_and_turns_it_off();
    void the_master_password_is_asked_once_a_year();
    void the_standard_dialog_asks_the_finger_for_any_vault();
    void the_standard_dialog_waits_and_asks_the_keyfile();
    void a_master_password_changed_elsewhere_turns_it_off_and_says_so();

private:
    std::unique_ptr<VaultController> unlocked(StandIn*& standIn);
    QTemporaryDir* folder = nullptr;
};

void TestQuickUnlock::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_quick_unlock"));
}

void TestQuickUnlock::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestQuickUnlock::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
}

void TestQuickUnlock::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
    delete folder;
    folder = nullptr;
}

std::unique_ptr<VaultController> TestQuickUnlock::unlocked(StandIn*& standIn)
{
    auto controller = std::make_unique<VaultController>();
    auto owned = std::make_unique<StandIn>();
    standIn = owned.get();
    controller->setQuickUnlockForTests(std::move(owned));
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("finger.chronovault"))),
                            QStringLiteral("pass"), QStringLiteral("pass"));
    if (!QTest::qWaitFor([&controller]() { return controller->state() == State::Unlocked; }, 10000)) {
        return nullptr;
    }
    return controller;
}

void TestQuickUnlock::a_finger_unlocks_once_turned_on()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    QVERIFY(controller->quickUnlockAvailable());
    QVERIFY(!controller->quickUnlockOn());
    controller->setQuickUnlock(true);
    QVERIFY(controller->quickUnlockOn());
    QVERIFY(controller->saveItem(controller->newLogin()));
    QTRY_VERIFY(controller->isIdle());

    controller->lock();
    controller->unlockWithQuickUnlock();
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QCOMPARE(controller->items()->rowCount(), 1);

    controller->setQuickUnlock(false);
    QVERIFY(!controller->quickUnlockOn());
    QVERIFY(standIn->kept.isEmpty());
}

void TestQuickUnlock::a_cancelled_or_unknown_finger_leaves_it_locked()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    controller->setQuickUnlock(true);
    controller->lock();
    standIn->next = QuickUnlock::Result::Cancelled;
    controller->unlockWithQuickUnlock();
    QCOMPARE(controller->state(), State::Locked);
    QVERIFY(!controller->messageIsTrouble());
    standIn->next = QuickUnlock::Result::Failed;
    controller->unlockWithQuickUnlock();
    QCOMPARE(controller->state(), State::Locked);
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(controller->quickUnlockOn()); // still on: try again
    controller->unlock(QStringLiteral("pass"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
}

void TestQuickUnlock::a_lost_key_turns_it_off_and_says_so()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    controller->setQuickUnlock(true);
    controller->lock();
    standIn->next = QuickUnlock::Result::NoKey;
    controller->unlockWithQuickUnlock();
    QCOMPARE(controller->state(), State::Locked);
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(!controller->quickUnlockOn());
    // The setting itself is off, as the message says (the owner, 0.4.74).
    QVERIFY(!controller->quickUnlockEnabled());
    QVERIFY(controller->message().contains(QStringLiteral("so it was turned off")));
}

void TestQuickUnlock::a_new_master_password_is_kept_for_the_finger()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    controller->setQuickUnlock(true);
    QSignalSpy changed(controller.get(), &VaultController::masterPasswordChanged);
    controller->changeMasterPassword(QStringLiteral("pass"), QStringLiteral("new"), QStringLiteral("new"), {});
    QTRY_COMPARE(changed.size(), 1);
    QVERIFY(changed.first().first().toBool());
    QCOMPARE(standIn->stores, 2); // the new key kept
    QVERIFY(controller->quickUnlockOn());
    controller->lock();
    controller->unlockWithQuickUnlock();
    QTRY_COMPARE(controller->state(), State::Unlocked);
}

void TestQuickUnlock::a_new_key_not_kept_says_so_and_turns_it_off()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    controller->setQuickUnlock(true);
    standIn->failStore = true;
    QSignalSpy changed(controller.get(), &VaultController::masterPasswordChanged);
    controller->changeMasterPassword(QStringLiteral("pass"), QStringLiteral("new"), QStringLiteral("new"), {});
    QTRY_COMPARE(changed.size(), 1);
    QTRY_VERIFY(!controller->quickUnlockOn());
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(controller->message().contains(QStringLiteral("Settings")));
}

// The standard dialog (D-073): the vault that is open stays open until the
// other one is opened with what was proven.
void TestQuickUnlock::the_standard_dialog_asks_the_finger_for_any_vault()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    const QString first = controller->vaultPath();
    controller->setQuickUnlock(true);
    const QString second = folder->filePath(QStringLiteral("second.chronovault"));
    controller->createVault(QUrl::fromLocalFile(second), QStringLiteral("two"), QStringLiteral("two"));
    QTRY_VERIFY(controller->vaultPath() == second && controller->state() == State::Unlocked && controller->isIdle());
    controller->setQuickUnlock(true);
    QVERIFY(standIn->kept.contains(first) && standIn->kept.contains(second));
    controller->lock();
    controller->unlock(QStringLiteral("two"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    QSignalSpy proven(controller.get(), &VaultController::proven);
    // Cancelled: the one open is still open, and nothing was proven.
    standIn->next = QuickUnlock::Result::Cancelled;
    controller->askFor(first);
    QVERIFY(controller->askFingerOn());
    controller->proveWithFinger();
    QVERIFY(!controller->openProven());
    controller->stopAsking();
    QCOMPARE(proven.size(), 0);
    QVERIFY(controller->vaultPath() == second && controller->state() == State::Unlocked);
    // A wrong password: the same.
    controller->askFor(first);
    controller->proveWithPassword(QStringLiteral("wrong"));
    QTRY_VERIFY(controller->askTrouble());
    QVERIFY(!controller->openProven());
    QVERIFY(controller->vaultPath() == second && controller->state() == State::Unlocked);
    // The finger: proven, and opened only when that is asked.
    standIn->next = QuickUnlock::Result::Done;
    controller->askFor(first);
    controller->proveWithFinger();
    QTRY_COMPARE(proven.size(), 1);
    QVERIFY(controller->vaultPath() == second && controller->state() == State::Unlocked);
    QVERIFY(controller->openProven());
    controller->stopAsking();
    QTRY_VERIFY(controller->vaultPath() == first && controller->state() == State::Unlocked);
    // Enough for an export.
    QSignalSpy done(controller.get(), &VaultController::exportDone);
    controller->askFor(first);
    controller->proveWithFinger();
    QTRY_COMPARE(proven.size(), 2);
    controller->exportProven(QUrl::fromLocalFile(folder->filePath(QStringLiteral("out.json"))));
    QTRY_COMPARE(done.size(), 1);
    QVERIFY(done.last().first().toBool());
    // The master password due again: no finger.
    VaultsFile settings;
    QVariantMap times = settings.value(QStringLiteral("security/lastMasterPassword")).toMap();
    times.insert(second, QDateTime::currentDateTimeUtc().addDays(-400));
    settings.setValue(QStringLiteral("security/lastMasterPassword"), times);
    controller->askFor(second);
    QVERIFY(!controller->askFingerOn());
    controller->stopAsking();
}

void TestQuickUnlock::the_standard_dialog_waits_and_asks_the_keyfile()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    const QString keyfile = folder->filePath(QStringLiteral("key"));
    QFile made(keyfile);
    QVERIFY(made.open(QIODevice::WriteOnly) && made.write(QByteArray(64, 'k')) == 64);
    made.close();
    const QString first = controller->vaultPath();
    const QString second = folder->filePath(QStringLiteral("second.chronovault"));
    controller->createVault(QUrl::fromLocalFile(second), QStringLiteral("two"), QStringLiteral("two"),
                            QUrl::fromLocalFile(keyfile));
    QTRY_VERIFY(controller->vaultPath() == second && controller->state() == State::Unlocked && controller->isIdle());
    controller->lock();
    // Its keyfile not known on this computer.
    controller->askFor(second);
    controller->setAskKeyfile(QUrl());
    QVERIFY(!controller->askKeyfileNeeded());
    controller->proveWithPassword(QStringLiteral("two"));
    QTRY_VERIFY(controller->askKeyfileNeeded() && controller->askTrouble());
    controller->setAskKeyfile(QUrl::fromLocalFile(keyfile));
    QSignalSpy proven(controller.get(), &VaultController::proven);
    controller->proveWithPassword(QStringLiteral("two"));
    QTRY_COMPARE(proven.size(), 1);
    // Wrong passwords: the waits of D-041, for the vault asked for.
    controller->askFor(first);
    for (int wrong = 0; wrong < 3; ++wrong) {
        controller->proveWithPassword(QStringLiteral("no"));
        QTRY_VERIFY(!controller->askBusy());
    }
    QVERIFY(controller->askWaitSeconds() > 0);
    controller->proveWithPassword(QStringLiteral("pass"));
    QVERIFY(!controller->askBusy()); // not tried while waiting
    QCOMPARE(controller->unlockWaitSeconds(), 0); // the one shown does not wait
    controller->stopAsking();
}

void TestQuickUnlock::the_master_password_is_asked_once_a_year()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    controller->setQuickUnlock(true);
    controller->lock();
    QVERIFY(!controller->reminderDue());
    // A year and more since the master password was last typed.
    {
        VaultsFile settings;
        QVariantMap times = settings.value(QStringLiteral("security/lastMasterPassword")).toMap();
        times.insert(controller->vaultPath(), QDateTime::currentDateTimeUtc().addDays(-400));
        settings.setValue(QStringLiteral("security/lastMasterPassword"), times);
    }
    QVERIFY(controller->reminderDue());
    // Quarter by default; Never stops it; Year waits a year (D-049).
    QCOMPARE(controller->masterPasswordReminder(), 2);
    controller->setMasterPasswordReminder(4);
    QVERIFY(!controller->reminderDue());
    controller->setMasterPasswordReminder(3);
    QVERIFY(controller->reminderDue());
    {
        VaultsFile settings;
        QVariantMap times = settings.value(QStringLiteral("security/lastMasterPassword")).toMap();
        times.insert(controller->vaultPath(), QDateTime::currentDateTimeUtc().addDays(-100));
        settings.setValue(QStringLiteral("security/lastMasterPassword"), times);
    }
    QVERIFY(!controller->reminderDue()); // a year has not passed
    controller->setMasterPasswordReminder(2);
    QVERIFY(controller->reminderDue()); // a quarter has
    controller->setMasterPasswordReminder(1);
    QVERIFY(controller->reminderDue());
    controller->setMasterPasswordReminder(2);
    // "Not now": the reminder waits another year, and the finger opens it.
    controller->dismissReminder();
    QVERIFY(!controller->reminderDue());
    QTRY_COMPARE(controller->state(), State::Unlocked);
    // Typing it also lets the reminder wait a year.
    controller->lock();
    controller->unlock(QStringLiteral("pass"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QVERIFY(!controller->reminderDue());
}

void TestQuickUnlock::a_master_password_changed_elsewhere_turns_it_off_and_says_so()
{
    StandIn* standIn = nullptr;
    auto controller = unlocked(standIn);
    QVERIFY(controller);
    controller->setQuickUnlock(true);
    QTRY_VERIFY(controller->isIdle());
    const QString vaultFile = controller->vaultPath();
    controller->lock();
    // Another computer gives the vault another master password.
    const OpenResult opened = openVaultFile(vaultFile, "pass");
    QCOMPARE(opened.status, OpenStatus::Opened);
    QCOMPARE(saveVaultFile(vaultFile, opened.contents, makeVaultKey("other", crypto::KdfParameters{1024, 1, 1})),
             SaveStatus::Saved);
    controller->unlockWithQuickUnlock();
    QTRY_VERIFY(controller->messageIsTrouble());
    QCOMPARE(controller->state(), State::Locked);
    QVERIFY(controller->message().contains(QStringLiteral("another computer")));
    QVERIFY(!controller->quickUnlockOn());
}

QTEST_MAIN(TestQuickUnlock)
#include "test_quick_unlock.moc"
