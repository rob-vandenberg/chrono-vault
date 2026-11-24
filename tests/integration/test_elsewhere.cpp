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
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_elsewhere.cpp 0.5.81

// --- Version history ----------------------------------------------------
// v0.5.81: What an item deleted elsewhere shows (D-061)
// v0.5.78: This device's value stands; the sides left and right (D-061)
// v0.5.76: A conflict settled as the Sync conflicts dialog chooses (D-060)
// v0.2.45: A change here and one elsewhere merged, no longer a conflict copy (D-054)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
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

// Another Chrono Vault changes the title of an item, at a later moment.
void retitleElsewhere(const QString& vaultFile, const QByteArray& password, const QString& from, const QString& to)
{
    const OpenResult opened = openVaultFile(vaultFile, password);
    ContentsResult read = contentsFromJson(opened.contents);
    for (Item& item : read.contents.items) {
        if (item.title == from) {
            item.title = to;
            item.modified = QDateTime::currentDateTimeUtc().addSecs(60);
        }
    }
    QCOMPARE(saveVaultFile(vaultFile, contentsToJson(read.contents), opened.key), SaveStatus::Saved);
}

// Another device deletes the item with this title, a minute ago.
void deleteElsewhere(const QString& vaultFile, const QByteArray& password, const QString& title)
{
    const OpenResult opened = openVaultFile(vaultFile, password);
    ContentsResult read = contentsFromJson(opened.contents);
    for (qsizetype index = 0; index < read.contents.items.size(); ++index) {
        if (read.contents.items[index].title == title) {
            read.contents.deletedItems.append(
                DeletedItem{read.contents.items[index].id, QDateTime::currentDateTimeUtc().addSecs(-60)});
            read.contents.items.removeAt(index);
            break;
        }
    }
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
    void the_same_text_changed_here_and_elsewhere_is_noted_and_can_be_swapped();
    void a_conflict_is_settled_as_the_sync_conflicts_dialog_chooses_data();
    void a_conflict_is_settled_as_the_sync_conflicts_dialog_chooses();
    void an_item_deleted_elsewhere_goes_to_the_trash_with_use_other_value();
    void a_deletion_here_stays_after_a_merge();
    void a_change_kept_through_a_lock_never_goes_over_one_made_elsewhere();
    void a_vault_given_another_password_elsewhere_locks_and_says_so();
    void a_merge_that_cannot_be_written_is_tried_again_and_keeps_both();
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
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestElsewhere::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
    vaultPath = folder->filePath(QStringLiteral("Shared.chronovault"));
}

void TestElsewhere::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
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
    QTRY_VERIFY(controller->isIdle());
    // Changed elsewhere, and here before this Chrono Vault heard of it: both
    // versions are merged (D-054), with no conflict copy.
    addElsewhere(vaultPath, "pass", QStringLiteral("Theirs"));
    QVariantMap mine = controller->newLogin();
    mine.insert(QStringLiteral("title"), QStringLiteral("Mine"));
    QVERIFY(controller->saveItem(mine));

    QTRY_COMPARE(titlesIn(vaultPath, "pass"), QStringList({"Mine", "Theirs"}));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(shownTitles(*controller), QStringList({"Mine", "Theirs"}));
    QVERIFY(controller->message().contains(QStringLiteral("merged")));
    QVERIFY(conflictOf(*controller).isEmpty());
    QVERIFY(!controller->unsavedChanges());
}

