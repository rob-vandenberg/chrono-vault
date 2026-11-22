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
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_elsewhere.cpp 0.0.16

// --- Version history ----------------------------------------------------
// v0.0.16: Initial release: a change made elsewhere to the vault file (D-032)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};

// Another Chrono Vault, on another computer, adds an item to the vault.
void addElsewhere(const QString& vaultFile, const QByteArray& password, const QString& title)
{
    const OpenResult opened = openVaultFile(vaultFile, password);
    QCOMPARE(opened.status, OpenStatus::Opened);
    ContentsResult read = contentsFromJson(opened.contents);
    QCOMPARE(read.status, ContentsStatus::Read);
    Item item;
    item.id = QUuid::createUuid();
    item.title = title;
    read.contents.items.append(item);
    QCOMPARE(saveVaultFile(vaultFile, contentsToJson(read.contents), opened.key), SaveStatus::Saved);
}

QStringList titlesIn(const QString& vaultFile, const QByteArray& password)
{
    const OpenResult opened = openVaultFile(vaultFile, password);
    QStringList titles;
    for (const Item& item : contentsFromJson(opened.contents).contents.items) {
        titles << item.title;
    }
    titles.sort();
    return titles;
}

QStringList shownTitles(VaultController& controller)
{
    QStringList titles;
    for (int row = 0; row < controller.items()->rowCount(); ++row) {
        titles << controller.items()->data(controller.items()->index(row), ItemListModel::TitleRole).toString();
    }
    return titles;
}

QString conflictOf(const VaultController& controller)
{
    for (const QString& one : controller.vaults()) {
        if (one.contains(QStringLiteral("(conflict "))) {
            return one;
        }
    }
    return {};
}

} // namespace

// Another computer changed the vault file: read again when nothing waits
// here, both versions kept when something does.
class TestElsewhere : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void a_change_made_elsewhere_is_read_again();
    void a_change_here_and_one_elsewhere_are_both_kept();
    void a_change_kept_through_a_lock_never_goes_over_one_made_elsewhere();
    void a_vault_given_another_password_elsewhere_locks_and_says_so();
    void a_conflict_copy_that_cannot_be_written_never_lets_a_save_go_over_the_other_version();
    void a_version_that_cannot_be_read_yet_keeps_the_vault_open();
    void a_conflict_with_a_version_not_readable_yet_never_lets_a_save_go_over_it();

private:
    std::unique_ptr<VaultController> unlocked(const QString& vaultFile);

    QTemporaryDir* folder = nullptr;
    QString vaultPath;
};

void TestElsewhere::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_elsewhere"));
}

void TestElsewhere::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestElsewhere::init()
{
    QSettings().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
    vaultPath = folder->filePath(QStringLiteral("Shared.chronovault"));
}

void TestElsewhere::cleanup()
{
    QSettings().clear();
    delete folder;
    folder = nullptr;
}

std::unique_ptr<VaultController> TestElsewhere::unlocked(const QString& vaultFile)
{
    auto controller = std::make_unique<VaultController>();
    controller->setKdfParameters(quick);
    controller->createVault(QUrl::fromLocalFile(vaultFile), QStringLiteral("pass"), QStringLiteral("pass"));
    if (!QTest::qWaitFor([&controller]() { return controller->state() == State::Unlocked; }, 10000)) {
        return nullptr;
    }
    return controller;
}

void TestElsewhere::a_change_made_elsewhere_is_read_again()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QVariantMap mine = controller->newLogin();
    mine.insert(QStringLiteral("title"), QStringLiteral("Mine"));
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->isIdle());

    addElsewhere(vaultPath, "pass", QStringLiteral("Theirs"));
    QTRY_COMPARE(shownTitles(*controller), QStringList({"Mine", "Theirs"}));
    QVERIFY(controller->message().contains(QStringLiteral("changed elsewhere")));
    QVERIFY(conflictOf(*controller).isEmpty());

    // Saving after that keeps both items.
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(titlesIn(vaultPath, "pass"), QStringList({"Mine", "Theirs"}));
}

void TestElsewhere::a_change_here_and_one_elsewhere_are_both_kept()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    // Changed elsewhere, and here before this Chrono Vault heard of it.
    addElsewhere(vaultPath, "pass", QStringLiteral("Theirs"));
    QVariantMap mine = controller->newLogin();
    mine.insert(QStringLiteral("title"), QStringLiteral("Mine"));
    QVERIFY(controller->saveItem(mine));

    QTRY_VERIFY(!conflictOf(*controller).isEmpty());
    const QString conflict = conflictOf(*controller);
    QTRY_COMPARE(shownTitles(*controller), QStringList{QStringLiteral("Theirs")});
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(controller->message().contains(QFileInfo(conflict).completeBaseName()));
    QCOMPARE(titlesIn(vaultPath, "pass"), QStringList{QStringLiteral("Theirs")});
    QCOMPARE(titlesIn(conflict, "pass"), QStringList{QStringLiteral("Mine")});
    QVERIFY(!controller->unsavedChanges());
}

