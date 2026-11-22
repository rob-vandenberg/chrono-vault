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
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_quick_unlock.cpp 0.0.23

// --- Version history ----------------------------------------------------
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
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestQuickUnlock::init()
{
    QSettings().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
}

void TestQuickUnlock::cleanup()
{
    QSettings().clear();
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
        QSettings settings;
        QVariantMap times = settings.value(QStringLiteral("security/lastMasterPassword")).toMap();
        times.insert(controller->vaultPath(), QDateTime::currentDateTimeUtc().addDays(-400));
        settings.setValue(QStringLiteral("security/lastMasterPassword"), times);
    }
    QVERIFY(controller->reminderDue());
    controller->setMasterPasswordReminder(false);
    QVERIFY(!controller->reminderDue());
    controller->setMasterPasswordReminder(true);
    QVERIFY(controller->reminderDue());
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