void TestElsewhere::the_same_text_changed_here_and_elsewhere_is_noted_and_can_be_swapped()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QVariantMap item = controller->newLogin();
    item.insert(QStringLiteral("title"), QStringLiteral("Bank"));
    QVERIFY(controller->saveItem(item));
    QTRY_VERIFY(controller->isIdle());
    const QString id = item.value(QStringLiteral("id")).toString();

    // Elsewhere "The bank", a minute later than "My bank" here: this
    // device's stands all the same, the other waits for the person (D-061).
    retitleElsewhere(vaultPath, "pass", QStringLiteral("Bank"), QStringLiteral("The bank"));
    QVariantMap mine = controller->item(id);
    mine.insert(QStringLiteral("title"), QStringLiteral("My bank"));
    QVERIFY(controller->saveItem(mine));

    QTRY_COMPARE(titlesIn(vaultPath, "pass"), QStringList{QStringLiteral("My bank")});
    QTRY_VERIFY(controller->isIdle());
    QVERIFY(controller->messageIsTrouble());
    QCOMPARE(controller->items()->data(controller->items()->index(0), ItemListModel::MergedRole).toBool(), true);
    const QVariantList notes = controller->mergeNotes(id);
    QCOMPARE(notes.size(), 1);
    QCOMPARE(notes[0].toMap().value(QStringLiteral("otherValue")).toString(), QStringLiteral("The bank"));

    // "Use the other value" puts it in place, and the note goes.
    controller->useOtherValue(id, 0);
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(titlesIn(vaultPath, "pass"), QStringList{QStringLiteral("The bank")});
    QVERIFY(controller->mergeNotes(id).isEmpty());
    QCOMPARE(controller->items()->data(controller->items()->index(0), ItemListModel::MergedRole).toBool(), false);
}

void TestElsewhere::a_conflict_is_settled_as_the_sync_conflicts_dialog_chooses_data()
{
    QTest::addColumn<QString>("way");
    QTest::addColumn<QStringList>("titles");
    QTest::newRow("this device") << QStringLiteral("left") << QStringList{QStringLiteral("My bank")};
    QTest::newRow("other device") << QStringLiteral("right") << QStringList{QStringLiteral("The bank")};
    QTest::newRow("keep both") << QStringLiteral("both") << QStringList{QStringLiteral("My bank"), QStringLiteral("The bank")};
    QTest::newRow("per field") << QStringLiteral("field") << QStringList{QStringLiteral("The bank")};
}

void TestElsewhere::a_conflict_is_settled_as_the_sync_conflicts_dialog_chooses()
{
    QFETCH(QString, way);
    QFETCH(QStringList, titles);
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QVariantMap item = controller->newLogin();
    item.insert(QStringLiteral("title"), QStringLiteral("Bank"));
    QVERIFY(controller->saveItem(item));
    QTRY_VERIFY(controller->isIdle());
    const QString id = item.value(QStringLiteral("id")).toString();
    retitleElsewhere(vaultPath, "pass", QStringLiteral("Bank"), QStringLiteral("The bank"));
    QVariantMap mine = controller->item(id);
    mine.insert(QStringLiteral("title"), QStringLiteral("My bank"));
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->isIdle());
    QTRY_COMPARE(controller->conflictCount(), 1);

    // The row and the item's choices, as the dialog shows them (D-060):
    // this device's on the left, the other device's on the right (D-061).
    const QVariantMap row = controller->conflicts().constFirst().toMap();
    QCOMPARE(row.value(QStringLiteral("name")).toString(), QStringLiteral("My bank"));
    QCOMPARE(row.value(QStringLiteral("fields")).toString(), QStringLiteral("Title"));
    QCOMPARE(row.value(QStringLiteral("kind")).toString(), QStringLiteral("both"));
    const QVariantMap conflict = controller->conflictOf(id);
    QCOMPARE(conflict.value(QStringLiteral("leftName")).toString(), QStringLiteral("This device"));
    QCOMPARE(conflict.value(QStringLiteral("rightName")).toString(), QStringLiteral("Other device"));
    const QVariantList rows = conflict.value(QStringLiteral("rows")).toList();
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows[0].toMap().value(QStringLiteral("left")).toString(), QStringLiteral("My bank"));
    QCOMPARE(rows[0].toMap().value(QStringLiteral("right")).toString(), QStringLiteral("The bank"));

    if (way == QLatin1String("field")) {
        controller->settleConflict(id, {QStringLiteral("right")});
    } else {
        controller->settleConflicts({id}, way);
    }
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->conflictCount(), 0);
    QStringList onDisk = titlesIn(vaultPath, "pass");
    onDisk.sort();
    QCOMPARE(onDisk, titles);
}

