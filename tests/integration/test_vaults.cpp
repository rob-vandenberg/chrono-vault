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
#include "vault_file.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vaults.cpp 0.0.15

// --- Version history ----------------------------------------------------
// v0.0.15: Initial release: several vaults, the keyfile, changing the master password (D-030, D-031)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};

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

// Several vaults, the first the primary whose master password unlocks the
// others; a keyfile beside the password; a new master password.
class TestVaults : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void a_second_vault_is_opened_with_its_password_saved_in_the_primary();
    void a_lock_locks_every_vault_and_the_primary_unlocks_them_again();
    void a_vault_whose_password_is_not_saved_asks_for_it();
    void the_one_vault_of_an_older_version_becomes_the_primary();
    void a_vault_with_a_keyfile_needs_it_and_remembers_it();
    void the_master_password_and_keyfile_are_changed_only_with_the_current_password();
    void a_primary_with_a_keyfile_takes_its_password_change_after_a_switch();
    void a_password_not_saved_in_the_primary_is_said_there_and_the_vault_stays_listed();
    void a_changed_password_is_changed_in_the_primary_too();
    void a_new_keyfile_is_never_made_over_a_file();
    void a_stale_saved_password_and_a_primary_changed_elsewhere_are_said();

private:
    std::unique_ptr<VaultController> controller();
    void create(VaultController& vaults, const QString& name, const QString& password, const QUrl& keyfile = {},
                bool save = false);
    QString file(const QString& name) const { return folder->filePath(name + QStringLiteral(".chronovault")); }

    QTemporaryDir* folder = nullptr;
};

void TestVaults::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_vaults"));
}

void TestVaults::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestVaults::init()
{
    QSettings().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
}

void TestVaults::cleanup()
{
    QSettings().clear();
    delete folder;
    folder = nullptr;
}

std::unique_ptr<VaultController> TestVaults::controller()
{
    auto made = std::make_unique<VaultController>();
    made->setKdfParameters(quick);
    return made;
}

void TestVaults::create(VaultController& vaults, const QString& name, const QString& password, const QUrl& keyfile,
                        bool save)
{
    vaults.createVault(QUrl::fromLocalFile(file(name)), password, password, keyfile, save);
    QTRY_COMPARE(vaults.vaultPath(), file(name));
    QTRY_COMPARE(vaults.state(), State::Unlocked);
    QTRY_VERIFY(vaults.isIdle());
}

