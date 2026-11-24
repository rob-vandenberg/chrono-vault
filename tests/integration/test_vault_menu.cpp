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


#include "settings_files.h"
#include "vault_contents.h"
#include "vault_controller.h"
#include "vault_file.h"
#include "webdav_sync.h"
#include "webdav_test_server.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vault_menu.cpp 0.5.78

// --- Version history ----------------------------------------------------
// v0.5.78: An import's every difference waits for the person, an item deleted here too (D-061)
// v0.5.77: Import from another vault (D-060)
// v0.5.75: Initial release: the vault menu's Lock, Move… and Forget… (D-060)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};
const QString vaultName = QStringLiteral("Home.chronovault");

class MemoryStore : public SecretStore
{
public:
    bool keep(const QString& name, const QByteArray& secret) override
    {
        const QMutexLocker locker(&lock);
        kept.insert(name, secret);
        return true;
    }
    std::pair<Result, QByteArray> find(const QString& name) override
    {
        const QMutexLocker locker(&lock);
        return kept.contains(name) ? std::pair{Result::Done, kept.value(name)} : std::pair{Result::NoSecret, QByteArray()};
    }
    bool remove(const QString& name) override
    {
        const QMutexLocker locker(&lock);
        kept.remove(name);
        return true;
    }

private:
    QMutex lock;
    QHash<QString, QByteArray> kept;
};

std::unique_ptr<VaultController> device()
{
    auto controller = std::make_unique<VaultController>();
    controller->setKdfParameters(quick);
    controller->setSecretStoreForTests(std::make_unique<MemoryStore>());
    return controller;
}

bool connected(VaultController& controller, const WebDavTestServer& server)
{
    QSignalSpy answered(&controller, &VaultController::webDavConnected);
    controller.connectWebDav(server.folder().toString(), server.user, server.password, QString());
    return answered.wait(10000) && answered.constFirst().at(0).toBool();
}

bool addHere(VaultController& controller, const QString& title)
{
    QVariantMap item = controller.newLogin();
    item.insert(QStringLiteral("title"), title);
    return controller.saveItem(item) && QTest::qWaitFor([&controller]() { return controller.isIdle(); }, 10000);
}

QStringList titlesIn(const QString& file, const QByteArray& password = "pass")
{
    const OpenResult opened = openVaultFile(file, password);
    QStringList titles;
    for (const Item& item : contentsFromJson(opened.contents).contents.items) {
        titles << item.title;
    }
    return titles;
}

QStringList titlesOfData(const QByteArray& data)
{
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("read.chronovault"));
    return saveVaultData(file, data) == SaveStatus::Saved ? titlesIn(file) : QStringList{QStringLiteral("(none)")};
}

// A new local vault with one item, unlocked.
bool madeWithItem(VaultController& controller, const QString& file)
{
    controller.createVault(QUrl::fromLocalFile(file), QStringLiteral("pass"), QStringLiteral("pass"));
    return QTest::qWaitFor([&controller]() { return controller.state() == State::Unlocked; }, 10000) &&
           addHere(controller, QStringLiteral("Mine"));
}

// The answer of a move.
std::pair<bool, QString> moved(QSignalSpy& done)
{
    if (done.isEmpty() && !done.wait(10000)) {
        return {false, QStringLiteral("no answer")};
    }
    return {done.constLast().at(0).toBool(), done.constLast().at(1).toString()};
}

QString waitCountKey(const QString& vaultFile)
{
    return QStringLiteral("security/wrongAttempts/") +
           QString::fromLatin1(QCryptographicHash::hash(vaultFile.toUtf8(), QCryptographicHash::Sha256).toHex().left(24)) +
           QStringLiteral("/count");
}

} // namespace