void TestElsewhere::an_item_deleted_elsewhere_goes_to_the_trash_with_use_other_value()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QVariantMap item = controller->newLogin();
    item.insert(QStringLiteral("title"), QStringLiteral("Bank"));
    QVERIFY(controller->saveItem(item));
    QTRY_VERIFY(controller->isIdle());
    const QString id = item.value(QStringLiteral("id")).toString();
    deleteElsewhere(vaultPath, "pass", QStringLiteral("Bank"));
    QVariantMap mine = controller->item(id);
    mine.insert(QStringLiteral("title"), QStringLiteral("My bank"));
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->isIdle());
    QTRY_COMPARE(controller->conflictCount(), 1);
    QCOMPARE(controller->conflicts().constFirst().toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("deletedThere"));
    // Every field of what remains, "(deleted)" on the other side, and nothing
    // to choose per field (D-061).
    const QVariantMap conflict = controller->conflictOf(id);
    QVERIFY(conflict.value(QStringLiteral("rows")).toList().isEmpty());
    const QVariantMap first = conflict.value(QStringLiteral("allRows")).toList().constFirst().toMap();
    QCOMPARE(first.value(QStringLiteral("left")).toString(), QStringLiteral("My bank"));
    QCOMPARE(first.value(QStringLiteral("right")).toString(), QStringLiteral("(deleted)"));
    QVERIFY(conflict.value(QStringLiteral("canKeepHere")).toBool());
    QVERIFY(!conflict.value(QStringLiteral("canKeepBoth")).toBool());
    // Keep both is no way to settle it: nothing changes.
    controller->settleConflicts({id}, QStringLiteral("both"));
    QCOMPARE(controller->conflictCount(), 1);
    controller->settleConflicts({id}, QStringLiteral("right"));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->conflictCount(), 0);
    QVERIFY(controller->item(id).value(QStringLiteral("trashed")).toBool()); // in the Trash, never gone for good
}

void TestElsewhere::a_deletion_here_stays_after_a_merge()
{
    auto controller = unlocked(vaultPath);
    QVERIFY(controller);
    QVariantMap item = controller->newLogin();
    item.insert(QStringLiteral("title"), QStringLiteral("Old"));
    QVERIFY(controller->saveItem(item));
    QTRY_VERIFY(controller->isIdle());
    // Elsewhere an item is added; here "Old" is deleted for good meanwhile.
    addElsewhere(vaultPath, "pass", QStringLiteral("Theirs"));
    controller->deleteItem(item.value(QStringLiteral("id")).toString());
    QTRY_COMPARE(titlesIn(vaultPath, "pass"), QStringList{QStringLiteral("Theirs")});
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(shownTitles(*controller), QStringList{QStringLiteral("Theirs")});
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
    // Merged with the other version (D-054): both kept, no conflict copy.
    QTRY_COMPARE(titlesIn(vaultFile, "pass"), QStringList({"Mine", "Theirs"}));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(shownTitles(*controller), QStringList({"Mine", "Theirs"}));
    QVERIFY(conflictOf(*controller).isEmpty());
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

void TestElsewhere::a_merge_that_cannot_be_written_is_tried_again_and_keeps_both()
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
    QCOMPARE(titlesIn(vaultFile, "pass"), QStringList{QStringLiteral("Theirs")});
    // Tried again: the merge of both is written (D-054).
    controller->retrySave();
    QTRY_COMPARE(titlesIn(vaultFile, "pass"), QStringList({"Mine", "Theirs"}));
    QTRY_VERIFY(controller->isIdle());
    QVERIFY(conflictOf(*controller).isEmpty());
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
