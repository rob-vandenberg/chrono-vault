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
#include "settings_files.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vaults.cpp 0.7.110

// --- Version history ----------------------------------------------------
// v0.7.110: A kept vault replaces the open one with no page between; a failure keeps it (D-078)
// v0.7.109: A Closed vault picked as a file opens at once
// v0.6.106: Close and Lock per vault (D-075)
// v0.3.64: A lock keeps the vault used last; its own password opens it (D-058)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
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
    void vaults_not_found_are_hidden_and_looked_for_again();
    void close_and_lock_work_on_one_vault();
    void a_lock_without_an_open_vault_locks_the_key_store();
    void a_password_that_cannot_be_removed_leaves_the_vault_closed();
    void a_start_and_a_missing_primary_show_the_right_vault();
    void a_password_typed_on_the_start_up_page_is_remembered();
    void a_closed_vault_picked_as_a_file_opens_at_once();
    void a_vault_whose_password_is_not_saved_asks_for_it();
    void the_one_vault_of_an_older_version_becomes_the_primary();
    void a_vault_with_a_keyfile_needs_it_and_remembers_it();
    void the_master_password_and_keyfile_are_changed_only_with_the_current_password();
    void a_primary_with_a_keyfile_takes_its_password_change_after_a_switch();
    void a_password_not_saved_in_the_primary_is_said_there_and_the_vault_stays_listed();
    void a_changed_password_is_changed_in_the_primary_too();
    void a_new_keyfile_is_never_made_over_a_file();
    void a_stale_saved_password_and_a_primary_changed_elsewhere_are_said();
    void a_kept_vault_replaces_the_open_one_with_no_page_between();

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
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestVaults::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
}