class TestVaultMenu : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_vault_menu"));
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        QDir(webDavDataFolder()).removeRecursively();
    }
    void cleanup() { init(); }
    void cleanupTestCase()
    {
        init();
        QFile::remove(SettingsFile().fileName());
        QFile::remove(VaultsFile().fileName());
    }

    void lock_locks_only_the_unlocked_vault()
    {
        QTemporaryDir folder;
        auto controller = device();
        QVERIFY(madeWithItem(*controller, folder.filePath(QStringLiteral("Home.chronovault"))));
        controller->lockVault(folder.filePath(QStringLiteral("Other.chronovault")));
        QCOMPARE(controller->state(), State::Unlocked);
        controller->lockVault(controller->vaultPath());
        QCOMPARE(controller->state(), State::Locked);
    }

    void a_vault_moves_to_another_file_here()
    {
        QTemporaryDir folder;
        const QString from = folder.filePath(QStringLiteral("Home.chronovault"));
        const QString to = folder.filePath(QStringLiteral("elsewhere/Home moved.chronovault"));
        QVERIFY(QDir().mkpath(folder.filePath(QStringLiteral("elsewhere"))));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, from));
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultTo(from, QUrl::fromLocalFile(to));
        // Locked at once: nothing changes while it is copied (the owner).
        QCOMPARE(controller->state(), State::Locked);
        const auto [ok, problem] = moved(done);
        QVERIFY2(ok, qPrintable(problem));
        QVERIFY(!QFileInfo::exists(from));
        QCOMPARE(titlesIn(to), QStringList{QStringLiteral("Mine")});
        QCOMPARE(controller->vaults(), QStringList{to});
        QCOMPARE(controller->vaultPath(), to);
        QCOMPARE(controller->state(), State::Locked);
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE_WITH_TIMEOUT(controller->state(), State::Unlocked, 10000);
    }

    void a_move_never_goes_over_a_file()
    {
        QTemporaryDir folder;
        const QString from = folder.filePath(QStringLiteral("Home.chronovault"));
        const QString there = folder.filePath(QStringLiteral("There.chronovault"));
        QFile file(there);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("someone's file");
        file.close();
        auto controller = device();
        QVERIFY(madeWithItem(*controller, from));
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultTo(from, QUrl::fromLocalFile(there));
        QVERIFY(!moved(done).first);
        QCOMPARE(controller->state(), State::Unlocked); // refused before anything was done
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("someone's file"));
        QCOMPARE(titlesIn(from), QStringList{QStringLiteral("Mine")});
    }

    void a_move_stopped_at_any_step_keeps_the_vault_data()
    {
        QTest::addColumn<int>("step");
        for (const MoveStep step : {MoveStep::Read, MoveStep::Written, MoveStep::Verified}) {
            QTest::newRow(QByteArray::number(static_cast<int>(step)).constData()) << static_cast<int>(step);
        }
    }
    void a_move_stopped_at_any_step_keeps_the_vault()
    {
        QFETCH(int, step);
        QTemporaryDir folder;
        const QString from = folder.filePath(QStringLiteral("Home.chronovault"));
        const QString to = folder.filePath(QStringLiteral("Moved.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, from));
        controller->setMoveStepCheckForTests([step](MoveStep reached) { return static_cast<int>(reached) != step; });
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultTo(from, QUrl::fromLocalFile(to));
        QVERIFY(!moved(done).first);
        QVERIFY(!controller->moving());
        QCOMPARE(titlesIn(from), QStringList{QStringLiteral("Mine")});
        QVERIFY(!QFileInfo::exists(to));
        QCOMPARE(controller->vaults(), QStringList{from});
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE_WITH_TIMEOUT(controller->state(), State::Unlocked, 10000);
    }

    void a_vault_moves_to_webdav_and_back_here()
    {
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString from = folder.filePath(QStringLiteral("Home.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, from));
        QVERIFY(connected(*controller, server));
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultToWebDav(from);
        auto [ok, problem] = moved(done);
        QVERIFY2(ok, qPrintable(problem));
        QVERIFY(!QFileInfo::exists(from));
        QCOMPARE(titlesOfData(server.fileData(vaultName)), QStringList{QStringLiteral("Mine")});
        QCOMPARE(server.files.size(), 1); // nothing passing left behind
        const QString workingCopy = controller->vaultPath();
        QCOMPARE(controller->vaults(), QStringList{workingCopy});
        QVERIFY(workingCopy.startsWith(webDavDataFolder()));
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE_WITH_TIMEOUT(controller->state(), State::Unlocked, 10000);
        QVERIFY(controller->onWebDav());
        QTRY_VERIFY_WITH_TIMEOUT(controller->syncForTests() && !controller->syncForTests()->running(), 10000);
        QVERIFY(controller->syncProblem().isEmpty());

        // Back to a file here: the server's copy and the working copy go.
        const QString back = folder.filePath(QStringLiteral("Back.chronovault"));
        done.clear();
        controller->moveVaultTo(workingCopy, QUrl::fromLocalFile(back));
        std::tie(ok, problem) = moved(done);
        QVERIFY2(ok, qPrintable(problem));
        QCOMPARE(titlesIn(back), QStringList{QStringLiteral("Mine")});
        QTRY_VERIFY_WITH_TIMEOUT(!server.files.contains(vaultName), 10000);
        QVERIFY(!QFileInfo::exists(workingCopy));
        QCOMPARE(controller->vaults(), QStringList{back});
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE_WITH_TIMEOUT(controller->state(), State::Unlocked, 10000);
        QVERIFY(!controller->onWebDav());
    }

    void a_move_to_webdav_stopped_at_any_step_keeps_the_vault_data()
    {
        a_move_stopped_at_any_step_keeps_the_vault_data();
    }
    void a_move_to_webdav_stopped_at_any_step_keeps_the_vault()
    {
        QFETCH(int, step);
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString from = folder.filePath(QStringLiteral("Home.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, from));
        QVERIFY(connected(*controller, server));
        controller->setMoveStepCheckForTests([step](MoveStep reached) { return static_cast<int>(reached) != step; });
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultToWebDav(from);
        QVERIFY(!moved(done).first);
        QTRY_VERIFY_WITH_TIMEOUT(server.files.isEmpty(), 10000); // nothing left on the server
        QCOMPARE(titlesIn(from), QStringList{QStringLiteral("Mine")});
        QCOMPARE(controller->vaults(), QStringList{from});
        QVERIFY(QDir(webDavDataFolder()).entryList(QDir::Files).isEmpty());
    }

    void a_move_never_replaces_a_vault_another_device_put_there_meanwhile()
    {
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString from = folder.filePath(QStringLiteral("Home.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, from));
        QVERIFY(connected(*controller, server));
        // After the folder was listed, another device puts its Home there.
        server.beforeAnswer = [&server](const QByteArray& method, const QString&) {
            if (method == "MOVE" && !server.files.contains(vaultName)) {
                server.setFile(vaultName, "another device's vault");
            }
        };
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultToWebDav(from);
        const auto [ok, problem] = moved(done);
        QVERIFY(!ok);
        QVERIFY(problem.contains(QStringLiteral("already in that folder")));
        QCOMPARE(server.fileData(vaultName), QByteArray("another device's vault"));
        QTRY_COMPARE_WITH_TIMEOUT(server.files.size(), 1, 10000); // the passing upload removed
        QCOMPARE(titlesIn(from), QStringList{QStringLiteral("Mine")});
        QCOMPARE(controller->vaults(), QStringList{from});
    }

    void a_webdav_vault_with_changes_not_on_the_server_is_not_moved()
    {
        WebDavTestServer server;
        QTemporaryDir folder;
        auto controller = device();
        QVERIFY(connected(*controller, server));
        controller->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(controller->syncForTests() && !controller->syncForTests()->running(), 10000);
        server.refuseWith = 503; // the change stays on this device
        QVERIFY(addHere(*controller, QStringLiteral("Not sent")));
        controller->lock();
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        const QString workingCopy = controller->vaultPath();
        controller->moveVaultTo(workingCopy, QUrl::fromLocalFile(folder.filePath(QStringLiteral("Here.chronovault"))));
        const auto [ok, problem] = moved(done);
        QVERIFY(!ok);
        QVERIFY(problem.contains(QStringLiteral("not on the server")));
        QVERIFY(QFileInfo::exists(workingCopy));
    }

    void a_moved_vault_keeps_its_count_of_wrong_passwords()
    {
        QTemporaryDir folder;
        const QString from = folder.filePath(QStringLiteral("Home.chronovault"));
        const QString to = folder.filePath(QStringLiteral("Moved.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, from));
        controller->lock();
        controller->unlock(QStringLiteral("wrong"));
        QTRY_COMPARE_WITH_TIMEOUT(controller->state(), State::Locked, 10000);
        QTRY_COMPARE(VaultsFile().value(waitCountKey(from)).toInt(), 1);
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultTo(from, QUrl::fromLocalFile(to));
        QVERIFY(moved(done).first);
        QCOMPARE(VaultsFile().value(waitCountKey(to)).toInt(), 1);
        QVERIFY(!VaultsFile().contains(waitCountKey(from)));
    }

    void a_moved_vaults_password_saved_in_the_primary_follows_it()
    {
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const QString other = folder.filePath(QStringLiteral("Other.chronovault"));
        const QString moved = folder.filePath(QStringLiteral("Other moved.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, primary));
        controller->createVault(QUrl::fromLocalFile(other), QStringLiteral("other pass"), QStringLiteral("other pass"),
                                QUrl(), true);
        QTRY_VERIFY_WITH_TIMEOUT(controller->vaultPath() == other && controller->state() == State::Unlocked, 10000);
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultTo(other, QUrl::fromLocalFile(moved));
        QVERIFY(::moved(done).first);
        QCOMPARE(controller->vaults(), (QStringList{primary, moved}));

        // The primary unlocked: its saved password names the new place, and
        // the moved vault opens with it, nothing typed.
        controller->switchToVault(primary);
        controller->unlock(QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked && controller->isIdle(), 10000);
        controller->switchToVault(moved);
        QTRY_VERIFY_WITH_TIMEOUT(controller->vaultPath() == moved && controller->state() == State::Unlocked, 10000);
        QVERIFY(!VaultsFile().contains(QStringLiteral("vaults/moved")));
        bool named = false;
        const OpenResult opened = openVaultFile(primary, "pass");
        for (const Item& item : contentsFromJson(opened.contents).contents.items) {
            for (const Field& field : item.fields) {
                named = named || (field.role == QLatin1String("vaultfile") && field.value == moved);
            }
        }
        QVERIFY(named);
    }

    void import_from_another_vault_adds_new_items_and_makes_differences_conflicts()
    {
        QTemporaryDir folder;
        const QString here = folder.filePath(QStringLiteral("Here.chronovault"));
        const QString other = folder.filePath(QStringLiteral("Other.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, here)); // "Mine"
        QVERIFY(addHere(*controller, QStringLiteral("Same")));
        // The other vault began as a copy: the same items, the same ids.
        QVERIFY(QFile::copy(here, other));
        {
            const OpenResult opened = openVaultFile(other, "pass");
            ContentsResult read = contentsFromJson(opened.contents);
            // An attachment and a tag only the other vault has.
            for (Item& item : read.contents.items) {
                if (item.title == QLatin1String("Same")) {
                    Attachment attachment;
                    attachment.id = QUuid::createUuid();
                    attachment.name = QStringLiteral("scan.pdf");
                    attachment.data = "pdf";
                    item.attachments.append(attachment);
                    item.tags.append(QStringLiteral("Work"));
                }
            }
            Item theirs;
            theirs.id = QUuid::createUuid();
            theirs.title = QStringLiteral("Theirs");
            read.contents.items.append(theirs);
            QCOMPARE(saveVaultFile(other, contentsToJson(read.contents), opened.key), SaveStatus::Saved);
        }
        QString mineId;
        for (int row = 0; row < controller->items()->rowCount(); ++row) {
            const QModelIndex index = controller->items()->index(row);
            if (controller->items()->data(index, ItemListModel::TitleRole).toString() == QStringLiteral("Mine")) {
                mineId = controller->items()->data(index, ItemListModel::IdRole).toString();
            }
        }
        QVariantMap mine = controller->item(mineId);
        mine.insert(QStringLiteral("title"), QStringLiteral("Mine here"));
        QVERIFY(controller->saveItem(mine));
        QTRY_VERIFY(controller->isIdle());

        // A wrong master password, and the vault itself, are refused.
        QSignalSpy read(controller.get(), &VaultController::importRead);
        controller->readVaultImport(QUrl::fromLocalFile(other), QStringLiteral("wrong"));
        QVERIFY(read.wait(10000));
        QVERIFY(!read.constLast().at(0).toMap().value(QStringLiteral("ok")).toBool());
        controller->readVaultImport(QUrl::fromLocalFile(here), QStringLiteral("pass"));
        QVERIFY(!read.constLast().at(0).toMap().value(QStringLiteral("ok")).toBool());

        controller->readVaultImport(QUrl::fromLocalFile(other), QStringLiteral("pass"));
        QVERIFY(read.wait(10000));
        QVERIFY(read.constLast().at(0).toMap().value(QStringLiteral("ok")).toBool());
        QCOMPARE(read.constLast().at(0).toMap().value(QStringLiteral("items")).toInt(), 3);
        QVERIFY(controller->confirmImport());
        QCOMPARE(controller->lastImportConflicts(), 2);
        QTRY_VERIFY(controller->isIdle());

        // "Theirs" added; "Mine" kept as "Mine here" with the imported title
        // to choose; "Same" kept as it is, the import's attachment and tag
        // waiting for the person (D-061).
        QStringList titles = titlesIn(here);
        titles.sort();
        QCOMPARE(titles, (QStringList{QStringLiteral("Mine here"), QStringLiteral("Same"), QStringLiteral("Theirs")}));
        QCOMPARE(controller->conflictCount(), 2);
        QCOMPARE(controller->conflicts().constFirst().toMap().value(QStringLiteral("kind")).toString(),
                 QStringLiteral("imported"));
        const QVariantMap conflict = controller->conflictOf(mineId);
        QCOMPARE(conflict.value(QStringLiteral("leftName")).toString(), QStringLiteral("Vault"));
        QCOMPARE(conflict.value(QStringLiteral("rightName")).toString(), QStringLiteral("Import"));
        const QVariantList rows = conflict.value(QStringLiteral("rows")).toList();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].toMap().value(QStringLiteral("left")).toString(), QStringLiteral("Mine here"));
        QCOMPARE(rows[0].toMap().value(QStringLiteral("right")).toString(), QStringLiteral("Mine"));
        QString sameId;
        for (const QVariant& one : controller->conflicts()) {
            if (one.toMap().value(QStringLiteral("name")).toString() == QLatin1String("Same")) {
                sameId = one.toMap().value(QStringLiteral("itemId")).toString();
                QCOMPARE(one.toMap().value(QStringLiteral("fields")).toString(), QStringLiteral("Tag, Attachment"));
            }
        }
        QVERIFY(!sameId.isEmpty());
        const auto sameOnDisk = [&here]() {
            const OpenResult opened = openVaultFile(here, "pass");
            for (const Item& item : contentsFromJson(opened.contents).contents.items) {
                if (item.title == QLatin1String("Same")) {
                    return item;
                }
            }
            return Item();
        };
        QVERIFY(sameOnDisk().attachments.isEmpty());
        QVERIFY(sameOnDisk().tags.isEmpty());
        // Imported again: the conflict left is not counted as new.
        controller->readVaultImport(QUrl::fromLocalFile(other), QStringLiteral("pass"));
        QVERIFY(read.wait(10000));
        QVERIFY(controller->confirmImport());
        QVERIFY(!controller->message().contains(QStringLiteral("differ")));
        QCOMPARE(controller->lastImportConflicts(), 0); // the dialog does not open again
        QTRY_VERIFY(controller->isIdle());
        QCOMPARE(controller->conflictCount(), 2);
        // The import's side taken for "Same": its attachment and tag added.
        controller->settleConflicts({sameId}, QStringLiteral("right"));
        QTRY_VERIFY(controller->isIdle());
        QCOMPARE(sameOnDisk().attachments.size(), 1);
        QCOMPARE(sameOnDisk().tags, QStringList{QStringLiteral("Work")});
        // Kept in the vault file as imported.
        bool imported = false;
        const OpenResult opened = openVaultFile(here, "pass");
        for (const Item& item : contentsFromJson(opened.contents).contents.items) {
            for (const MergeNote& note : item.mergeNotes) {
                imported = imported || note.imported;
            }
        }
        QVERIFY(imported);
    }

    // Deleted here for good, held by the import: the person chooses (D-061).
    // Kept until then, its deletion forgotten, so no sync takes it away; the
    // vault's side deletes it for good again, the import's keeps it.
    void an_item_deleted_here_and_imported_again_waits_for_the_person_data()
    {
        QTest::addColumn<QString>("side");
        QTest::addColumn<int>("items");
        QTest::newRow("vault") << QStringLiteral("left") << 0;
        QTest::newRow("import") << QStringLiteral("right") << 1;
    }
    void an_item_deleted_here_and_imported_again_waits_for_the_person()
    {
        QTemporaryDir folder;
        const QString here = folder.filePath(QStringLiteral("Here.chronovault"));
        const QString other = folder.filePath(QStringLiteral("Other.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, here)); // "Mine"
        QVERIFY(QFile::copy(here, other));
        const QString id = controller->items()->data(controller->items()->index(0), ItemListModel::IdRole).toString();
        controller->deleteItem(id); // for good
        QTRY_VERIFY(controller->isIdle());
        QSignalSpy read(controller.get(), &VaultController::importRead);
        controller->readVaultImport(QUrl::fromLocalFile(other), QStringLiteral("pass"));
        QVERIFY(read.wait(10000));
        QVERIFY(controller->confirmImport());
        QCOMPARE(controller->lastImportConflicts(), 1);
        QTRY_VERIFY(controller->isIdle());
        {
            const OpenResult opened = openVaultFile(here, "pass");
            const VaultContents contents = contentsFromJson(opened.contents).contents;
            QCOMPARE(contents.items.size(), 1);
            QVERIFY(contents.deletedItems.isEmpty()); // the deletion forgotten meanwhile
        }
        const QVariantList rows = controller->conflictOf(id).value(QStringLiteral("rows")).toList();
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows[0].toMap().value(QStringLiteral("left")).toString(), QStringLiteral("Deleted for good"));
        QFETCH(QString, side);
        QFETCH(int, items);
        controller->settleConflict(id, {side});
        QTRY_VERIFY(controller->isIdle());
        const OpenResult opened = openVaultFile(here, "pass");
        const VaultContents contents = contentsFromJson(opened.contents).contents;
        QCOMPARE(contents.items.size(), items);
        QCOMPARE(contents.deletedItems.size(), 1 - items);
        QCOMPARE(controller->conflictCount(), 0);
    }

    void forget_takes_a_vault_off_the_list_and_leaves_its_file()
    {
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const QString other = folder.filePath(QStringLiteral("Other.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, primary));
        QVERIFY(madeWithItem(*controller, other));
        QCOMPARE(controller->vaults(), (QStringList{primary, other}));

        // Never the primary.
        controller->forgetVault(primary);
        QCOMPARE(controller->vaults(), (QStringList{primary, other}));

        // The vault open now: locked, forgotten, the primary's unlock page.
        controller->forgetVault(other);
        QCOMPARE(controller->vaults(), QStringList{primary});
        QVERIFY(QFileInfo::exists(other));
        QCOMPARE(controller->vaultPath(), primary);
        QCOMPARE(controller->state(), State::Locked);
    }

    void forget_deletes_a_webdav_vaults_working_copy_at_once()
    {
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, primary));
        QVERIFY(connected(*controller, server));
        controller->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked && controller->onWebDav(), 10000);
        const QString workingCopy = controller->vaultPath();
        QTRY_VERIFY_WITH_TIMEOUT(controller->syncForTests() && !controller->syncForTests()->running(), 10000);
        controller->lock();
        controller->forgetVault(workingCopy);
        QCOMPARE(controller->vaults(), QStringList{primary});
        QVERIFY(!QFileInfo::exists(workingCopy));
        QVERIFY(!webDavPlaceOf(workingCopy));
        QVERIFY(server.files.contains(vaultName)); // the server's copy stays
    }

    void a_vault_not_found_can_be_forgotten()
    {
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const QString gone = folder.filePath(QStringLiteral("Gone.chronovault"));
        auto controller = device();
        QVERIFY(madeWithItem(*controller, primary));
        QVERIFY(madeWithItem(*controller, gone));
        controller->lock();
        QVERIFY(QFile::remove(gone));
        controller->checkVaults();
        QTRY_VERIFY(!controller->foundVaults().contains(gone));
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultTo(gone, QUrl::fromLocalFile(folder.filePath(QStringLiteral("To.chronovault"))));
        QVERIFY(!moved(done).first);
        controller->forgetVault(gone);
        QCOMPARE(controller->vaults(), QStringList{primary});
    }
};

QTEST_MAIN(TestVaultMenu)
#include "test_vault_menu.moc"