void TestElsewhere::a_change_kept_through_a_lock_never_goes_over_one_made_elsewhere()
{
#ifdef Q_OS_WIN
    QSKIP("A folder cannot be made read-only for its owner on Windows.");
#else
    QVERIFY(QDir(folder->path()).mkdir(QStringLiteral("shared")));
    const QString sharedFolder = folder->filePath(QStringLiteral("shared"));
    const QString vaultFile = sharedFolder + QStringLiteral("/Shared.chronovault");
    auto controller = unlocked(vaultFile);
    QVERIFY(controller);
    // A save that fails, kept through the lock.
    QVERIFY(QFile::setPermissions(sharedFolder, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QVariantMap mine = controller->newLogin();
    mine.insert(QStringLiteral("title"), QStringLiteral("Mine"));
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->saveFailed());
    controller->lock();
    QVERIFY(controller->hasKeptChanges());
    QVERIFY(QFile::setPermissions(sharedFolder, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                    QFileDevice::ExeOwner));
    // Meanwhile another computer changes the vault.
    addElsewhere(vaultFile, "pass", QStringLiteral("Theirs"));

    controller->unlock(QStringLiteral("pass"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QTRY_VERIFY(!conflictOf(*controller).isEmpty());
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(shownTitles(*controller), QStringList{QStringLiteral("Theirs")});
    QCOMPARE(titlesIn(vaultFile, "pass"), QStringList{QStringLiteral("Theirs")});
    QCOMPARE(titlesIn(conflictOf(*controller), "pass"), QStringList{QStringLiteral("Mine")});
#endif
}

void TestElsewhere::a_vault_given_another_password_elsewhere_locks_and_says_so()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QTRY_VERIFY(controller->isIdle());
    const OpenResult opened = openVaultFile(vaultPath, "pass");
    QCOMPARE(saveVaultFile(vaultPath, opened.contents, makeVaultKey("other", quick)), SaveStatus::Saved);
    QTRY_COMPARE(controller->state(), State::Locked);
    QVERIFY(controller->messageIsTrouble());
    controller->unlock(QStringLiteral("other"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
}

void TestElsewhere::a_conflict_copy_that_cannot_be_written_never_lets_a_save_go_over_the_other_version()
{
#ifdef Q_OS_WIN
    QSKIP("A folder cannot be made read-only for its owner on Windows.");
#else
    QVERIFY(QDir(folder->path()).mkdir(QStringLiteral("shared")));
    const QString sharedFolder = folder->filePath(QStringLiteral("shared"));
    const QString vaultFile = sharedFolder + QStringLiteral("/Shared.chronovault");
    auto controller = unlocked(vaultFile);
    QVERIFY(controller);
    addElsewhere(vaultFile, "pass", QStringLiteral("Theirs"));
    // The folder is full, say: neither the copy nor the vault can be written.
    QVERIFY(QFile::setPermissions(sharedFolder, QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    QVariantMap mine = controller->newLogin();
    mine.insert(QStringLiteral("title"), QStringLiteral("Mine"));
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->saveFailed());
    QVERIFY(QFile::setPermissions(sharedFolder, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                    QFileDevice::ExeOwner));
    // Tried again: still both kept, the other version never written over.
    controller->retrySave();
    QTRY_VERIFY(!conflictOf(*controller).isEmpty());
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(titlesIn(vaultFile, "pass"), QStringList{QStringLiteral("Theirs")});
    QCOMPARE(titlesIn(conflictOf(*controller), "pass"), QStringList{QStringLiteral("Mine")});
#endif
}

void TestElsewhere::a_version_that_cannot_be_read_yet_keeps_the_vault_open()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QTRY_VERIFY(controller->isIdle());
    const OpenResult before = openVaultFile(vaultPath, "pass");
    QCOMPARE(before.status, OpenStatus::Opened);
    // Half written by a cloud folder: the start of a new version only.
    QFile file(vaultPath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QByteArray start = file.read(60);
    start[45] = static_cast<char>(start.at(45) ^ 0x5a); // another nonce: another save
    QVERIFY(file.resize(0));
    QCOMPARE(file.write(start), start.size());
    file.close();
    QTRY_VERIFY(controller->message().contains(QStringLiteral("cannot be read yet")));
    QCOMPARE(controller->state(), State::Unlocked);
    // Once written whole, it is read.
    VaultContents whole;
    Item theirs;
    theirs.id = QUuid::createUuid();
    theirs.title = QStringLiteral("Theirs");
    whole.items << theirs;
    QCOMPARE(saveVaultFile(vaultPath, contentsToJson(whole), before.key), SaveStatus::Saved);
    QTRY_COMPARE(shownTitles(*controller), QStringList{QStringLiteral("Theirs")});
    QCOMPARE(controller->state(), State::Unlocked);
}

void TestElsewhere::a_conflict_with_a_version_not_readable_yet_never_lets_a_save_go_over_it()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QTRY_VERIFY(controller->isIdle());
    // Half written elsewhere, and changed here before this Chrono Vault heard of it.
    QFile file(vaultPath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QByteArray start = file.read(60);
    start[45] = static_cast<char>(start.at(45) ^ 0x5a);
    QVERIFY(file.resize(0));
    QCOMPARE(file.write(start), start.size());
    file.close();
    QVariantMap mine = controller->newLogin();
    mine.insert(QStringLiteral("title"), QStringLiteral("Mine"));
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(!conflictOf(*controller).isEmpty());
    QTRY_VERIFY(controller->message().contains(QStringLiteral("cannot be read yet")));
    QTRY_VERIFY(controller->isIdle());

    // The next change here goes into a copy again, never over the other version.
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->isIdle());
    QFile again(vaultPath);
    QVERIFY(again.open(QIODevice::ReadOnly));
    QCOMPARE(again.readAll(), start);
}

QTEST_MAIN(TestElsewhere)
#include "test_elsewhere.moc"
