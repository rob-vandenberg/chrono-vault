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

#include "vault_controller.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include "settings_files.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vault_controller.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.4: An item's icon survives a lock
// v0.0.3: Initial release, with a change that could not be saved surviving a lock

using namespace chrono_vault;
using State = VaultController::State;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};

QVariantMap withField(QVariantMap item, const QString& role, const QString& value)
{
    QVariantList fields = item.value(QStringLiteral("fields")).toList();
    for (QVariant& field : fields) {
        QVariantMap map = field.toMap();
        if (map.value(QStringLiteral("role")).toString() == role) {
            map.insert(QStringLiteral("value"), value);
            field = map;
        }
    }
    item.insert(QStringLiteral("fields"), fields);
    return item;
}

QString fieldOf(const QVariantMap& item, const QString& role)
{
    for (const QVariant& field : item.value(QStringLiteral("fields")).toList()) {
        if (field.toMap().value(QStringLiteral("role")).toString() == role) {
            return field.toMap().value(QStringLiteral("value")).toString();
        }
    }
    return {};
}

} // namespace

class TestVaultController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void a_new_vault_is_created_unlocked_and_empty();
    void a_saved_item_is_there_after_locking_and_unlocking();
    void a_wrong_password_leaves_the_vault_locked_and_says_so();
    void a_changed_and_a_deleted_item_are_saved_as_such();
    void a_new_vault_is_never_made_over_an_existing_file();
    void the_two_passwords_must_be_the_same();
    void what_is_copied_is_cleared_after_the_chosen_seconds();
    void the_generator_and_one_time_codes_answer();
    void the_last_vault_is_offered_again_locked();
    void a_change_that_could_not_be_saved_survives_a_lock();
    void a_save_that_succeeds_after_the_lock_leaves_nothing_kept();

private:
    VaultController* makeController();
    void createVault(VaultController& controller);

    QTemporaryDir* folder = nullptr;
    QString vaultPath;
};

void TestVaultController::initTestCase()
{
    // Settings of the tests' own, never the person's.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_vault_controller"));
}

void TestVaultController::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestVaultController::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
    vaultPath = folder->filePath(QStringLiteral("My vault.chronovault"));
}

void TestVaultController::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
    delete folder;
    folder = nullptr;
}

VaultController* TestVaultController::makeController()
{
    auto* controller = new VaultController(this);
    controller->setKdfParameters(quick);
    return controller;
}

void TestVaultController::createVault(VaultController& controller)
{
    controller.createVault(QUrl::fromLocalFile(vaultPath), QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(controller.state(), State::Unlocked);
    QVERIFY(QFile::exists(vaultPath));
}

void TestVaultController::a_new_vault_is_created_unlocked_and_empty()
{
    std::unique_ptr<VaultController> controller(makeController());
    QCOMPARE(controller->state(), State::NoVault);
    createVault(*controller);
    QCOMPARE(controller->items()->rowCount(), 0);
    QCOMPARE(controller->vaultName(), QStringLiteral("My vault"));
}

void TestVaultController::a_saved_item_is_there_after_locking_and_unlocking()
{
    std::unique_ptr<VaultController> controller(makeController());
    createVault(*controller);

    QVariantMap login = withField(controller->newLogin(), QStringLiteral("password"), QStringLiteral("s3cret!"));
    login.insert(QStringLiteral("title"), QStringLiteral("Bank"));
    login.insert(QStringLiteral("icon"), QString::fromLatin1(QByteArray("png bytes of an icon").toBase64()));
    QVERIFY(controller->saveItem(login));
    QTRY_VERIFY(controller->isIdle());

    controller->lock();
    QCOMPARE(controller->state(), State::Locked);
    QCOMPARE(controller->items()->rowCount(), 0);

    controller->unlock(QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QCOMPARE(controller->items()->rowCount(), 1);
    const QString id = controller->items()->data(controller->items()->index(0), ItemListModel::IdRole).toString();
    const QVariantMap stored = controller->item(id);
    QCOMPARE(stored.value(QStringLiteral("title")).toString(), QStringLiteral("Bank"));
    QCOMPARE(fieldOf(stored, QStringLiteral("password")), QStringLiteral("s3cret!"));
    QCOMPARE(QByteArray::fromBase64(stored.value(QStringLiteral("icon")).toString().toLatin1()),
             QByteArray("png bytes of an icon"));
}

void TestVaultController::a_wrong_password_leaves_the_vault_locked_and_says_so()
{
    std::unique_ptr<VaultController> controller(makeController());
    createVault(*controller);
    controller->lock();

    controller->unlock(QStringLiteral("Master"));
    QTRY_COMPARE(controller->state(), State::Locked);
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(!controller->message().isEmpty());
}

void TestVaultController::a_changed_and_a_deleted_item_are_saved_as_such()
{
    std::unique_ptr<VaultController> controller(makeController());
    createVault(*controller);

    QVariantMap first = controller->newLogin();
    first.insert(QStringLiteral("title"), QStringLiteral("First"));
    QVariantMap second = controller->newSecureNote();
    second.insert(QStringLiteral("title"), QStringLiteral("Second"));
    QVERIFY(controller->saveItem(first));
    QVERIFY(controller->saveItem(second));

    first = withField(first, QStringLiteral("username"), QStringLiteral("anna"));
    QVERIFY(controller->saveItem(first));
    controller->deleteItem(second.value(QStringLiteral("id")).toString());
    QTRY_VERIFY(controller->isIdle());

    // A second program reading the same file sees exactly that.
    std::unique_ptr<VaultController> reader(makeController());
    reader->chooseVault(QUrl::fromLocalFile(vaultPath));
    reader->unlock(QStringLiteral("master"));
    QTRY_COMPARE(reader->state(), State::Unlocked);
    QCOMPARE(reader->items()->rowCount(), 1);
    const QVariantMap stored = reader->item(first.value(QStringLiteral("id")).toString());
    QCOMPARE(stored.value(QStringLiteral("title")).toString(), QStringLiteral("First"));
    QCOMPARE(fieldOf(stored, QStringLiteral("username")), QStringLiteral("anna"));
}

void TestVaultController::a_new_vault_is_never_made_over_an_existing_file()
{
    {
        QFile existing(vaultPath);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        existing.write("someone's other file");
    }
    std::unique_ptr<VaultController> controller(makeController());
    QVERIFY(!controller->canCreateAt(QUrl::fromLocalFile(vaultPath)));
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(controller->canCreateAt(QUrl::fromLocalFile(folder->filePath(QStringLiteral("free name.chronovault")))));
    controller->createVault(QUrl::fromLocalFile(vaultPath), QStringLiteral("master"), QStringLiteral("master"));
    QCOMPARE(controller->state(), State::NoVault);
    QVERIFY(controller->messageIsTrouble());
    QFile existing(vaultPath);
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), QByteArray("someone's other file"));
}