void TestVaults::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
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
    QTRY_VERIFY(!vaults->isSwitching());
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Home"));
    QCOMPARE(vaults->items()->rowCount(), 1);
    const QVariantMap saved = vaults->item(vaults->items()->data(vaults->items()->index(0), ItemListModel::IdRole).toString());
    QCOMPARE(fieldOf(saved, QStringLiteral("vaultfile")), file("Work"));
    QCOMPARE(fieldOf(saved, QStringLiteral("password")), QStringLiteral("work pass"));
    QCOMPARE(saved.value(QStringLiteral("categoryId")).toString(), QStringLiteral("password"));

    // And to Work again: opened with the password saved in the primary.
    vaults->switchToVault(file("Work"));
    QTRY_VERIFY(!vaults->isSwitching());
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
    // Opened out of the primary: the primary's password, and back in Work,
    // the primary's items never shown (D-075).
    QCOMPARE(vaults->vaultPath(), file("Home"));
    QCOMPARE(vaults->returnVault(), file("Work"));
    bool primaryShown = false;
    const auto watching = connect(vaults.get(), &VaultController::stateChanged, this, [&]() {
        primaryShown = primaryShown || (vaults->state() == State::Unlocked && vaults->vaultPath() == file("Home"));
    });
    vaults->unlock(QStringLiteral("home pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    disconnect(watching);
    QVERIFY(!primaryShown);
    QCOMPARE(vaults->vaultPath(), file("Work"));
    QCOMPARE(vaults->items()->rowCount(), 1);
    QVERIFY(vaults->returnVault().isEmpty() && vaults->keyStoreOpen());
    // Work picked instead: its own password; from then on it is asked again,
    // since the primary was skipped.
    vaults->lock();
    vaults->switchToVault(file("Work"));
    QVERIFY(vaults->returnVault().isEmpty());
    vaults->unlock(QStringLiteral("home pass"));
    QTRY_COMPARE(vaults->state(), State::Locked); // the primary's password is not Work's
    vaults->unlock(QStringLiteral("work pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QVERIFY(!vaults->keyStoreOpen());
    vaults->lock();
    QCOMPARE(vaults->vaultPath(), file("Work"));
    QVERIFY(vaults->returnVault().isEmpty());
    vaults->unlock(QStringLiteral("work pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Work"));
    QCOMPARE(vaults->items()->rowCount(), 1);
    // Locked again, the primary chosen and unlocked opens Work with the
    // password saved in it.
    vaults->lock();
    vaults->switchToVault(file("Home"));
    QCOMPARE(vaults->vaultPath(), file("Home"));
    vaults->unlock(QStringLiteral("home pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    vaults->switchToVault(file("Work"));
    QTRY_COMPARE(vaults->vaultPath(), file("Work"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
}

void TestVaults::vaults_not_found_are_hidden_and_looked_for_again()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    vaults->lock();
    QCOMPARE(vaults->vaultPath(), file("Home"));
    vaults->switchToVault(file("Work"));
    // The vault chosen is gone (its drive not connected): the primary is
    // chosen, the last used kept to be shown struck through.
    QVERIFY(QFile::rename(file("Work"), file("Work") + QStringLiteral(".away")));
    vaults->checkVaults();
    QTRY_COMPARE(vaults->vaultPath(), file("Home"));
    QCOMPARE(vaults->missingVault(), file("Work"));
    QCOMPARE(vaults->foundVaults(), QStringList{file("Home")});
    // No vault at all: the start page, saying so.
    QVERIFY(QFile::rename(file("Home"), file("Home") + QStringLiteral(".away")));
    vaults->checkVaults();
    QTRY_COMPARE(vaults->state(), State::NoVault);
    QVERIFY(vaults->noVaultFound());
    QCOMPARE(vaults->vaultPath(), QString());
    // Back: the vault used last comes back by itself at the next look.
    QVERIFY(QFile::rename(file("Home") + QStringLiteral(".away"), file("Home")));
    QVERIFY(QFile::rename(file("Work") + QStringLiteral(".away"), file("Work")));
    vaults->checkVaults();
    QTRY_COMPARE(vaults->state(), State::Locked);
    QTRY_COMPARE(vaults->vaultPath(), file("Work"));
    QVERIFY(!vaults->noVaultFound());
    QCOMPARE(vaults->missingVault(), QString());
}

void TestVaults::close_and_lock_work_on_one_vault()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    // Close: only the open vault; the page begins with the primary, Closed.
    vaults->lockVault(file("Work")); // as a door: closed first
    QCOMPARE(vaults->state(), State::Unlocked);
    vaults->closeOpenVault(file("Work"));
    QCOMPARE(vaults->state(), State::Locked);
    QCOMPARE(vaults->vaultPath(), file("Home"));
    QCOMPARE(vaults->vaultStateOf(file("Work")), 1);
    QCOMPARE(vaults->vaultStateOf(file("Home")), 1);
    QVERIFY(vaults->keyStoreOpen());
    vaults->switchToVault(file("Home")); // opens without a password
    QTRY_COMPARE(vaults->state(), State::Unlocked);

    // Lock on a Closed one, the primary open: its password leaves the primary,
    // here and on the disk; the item stays.
    const QString savedId = vaults->items()->data(vaults->items()->index(0), ItemListModel::IdRole).toString();
    vaults->lockVault(file("Work"));
    QTRY_COMPARE(vaults->vaultStateOf(file("Work")), 0);
    QCOMPARE(fieldOf(vaults->item(savedId), QStringLiteral("password")), QString());
    QCOMPARE(fieldOf(vaults->item(savedId), QStringLiteral("vaultfile")), file("Work"));
    QVERIFY(vaults->saveItem(vaults->newLogin())); // a later save does not bring it back
    QTRY_VERIFY(vaults->isIdle());
    vaults->lock();
    vaults->unlock(QStringLiteral("home pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->items()->rowCount(), 2);
    QCOMPARE(vaults->vaultStateOf(file("Work")), 0);

    // An open vault without a kept password: Lock locks only that one, and a
    // lock of everything asks its own password.
    vaults->switchToVault(file("Work"));
    vaults->unlock(QStringLiteral("work pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    vaults->lockVault(file("Work"));
    QCOMPARE(vaults->state(), State::Locked);
    QCOMPARE(vaults->vaultPath(), file("Home"));
    QCOMPARE(vaults->vaultStateOf(file("Home")), 1);
    vaults->switchToVault(file("Work"));
    vaults->unlock(QStringLiteral("work pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    // Lock on the Closed primary: an open vault stays open.
    vaults->lockVault(file("Home"));
    QCOMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Work"));
    QCOMPARE(vaults->vaultStateOf(file("Home")), 0);
    vaults->lock();
    QCOMPARE(vaults->vaultPath(), file("Work"));
}

void TestVaults::a_lock_without_an_open_vault_locks_the_key_store()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    vaults->closeOpenVault(file("Work"));
    QVERIFY(vaults->keyStoreOpen());
    vaults->lock();
    QVERIFY(!vaults->keyStoreOpen());
    QCOMPARE(vaults->vaultStateOf(file("Work")), 0);
    QCOMPARE(vaults->vaultStateOf(file("Home")), 0);
    // Nothing was deleted.
    vaults->unlock(QStringLiteral("home pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultStateOf(file("Work")), 1);
    // Autolock as the window closes, with no vault open.
    vaults->closeOpenVault(file("Home"));
    vaults->setLockWhenClosed(true);
    vaults->mainWindowClosed();
    QVERIFY(!vaults->keyStoreOpen());
}

void TestVaults::a_password_that_cannot_be_removed_leaves_the_vault_closed()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    vaults->closeOpenVault(file("Work"));
    QSignalSpy problem(vaults.get(), &VaultController::vaultMenuProblem);
    QVERIFY(QFile::rename(file("Home"), file("Home") + QStringLiteral(".away")));
    vaults->lockVault(file("Work"));
    QTRY_COMPARE(problem.size(), 1);
    QCOMPARE(vaults->vaultStateOf(file("Work")), 1);
    QVERIFY(QFile::rename(file("Home") + QStringLiteral(".away"), file("Home")));
    vaults->lockVault(file("Work"));
    QTRY_COMPARE(vaults->vaultStateOf(file("Work")), 0);
    QCOMPARE(problem.size(), 1);
}

void TestVaults::a_start_and_a_missing_primary_show_the_right_vault()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    create(*vaults, QStringLiteral("Last"), QStringLiteral("last pass"));
    vaults.reset();
    auto again = controller();
    QCOMPARE(again->vaultPath(), file("Home"));
    QCOMPARE(again->lastUsedVault(), file("Last"));
    QVERIFY(again->returnVault().isEmpty());
    again.reset();
    QVERIFY(QFile::rename(file("Home"), file("Home") + QStringLiteral(".away")));
    auto without = controller();
    QCOMPARE(without->vaultPath(), file("Last"));
    without->unlock(QStringLiteral("last pass"));
    QTRY_COMPARE(without->state(), State::Unlocked);
    without->lock();
    QCOMPARE(without->vaultPath(), file("Last"));
}

void TestVaults::a_password_typed_on_the_start_up_page_is_remembered()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"));
    vaults->switchToVault(file("Home"));
    QTRY_VERIFY(!vaults->isSwitching());
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    vaults->closeOpenVault(file("Home"));
    vaults->switchToVault(file("Work"));
    QCOMPARE(vaults->state(), State::Locked);
    vaults->unlock(QStringLiteral("wrong"), true);
    QTRY_COMPARE(vaults->state(), State::Locked);
    vaults->unlock(QStringLiteral("work pass"), true);
    QTRY_VERIFY(vaults->state() == State::Unlocked && vaults->isKept(file("Work")));
    // Remembered: a lock asks the primary's password, and comes back (Q253).
    vaults->lock();
    QCOMPARE(vaults->vaultPath(), file("Home"));
    vaults->unlock(QStringLiteral("home pass"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Work"));
}

void TestVaults::a_closed_vault_picked_as_a_file_opens_at_once()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    create(*vaults, QStringLiteral("Own"), QStringLiteral("own pass"));
    vaults->switchToVault(file("Home"));
    QTRY_VERIFY(!vaults->isSwitching());
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    // The open one: nothing changes.
    vaults->chooseVault(QUrl::fromLocalFile(file("Home")));
    QCOMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Home"));
    // A Closed one: open at once, without the start-up page.
    vaults->chooseVault(QUrl::fromLocalFile(file("Work")));
    QTRY_VERIFY(vaults->vaultPath() == file("Work") && vaults->state() == State::Unlocked && !vaults->isSwitching());
    // One whose password is not kept: asked.
    vaults->chooseVault(QUrl::fromLocalFile(file("Own")));
    QCOMPARE(vaults->state(), State::Locked);
    QCOMPARE(vaults->vaultPath(), file("Own"));
}

void TestVaults::a_vault_whose_password_is_not_saved_asks_for_it()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Private"), QStringLiteral("own pass"), {}, false);
    vaults->switchToVault(file("Home"));
    QTRY_VERIFY(!vaults->isSwitching());
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
    VaultsFile().remove(QStringLiteral("vaults/list")); // as 0.0.14 left it
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
    QCOMPARE(again->lastUsedVault(), file("Work"));
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
    QTRY_VERIFY(!vaults->isSwitching());
    QTRY_COMPARE(vaults->state(), State::Unlocked);
    // It fails: Home stays open, and the reason names both (D-078).
    QSignalSpy failed(vaults.get(), &VaultController::switchFailed);
    vaults->switchToVault(file("Work"));
    QTRY_COMPARE(failed.size(), 1);
    QCOMPARE(failed.first().at(0).toString(), file("Work"));
    QCOMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Home"));
    QVERIFY(!vaults->isSwitching());
    QVERIFY(vaults->messageIsTrouble());
    QVERIFY(vaults->message().contains(QStringLiteral("“Home”")) && vaults->message().contains(QStringLiteral("“Work”")));
    QVERIFY(!vaults->isKept(file("Work")));
    vaults->switchToVault(file("Work")); // now asked
    QCOMPARE(vaults->state(), State::Locked);
    vaults->unlock(QStringLiteral("elsewhere"));
    QTRY_COMPARE(vaults->state(), State::Unlocked);

    // Home's password changed elsewhere: the kept key no longer opens it.
    {
        const OpenResult opened = openVaultFile(file("Home"), "home pass");
        QCOMPARE(opened.status, OpenStatus::Opened);
        QCOMPARE(saveVaultFile(file("Home"), opened.contents, makeVaultKey("other", quick)), SaveStatus::Saved);
    }
    vaults->switchToVault(file("Home"));
    QTRY_COMPARE(failed.size(), 2);
    QCOMPARE(vaults->state(), State::Unlocked);
    QCOMPARE(vaults->vaultPath(), file("Work"));
    QVERIFY(vaults->messageIsTrouble());
    QVERIFY(vaults->message().contains(QStringLiteral("changed elsewhere")));

    // The primary's file away for a while: it stays the primary.
    QVERIFY(QFile::rename(file("Home"), file("Home") + QStringLiteral(".away")));
    QCOMPARE(vaults->primaryVault(), file("Home"));
}

void TestVaults::a_kept_vault_replaces_the_open_one_with_no_page_between()
{
    auto vaults = controller();
    create(*vaults, QStringLiteral("Home"), QStringLiteral("home pass"));
    create(*vaults, QStringLiteral("Work"), QStringLiteral("work pass"), {}, true);
    for (const QString& to : {file("Home"), file("Work")}) {
        const QString from = vaults->vaultPath();
        // Each change of state, and whether the event loop ran (a frame could
        // have been drawn) since the one before.
        QList<State> seen;
        bool loopRan = false;
        bool drawnBetween = false;
        const QMetaObject::Connection watching = connect(vaults.get(), &VaultController::stateChanged, this, [&]() {
            if (!seen.isEmpty() && loopRan) {
                drawnBetween = true;
            }
            seen.append(vaults->state());
            loopRan = false;
            QTimer::singleShot(0, this, [&loopRan]() { loopRan = true; });
        });
        vaults->switchToVault(to);
        // Meanwhile the open vault stays as it is.
        QVERIFY(vaults->isSwitching());
        QCOMPARE(vaults->state(), State::Unlocked);
        QCOMPARE(vaults->vaultPath(), from);
        QTRY_VERIFY(!vaults->isSwitching());
        QCOMPARE(vaults->vaultPath(), to);
        QCOMPARE(seen, QList<State>({State::Busy, State::Unlocked}));
        QVERIFY(!drawnBetween);
        disconnect(watching);
        QTest::qWait(1); // the timers above end while their captures live
    }
}

QTEST_MAIN(TestVaults)
#include "test_vaults.moc"