void TestVaults::a_second_vault_is_opened_with_its_password_saved_in_the_primary()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    QVERIFY(vaults->canSaveInPrimary());
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    QCOMPARE(vaults->vaults(), QStringList({file("Home"), file("Work")}));
    QCOMPARE(vaults->primaryVault(), file("Home"));
    QVERIFY(!vaults->canSaveInPrimary()); // Work is shown, not the primary

    // Back to the primary: opened with the key kept from this session.
    vaults->switchToVault(file("Home"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Home"));
    QCOMPARE(vaults->items()->rowCount(), 1);
    const QVariantMap saved = vaults->item(vaults->items()->data(vaults->items()->index(0), ItemListModel::IdRole).toString());
    QCOMPARE(fieldOf(saved, QStringLiteral("vaultfile")), file("Work"));
    QCOMPARE(fieldOf(saved, QStringLiteral("password")), QStringLiteral("work pass"));
    QCOMPARE(saved.value(QStringLiteral("categoryId")).toString(), QStringLiteral("password"));

    // And to Work again: opened with the password saved in the primary.
    vaults->switchToVault(file("Work"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Work"));
}

void TestVaults::a_lock_locks_every_vault_and_the_primary_unlocks_them_again()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    QVERIFY(vaults->saveItem(vaults->newLogin()));
    QTRY_VERIFY(vaults->isIdle());

    vaults->lock();
    QCOMPARE(vaults->state(), State::Locked);
    QCOMPARE(vaults->vaultPath(), file("Home")); // the primary asks
    vaults->unlock(QStringLiteral("work pass"));
    QTRY_COMPARE(vaults->state(), State::Locked); // Work's own password is not the primary's
    vaults->unlock(QStringLiteral("home pass"));
    // The primary unlocks, then the vault that was shown comes back.
    QTRY_COMPARE(vaults->vaultPath(), file("Work"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->items()->rowCount(), 1);
}

void TestVaults::a_vault_whose_password_is_not_saved_asks_for_it()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Private"), QStringLiteral("own pass"), {}, false);
    vaults->switchToVault(file("Home"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->items()->rowCount(), 0); // nothing saved in the primary
    vaults->switchToVault(file("Private"));
    QTRY_COMPARE(vaults->state(), State::Locked);
    QCOMPARE(vaults->vaultPath(), file("Private"));
    vaults->unlock(QStringLiteral("own pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
}

void TestVaults::the_one_vault_of_an_older_version_becomes_the_primary()
{
    {
        auto vaults = controller();
        create(*vaults, QStringLiteral("Old"), QStringLiteral("pass"));
    }
    QSettings().remove(QStringLiteral("vaults/list")); // as 0.0.14 left it
    auto vaults = controller();
    QCOMPARE(vaults->vaults(), QStringList{file("Old")});
    QCOMPARE(vaults->primaryVault(), file("Old"));
    QCOMPARE(vaults->state(), State::Locked);
}

void TestVaults::a_vault_with_a_keyfile_needs_it_and_remembers_it()
{
    auto vaults = controller();
    const QString keyfile = folder->filePath(QStringLiteral("key.chronovaultkey"));
    QSignalSpy made(vaults.get(), &VaultController::keyfileMade);
    vaults->makeKeyfile(QUrl::fromLocalFile(keyfile));
    QTRY_COMPARE(made.size(), 1);
    QVERIFY(made.first().at(1).toBool());

    create(*vaults, QStringLiteral("Keyed"), QStringLiteral("pass"), QUrl::fromLocalFile(keyfile));
    QVERIFY(vaults->usesKeyfile());
    vaults->lock();
    QCOMPARE(vaults->keyfilePath(), keyfile);
    QVERIFY(vaults->keyfileNeeded());
    vaults->unlock(QStringLiteral("pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);

    // Not remembered (another computer): unlocking says the keyfile is needed.
    vaults->lock();
    vaults->setKeyfilePath(QString());
    QVERIFY(!vaults->keyfileNeeded());
    vaults->unlock(QStringLiteral("pass"));
    QTRY_VERIFY(vaults->keyfileNeeded());
    QCOMPARE(vaults->state(), State::Locked);
    QVERIFY(vaults->messageIsTrouble());

    // A keyfile that is gone is said.
    vaults->setKeyfilePath(QUrl::fromLocalFile(folder->filePath(QStringLiteral("gone"))).toString());
    vaults->unlock(QStringLiteral("pass"));
    QTRY_COMPARE(vaults->state(), State::Locked);
    QVERIFY(vaults->message().contains(QStringLiteral("keyfile")));
    vaults->setKeyfilePath(QUrl::fromLocalFile(keyfile).toString());
    vaults->unlock(QStringLiteral("pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
}

void TestVaults::the_master_password_and_keyfile_are_changed_only_with_the_current_password()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Mine"), QStringLiteral("old pass"));
    QSignalSpy changed(vaults.get(), &VaultController::masterPasswordChanged);

    vaults->changeMasterPassword(QStringLiteral("wrong"), QStringLiteral("new"), QStringLiteral("new"), {});
    QTRY_COMPARE(changed.size(), 1);
    QVERIFY(!changed.last().at(0).toBool());
    vaults->changeMasterPassword(QStringLiteral("old pass"), QStringLiteral("new"), QStringLiteral("other"), {});
    QTRY_COMPARE(changed.size(), 2);
    QVERIFY(!changed.last().at(0).toBool());

    // With a keyfile added, in a folder whose name a web address would misread.
    QVERIFY(QDir(folder->path()).mkdir(QStringLiteral("keys #1 100%")));
    const QString keyfile = folder->filePath(QStringLiteral("keys #1 100%/added.chronovaultkey"));
    QVERIFY(makeKeyfile(keyfile));
    vaults->changeMasterPassword(QStringLiteral("old pass"), QStringLiteral("new pass"), QStringLiteral("new pass"),
                                 QUrl::fromLocalFile(keyfile));
    QTRY_COMPARE(changed.size(), 3);
    QVERIFY(changed.last().at(0).toBool());
    QVERIFY(vaults->usesKeyfile());
    QCOMPARE(vaults->keyfilePath(), keyfile);
    QCOMPARE(vaults->keyfileUrl().toLocalFile(), keyfile); // as the window gives it back
    QVERIFY(vaults->saveItem(vaults->newLogin())); // later saves keep the new key
    QTRY_VERIFY(vaults->isIdle());

    vaults->lock();
    vaults->unlock(QStringLiteral("old pass"));
    QTRY_COMPARE(vaults->state(), State::Locked);
    vaults->unlock(QStringLiteral("new pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->items()->rowCount(), 1);

    // And the keyfile taken off again.
    vaults->changeMasterPassword(QStringLiteral("new pass"), QStringLiteral("third"), QStringLiteral("third"), {});
    QTRY_COMPARE(changed.size(), 4);
    QVERIFY(changed.last().at(0).toBool());
    QVERIFY(!vaults->usesKeyfile());
    vaults->lock();
    QVERIFY(!vaults->keyfileNeeded());
    vaults->unlock(QStringLiteral("third"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
}

void TestVaults::a_primary_with_a_keyfile_takes_its_password_change_after_a_switch()
{
    auto vaults = controller();
    const QString keyfile = folder->filePath(QStringLiteral("home.chronovaultkey"));
    QVERIFY(makeKeyfile(keyfile));
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"), QUrl::fromLocalFile(keyfile));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    vaults->switchToVault(file("Home"));
    QTRY_COMPARE(vaults->vaultPath(), file("Home"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QSignalSpy changed(vaults.get(), &VaultController::masterPasswordChanged);
    vaults->changeMasterPassword(QStringLiteral("home pass"), QStringLiteral("new"), QStringLiteral("new"),
                                 QUrl::fromLocalFile(keyfile));
    QTRY_COMPARE(changed.size(), 1);
    QVERIFY(changed.first().at(0).toBool());
}

void TestVaults::a_password_not_saved_in_the_primary_is_said_there_and_the_vault_stays_listed()
{
#ifdef Q_OS_WIN
    QSKIP("A folder cannot be made read-only for its owner on Windows.");
#else
    auto vaults = controller();
    QVERIFY(QDir(folder->path()).mkdir(QStringLiteral("primary")));
    const QString primaryFolder = folder->filePath(QStringLiteral("primary"));
    const QString home = primaryFolder + QStringLiteral("/Home.chronovault");
    vaults->createVault(QUrl::fromLocalFile(home), QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    // The primary's folder can no longer be written to: its save fails.
    QVERIFY(QFile::setPermissions(primaryFolder, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    vaults->createVault(QUrl::fromLocalFile(file("Work")), QStringLiteral("b"), QStringLiteral("b"), QUrl(), true);
    QTRY_VERIFY(vaults->isIdle());
    QTRY_VERIFY(vaults->message().contains(QStringLiteral("Work")));
    QVERIFY(vaults->messageIsTrouble());
    QCOMPARE(vaults->vaultPath(), home); // the failure is said on the primary, where it can be tried again
    QVERIFY(vaults->saveFailed());
    QVERIFY(vaults->vaults().contains(file("Work")));
    QVERIFY(QFile::exists(file("Work")));
    QVERIFY(QFile::setPermissions(primaryFolder, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                     QFileDevice::ExeOwner));
    vaults->retrySave();
    QTRY_VERIFY(vaults->isIdle());
    QVERIFY(!vaults->saveFailed());
#endif
}

void TestVaults::a_changed_password_is_changed_in_the_primary_too()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    QSignalSpy changed(vaults.get(), &VaultController::masterPasswordChanged);
    vaults->changeMasterPassword(QStringLiteral("work pass"), QStringLiteral("new work"), QStringLiteral("new work"),
                                 {});
    QTRY_COMPARE(changed.size(), 1);
    QVERIFY(changed.first().at(0).toBool());
    QTRY_VERIFY(vaults->isIdle());
    QVERIFY(!vaults->messageIsTrouble());

    // A new session: the primary's saved password opens Work.
    vaults->lock();
    vaults.reset();
    auto again = controller();
    QCOMPARE(again->vaultPath(), file("Home"));
    again->unlock(QStringLiteral("home pass"));
    QTRY_COMPARE(again->state(), State::Unlocked);
    again->switchToVault(file("Work"));
    QTRY_COMPARE(again->vaultPath(), file("Work"));
    QTRY_COMPARE(again->state(), State::Unlocked);
}

void TestVaults::a_new_keyfile_is_never_made_over_a_file()
{
    auto vaults = controller();
    const QString keyfile = folder->filePath(QStringLiteral("key"));
    QVERIFY(makeKeyfile(keyfile));
    const auto before = keyfileFingerprint(keyfile);
    QSignalSpy made(vaults.get(), &VaultController::keyfileMade);
    vaults->makeKeyfile(QUrl::fromLocalFile(keyfile));
    QTRY_COMPARE(made.size(), 1);
    QVERIFY(!made.first().at(1).toBool());
    QCOMPARE(keyfileFingerprint(keyfile), before);
}

void TestVaults::a_stale_saved_password_and_a_primary_changed_elsewhere_are_said()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);

    // Work's password changed by another Chrono Vault: the saved one is stale.
    {
        const OpenResult opened = openVaultFile(file("Work"), "work pass");
        QCOMPARE(opened.status, OpenStatus::Opened);
        QCOMPARE(saveVaultFile(file("Work"), opened.contents, makeVaultKey("elsewhere", quick)), SaveStatus::Saved);
    }
    vaults->switchToVault(file("Home"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    vaults->switchToVault(file("Work"));
    QTRY_COMPARE(vaults->state(), State::Locked);
    QTRY_VERIFY(vaults->message().contains(QStringLiteral("Home")));
    vaults->unlock(QStringLiteral("elsewhere"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);

    // Home's password changed elsewhere: the kept key no longer opens it.
    {
        const OpenResult opened = openVaultFile(file("Home"), "home pass");
        QCOMPARE(opened.status, OpenStatus::Opened);
        QCOMPARE(saveVaultFile(file("Home"), opened.contents, makeVaultKey("other", quick)), SaveStatus::Saved);
    }
    vaults->switchToVault(file("Home"));
    QTRY_COMPARE(vaults->state(), State::Locked);
    QTRY_VERIFY(vaults->messageIsTrouble());
    QVERIFY(vaults->message().contains(QStringLiteral("changed elsewhere")));

    // The primary's file away for a while: it stays the primary.
    QVERIFY(QFile::rename(file("Home"), file("Home") + QStringLiteral(".away")));
    QCOMPARE(vaults->primaryVault(), file("Home"));
}

QTEST_MAIN(TestVaults)
#include "test_vaults.moc"