void TestVaultController::the_two_passwords_must_be_the_same()
{
    std::unique_ptr<VaultController> controller(makeController());
    controller->createVault(QUrl::fromLocalFile(vaultPath), QStringLiteral("master"), QStringLiteral("mastr"));
    QCOMPARE(controller->state(), State::NoVault);
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(!QFile::exists(vaultPath));
    controller->createVault(QUrl::fromLocalFile(vaultPath), QString(), QString());
    QCOMPARE(controller->state(), State::NoVault);
    QVERIFY(!QFile::exists(vaultPath));
}

void TestVaultController::what_is_copied_is_cleared_after_the_chosen_seconds()
{
    std::unique_ptr<VaultController> controller(makeController());
    controller->setClipboardClearSeconds(1);
    controller->copyToClipboard(QStringLiteral("s3cret!"));
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("s3cret!"));
    QTRY_COMPARE_WITH_TIMEOUT(QGuiApplication::clipboard()->text(), QString(), 5000);
}

void TestVaultController::the_generator_and_one_time_codes_answer()
{
    std::unique_ptr<VaultController> controller(makeController());
    QCOMPARE(controller->generatePassword(24, true, true, true, false).size(), 24);
    QCOMPARE(controller->oneTimeCode(QStringLiteral("JBSWY3DPEHPK3PXP")).size(), 6);
    QVERIFY(controller->oneTimeCode(QStringLiteral("not a secret!")).isEmpty());
    QCOMPARE(controller->fieldName(QStringLiteral("password")), QStringLiteral("Password"));
}

void TestVaultController::the_last_vault_is_offered_again_locked()
{
    {
        std::unique_ptr<VaultController> controller(makeController());
        createVault(*controller);
    }
    std::unique_ptr<VaultController> again(makeController());
    QCOMPARE(again->state(), State::Locked);
    QCOMPARE(again->vaultPath(), vaultPath);
}

void TestVaultController::a_change_that_could_not_be_saved_survives_a_lock()
{
    std::unique_ptr<VaultController> controller(makeController());
    createVault(*controller);

    // A folder where the save writes its new file makes every save fail, as
    // a folder that cannot be written to does.
    const QString blocker = savingPathFor(vaultPath);
    QVERIFY(QDir().mkdir(blocker));
    QVariantMap login = controller->newLogin();
    login.insert(QStringLiteral("title"), QStringLiteral("Not saved yet"));
    QVERIFY(controller->saveItem(login));
    QTRY_VERIFY(controller->isIdle());
    QVERIFY(controller->unsavedChanges());
    QVERIFY(controller->saveFailed());
    QVERIFY(controller->messageIsTrouble());

    // Another message, such as after copying, does not hide that a change
    // is not saved.
    controller->copyToClipboard(QStringLiteral("x"));
    QVERIFY(!controller->messageIsTrouble());
    QVERIFY(controller->saveFailed());

    controller->lock();
    QCOMPARE(controller->state(), State::Locked);
    QVERIFY(controller->hasKeptChanges());
    QVERIFY(QDir().rmdir(blocker));

    controller->unlock(QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QCOMPARE(controller->items()->rowCount(), 1);
    QTRY_VERIFY(controller->isIdle());
    QVERIFY(!controller->unsavedChanges());
    QVERIFY(!controller->saveFailed());
    QVERIFY(!controller->hasKeptChanges());

    std::unique_ptr<VaultController> reader(makeController());
    reader->chooseVault(QUrl::fromLocalFile(vaultPath));
    reader->unlock(QStringLiteral("master"));
    QTRY_COMPARE(reader->state(), State::Unlocked);
    QCOMPARE(reader->items()->rowCount(), 1);
}

void TestVaultController::a_save_that_succeeds_after_the_lock_leaves_nothing_kept()
{
    std::unique_ptr<VaultController> controller(makeController());
    createVault(*controller);
    QVERIFY(controller->saveItem(controller->newLogin()));
    controller->lock(); // the save is still running
    QVERIFY(controller->hasKeptChanges());
    QTRY_VERIFY(controller->isIdle());
    QVERIFY(!controller->hasKeptChanges());
}

QTEST_MAIN(TestVaultController)
#include "test_vault_controller.moc"
