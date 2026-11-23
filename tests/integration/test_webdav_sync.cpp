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

#include <QDir>
#include <QFile>
#include <QMutex>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_webdav_sync.cpp 0.3.62

// --- Version history ----------------------------------------------------
// v0.3.62: A failed first upload leaves nothing; the folder browser; leftovers; the same bytes (built apart)
// v0.3.61: The first upload through a lock's placeholder that has no tag or size (pCloud)
// v0.3.60: Initial release: a vault kept on a WebDAV server (D-057); its interruptions built apart

using namespace chrono_vault;
using State = VaultController::State;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};
const QString vaultName = QStringLiteral("Home.chronovault");

// The system's store, in memory.
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

[[maybe_unused]] QStringList titlesOfData(const QByteArray& data, const QByteArray& password = "pass")
{
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("read.chronovault"));
    if (saveVaultData(file, data) != SaveStatus::Saved) {
        return {QStringLiteral("(not a vault)")};
    }
    const OpenResult opened = openVaultFile(file, password);
    QStringList titles;
    for (const Item& item : contentsFromJson(opened.contents).contents.items) {
        titles << item.title;
    }
    titles.sort();
    return titles;
}

// Another device adds an item to the vault on the server.
[[maybe_unused]] void addOnServer(WebDavTestServer& server, const QString& title)
{
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("other.chronovault"));
    QCOMPARE(saveVaultData(file, server.fileData(vaultName)), SaveStatus::Saved);
    const OpenResult opened = openVaultFile(file, "pass");
    QCOMPARE(opened.status, OpenStatus::Opened);
    ContentsResult read = contentsFromJson(opened.contents);
    Item item;
    item.id = QUuid::createUuid();
    item.title = title;
    read.contents.items.append(item);
    QCOMPARE(saveVaultFile(file, contentsToJson(read.contents), opened.key), SaveStatus::Saved);
    QFile written(file);
    QVERIFY(written.open(QIODevice::ReadOnly));
    server.setFile(vaultName, written.readAll());
}

[[maybe_unused]] QStringList shownTitles(VaultController& controller)
{
    QStringList titles;
    for (int row = 0; row < controller.items()->rowCount(); ++row) {
        titles << controller.items()->data(controller.items()->index(row), ItemListModel::TitleRole).toString();
    }
    titles.sort();
    return titles;
}

[[maybe_unused]] bool addHere(VaultController& controller, const QString& title)
{
    QVariantMap item = controller.newLogin();
    item.insert(QStringLiteral("title"), title);
    return controller.saveItem(item) && QTest::qWaitFor([&controller]() { return controller.isIdle(); }, 10000);
}

bool syncedNow(VaultController& controller)
{
    controller.syncNow();
    return QTest::qWaitFor(
        [&controller]() {
            return controller.isIdle() && controller.syncForTests() && !controller.syncForTests()->running();
        },
        10000);
}

std::unique_ptr<VaultController> device()
{
    auto controller = std::make_unique<VaultController>();
    controller->setKdfParameters(quick);
    controller->setSecretStoreForTests(std::make_unique<MemoryStore>());
    return controller;
}

bool connected(VaultController& controller, const WebDavTestServer& server, const QString& password = QStringLiteral("secret"),
               const QString& trusted = QString())
{
    QSignalSpy answered(&controller, &VaultController::webDavConnected);
    controller.connectWebDav(server.folder().toString(), server.user, password, trusted);
    return answered.wait(10000) && answered.constFirst().at(0).toBool();
}

} // namespace

class TestWebDavSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_webdav_sync"));
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

#if defined(CHRONO_VAULT_WEBDAV_INTERRUPTIONS)
    // Built as test_webdav_interruptions, so each test stays under 20 seconds (OR-11).
    void a_sync_stopped_at_any_step_loses_nothing_data()
    {
        QTest::addColumn<int>("step");
        for (const SyncStep step : {SyncStep::Downloaded, SyncStep::Merged, SyncStep::Uploaded, SyncStep::Verified,
                                    SyncStep::Locked, SyncStep::Moved, SyncStep::BaseWritten}) {
            QTest::newRow(QByteArray::number(static_cast<int>(step)).constData()) << static_cast<int>(step);
        }
    }
    void a_sync_stopped_at_any_step_loses_nothing()
    {
        QFETCH(int, step);
        WebDavTestServer server;
        auto first = device();
        std::optional<SyncStep> stopAt;
        first->setSyncStepCheckForTests([&stopAt](SyncStep reached) {
            if (stopAt && *stopAt == reached) {
                stopAt.reset();
                return false;
            }
            return true;
        });
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(syncedNow(*first));
        addOnServer(server, QStringLiteral("Theirs"));
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        stopAt = static_cast<SyncStep>(step);
        QVERIFY(syncedNow(*first));
        QVERIFY(!stopAt.has_value());
        // A lock left by the stopped sync runs out on a real server.
        for (auto& file : server.files) {
            file.lockToken.clear();
        }
        // Another device changes it again, then the next syncs finish.
        addOnServer(server, QStringLiteral("Theirs again"));
        QVERIFY(syncedNow(*first));
        QVERIFY(syncedNow(*first));
        const QStringList all{QStringLiteral("Mine"), QStringLiteral("Theirs"), QStringLiteral("Theirs again")};
        QCOMPARE(titlesOfData(server.fileData(vaultName)), all);
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*first), all, 10000);
        QVERIFY(first->syncProblem().isEmpty());
    }

#elif defined(CHRONO_VAULT_WEBDAV_CREATE)
    // Built as test_webdav_create, so each test stays under 20 seconds (OR-11).
    void a_new_vault_that_cannot_reach_the_server_leaves_nothing()
    {
        WebDavTestServer server;
        auto first = device();
        QVERIFY(connected(*first, server));
        server.refuseWith = 507; // the server's disk is full
        QSignalSpy failed(first.get(), &VaultController::webDavCreateFailed);
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QVERIFY(failed.wait(10000));
        QVERIFY(failed.constFirst().at(0).toString().contains(QStringLiteral("Nothing was kept")));
        QCOMPARE(first->state(), State::NoVault);
        QVERIFY(first->vaults().isEmpty());
        QVERIFY(QDir(webDavDataFolder()).entryList(QDir::Files).isEmpty());
        QVERIFY(!VaultsFile().value(QStringLiteral("vaults/webdav")).toMap().contains(webDavDataFolder() + QStringLiteral("/Home.chronovault")));
    }

    void a_vault_goes_in_the_folder_chosen_in_the_browser()
    {
        WebDavTestServer server;
        auto first = device();
        QVERIFY(connected(*first, server));
        QSignalSpy shown(first.get(), &VaultController::webDavFolderShown);
        first->makeWebDavFolder(QStringLiteral("Work vaults"));
        QVERIFY(shown.wait(10000));
        QVERIFY(shown.constLast().at(0).toString().endsWith(QStringLiteral("/Work vaults/")));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(!server.fileData(QStringLiteral("Work vaults/Home.chronovault")).isEmpty());
        // A folder that cannot be opened leaves the one shown: the next vault
        // still goes there.
        QSignalSpy done(first.get(), &VaultController::webDavDone);
        first->openWebDavFolder(QStringLiteral("Gone"));
        QVERIFY(done.wait(10000));
        QVERIFY(!done.constLast().at(0).toBool());
        first->createWebDavVault(QStringLiteral("Second"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(!server.fileData(QStringLiteral("Work vaults/Second.chronovault")).isEmpty(), 10000);

        // Up again: the folder is listed in the folder above.
        shown.clear();
        first->openWebDavFolder(QStringLiteral(".."));
        QVERIFY(shown.wait(10000));
        const QVariantList entries = shown.constLast().at(1).toList();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.constFirst().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Work vaults"));
        QVERIFY(entries.constFirst().toMap().value(QStringLiteral("folder")).toBool());
    }

    void its_own_leftover_upload_is_removed_and_the_same_bytes_are_no_change()
    {
        WebDavTestServer server;
        auto first = device();
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(syncedNow(*first));
        const QString id = webDavPlaceOf(first->vaultPath())->id;
        const QString leftover = vaultName + QStringLiteral(".") + id + QStringLiteral(".uploading");
        const QString othersLeftover = vaultName + QStringLiteral(".0123.uploading");
        server.setFile(leftover, QByteArrayLiteral("stopped upload"));
        server.setFile(othersLeftover, QByteArrayLiteral("another device's"));
        // The same bytes under a new tag, as pCloud gives one after an upload.
        const QByteArray before = server.fileData(vaultName);
        server.setFile(vaultName, before);
        QVERIFY(syncedNow(*first));
        QVERIFY(!server.files.contains(leftover));
        QVERIFY(server.files.contains(othersLeftover));
        QCOMPARE(server.fileData(vaultName), before);
        QVERIFY(!first->message().contains(QStringLiteral("changed elsewhere")));
        QVERIFY(first->syncProblem().isEmpty());
    }
#else
    void a_new_vault_goes_to_the_server()
    {
        WebDavTestServer server;
        auto first = device();
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(first->onWebDav());
        // The first upload goes through a lock's placeholder, listed with no
        // tag and no size, as pCloud lists one (0.3.61).
        QTRY_VERIFY_WITH_TIMEOUT(!server.fileData(vaultName).isEmpty(), 10000);
        QVERIFY(first->syncProblem().isEmpty());
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QCOMPARE(titlesOfData(server.fileData(vaultName)), QStringList{QStringLiteral("Mine")});
        QVERIFY(first->syncProblem().isEmpty());
        QVERIFY(first->vaultPlace(first->vaultPath()).value(QStringLiteral("onWebDav")).toBool());
        // A name every system can keep as a file.
        for (const QString& bad : {QStringLiteral("Work: 2026"), QStringLiteral("a?b"), QStringLiteral("CON"),
                                   QStringLiteral("name.")}) {
            first->createWebDavVault(bad, QStringLiteral("pass"), QStringLiteral("pass"));
            QVERIFY(first->message().contains(QStringLiteral("without")));
        }
        // Never over a vault already there.
        first->createWebDavVault(QStringLiteral("Other"), QStringLiteral("pass"), QStringLiteral("pass"));
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QVERIFY(first->message().contains(QStringLiteral("already in that folder")));
    }

    void two_devices_keep_each_others_changes()
    {
        WebDavTestServer server;
        auto first = device();
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(syncedNow(*first));

        auto second = device();
        QVERIFY(connected(*second, server));
        QSignalSpy done(second.get(), &VaultController::webDavDone);
        second->openWebDavVault(vaultName);
        QVERIFY(done.wait(10000));
        QVERIFY(done.constFirst().at(0).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(second->state() == State::Locked, 10000);
        QVERIFY(second->vaultPath() != first->vaultPath());
        second->unlock(QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(second->state() == State::Unlocked, 10000);
        QVERIFY(syncedNow(*second));

        // Both change, neither has the other's change yet.
        QVERIFY(addHere(*first, QStringLiteral("From the first")));
        QVERIFY(addHere(*second, QStringLiteral("From the second")));
        QVERIFY(syncedNow(*first));
        QVERIFY(syncedNow(*second));
        QVERIFY(syncedNow(*first));
        const QStringList both{QStringLiteral("From the first"), QStringLiteral("From the second")};
        QCOMPARE(titlesOfData(server.fileData(vaultName)), both);
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*first), both, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*second), both, 10000);
        // Nothing new: nothing goes up again.
        const QByteArray before = server.fileData(vaultName);
        QVERIFY(syncedNow(*first));
        QVERIFY(syncedNow(*second));
        QCOMPARE(server.fileData(vaultName), before);
    }

    void a_version_put_there_meanwhile_is_never_replaced_data()
    {
        QTest::addColumn<bool>("locks");
        QTest::addColumn<bool>("tags");
        QTest::newRow("with locks") << true << true;
        QTest::newRow("without locks") << false << true;
        QTest::newRow("without locks or tags") << false << false;
    }
    void a_version_put_there_meanwhile_is_never_replaced()
    {
        QFETCH(bool, locks);
        QFETCH(bool, tags);
        WebDavTestServer server;
        server.locksOffered = locks;
        server.tagsOffered = tags;
        auto first = device();
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(syncedNow(*first));
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        // Right before the move is checked, another device uploads.
        bool done = false;
        server.beforeAnswer = [&server, &done](const QByteArray& method, const QString& name) {
            if (!done && method == "LOCK" && name == vaultName) {
                done = true;
                addOnServer(server, QStringLiteral("Theirs"));
            }
        };
        QVERIFY(syncedNow(*first));
        QVERIFY(done);
        QCOMPARE(titlesOfData(server.fileData(vaultName)), (QStringList{QStringLiteral("Mine"), QStringLiteral("Theirs")}));
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*first), (QStringList{QStringLiteral("Mine"), QStringLiteral("Theirs")}),
                                  10000);
    }

    void a_wrong_server_password_is_asked_again()
    {
        WebDavTestServer server;
        auto first = device();
        QVERIFY(connected(*first, server));
        QVERIFY(!connected(*first, server, QStringLiteral("wrong")));
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(syncedNow(*first));
        server.password = QStringLiteral("changed");
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QVERIFY(first->webDavPasswordNeeded());
        QVERIFY(first->syncProblem().contains(QStringLiteral("password")));
        first->giveWebDavPassword(QStringLiteral("changed"));
        QTRY_VERIFY_WITH_TIMEOUT(titlesOfData(server.fileData(vaultName)) == QStringList{QStringLiteral("Mine")}, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(first->syncProblem().isEmpty(), 10000);
    }

    void an_untrusted_certificate_is_asked_and_remembered()
    {
        WebDavTestServer server(true);
        auto first = device();
        QSignalSpy answered(first.get(), &VaultController::webDavConnected);
        first->connectWebDav(server.folder().toString(), server.user, server.password, QString());
        QVERIFY(answered.wait(10000));
        QVERIFY(!answered.constFirst().at(0).toBool());
        const QString fingerprint = answered.constFirst().at(3).toString();
        QCOMPARE(fingerprint.toLatin1().replace(':', "").toLower(), WebDavTestServer::certificateFingerprint());
        QVERIFY(connected(*first, server, server.password, fingerprint));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QVERIFY(first->syncProblem().isEmpty());
        QCOMPARE(titlesOfData(server.fileData(vaultName)), QStringList{QStringLiteral("Mine")});
        QVERIFY(first->isPlainHttp(QStringLiteral("http://example.org/dav")));
        QVERIFY(!first->isPlainHttp(server.folder().toString()));
    }

    void a_local_vault_moves_to_the_server_and_back()
    {
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString local = folder.filePath(QStringLiteral("Home.chronovault"));
        auto first = device();
        first->createVault(QUrl::fromLocalFile(local), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(!first->onWebDav());
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(connected(*first, server));
        first->moveToWebDav(local);
        QTRY_VERIFY_WITH_TIMEOUT(server.files.contains(vaultName) && !server.fileData(vaultName).isEmpty(), 10000);
        QVERIFY(first->onWebDav());
        QVERIFY(syncedNow(*first));
        QCOMPARE(titlesOfData(server.fileData(vaultName)), QStringList{QStringLiteral("Mine")});

        // Disconnected: kept on this device, still on the server.
        QSignalSpy done(first.get(), &VaultController::webDavDone);
        first->disconnectWebDav(local, false);
        QVERIFY(!first->onWebDav());
        QVERIFY(QFileInfo::exists(local));
        QVERIFY(first->vaults().contains(local));
        QVERIFY(server.files.contains(vaultName));

        // Moved again and disconnected with the server's copy deleted.
        QVERIFY(connected(*first, server, QStringLiteral("secret")));
        server.files.remove(vaultName);
        QVERIFY(connected(*first, server));
        first->moveToWebDav(local);
        QVERIFY(syncedNow(*first));
        QVERIFY(server.files.contains(vaultName));
        done.clear();
        first->disconnectWebDav(local, true);
        QVERIFY(done.wait(10000));
        QVERIFY(done.constLast().at(0).toBool());
        QVERIFY(!server.files.contains(vaultName));
        QVERIFY(QFileInfo::exists(local));
    }

    void a_master_password_changed_here_goes_to_the_server_data()
    {
        QTest::addColumn<bool>("tags");
        QTest::newRow("with tags") << true;
        QTest::newRow("without tags") << false;
    }
    void a_master_password_changed_here_goes_to_the_server()
    {
        QFETCH(bool, tags);
        WebDavTestServer server;
        server.tagsOffered = tags;
        auto first = device();
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QSignalSpy changed(first.get(), &VaultController::masterPasswordChanged);
        first->changeMasterPassword(QStringLiteral("pass"), QStringLiteral("new pass"), QStringLiteral("new pass"), QUrl());
        QVERIFY(changed.wait(10000));
        QVERIFY(changed.constFirst().at(0).toBool());
        QVERIFY(syncedNow(*first));
        QVERIFY(syncedNow(*first));
        QCOMPARE(first->state(), State::Unlocked);
        QCOMPARE(titlesOfData(server.fileData(vaultName), "new pass"), QStringList{QStringLiteral("Mine")});
        for (const QString& one : first->vaults()) {
            QVERIFY(!one.contains(QStringLiteral("(conflict ")));
        }
    }

    void another_master_password_on_the_server_locks_and_keeps_this_version()
    {
        WebDavTestServer server;
        auto first = device();
        QVERIFY(connected(*first, server));
        first->createWebDavVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        // Another device gives it another master password.
        {
            QTemporaryDir folder;
            const QString file = folder.filePath(QStringLiteral("other.chronovault"));
            QCOMPARE(saveVaultData(file, server.fileData(vaultName)), SaveStatus::Saved);
            const OpenResult opened = openVaultFile(file, "pass");
            const VaultKey other = makeVaultKey("new pass", quick);
            QCOMPARE(saveVaultFile(file, opened.contents, other), SaveStatus::Saved);
            QFile written(file);
            QVERIFY(written.open(QIODevice::ReadOnly));
            server.setFile(vaultName, written.readAll());
        }
        QVERIFY(addHere(*first, QStringLiteral("Mine too")));
        first->syncNow();
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Locked, 10000);
        QVERIFY(first->message().contains(QStringLiteral("another master password")));
        QString conflict;
        for (const QString& one : first->vaults()) {
            if (one.contains(QStringLiteral("(conflict "))) {
                conflict = one;
            }
        }
        QVERIFY(!conflict.isEmpty());
        const OpenResult kept = openVaultFile(conflict, "pass");
        QCOMPARE(kept.status, OpenStatus::Opened);
        QCOMPARE(contentsFromJson(kept.contents).contents.items.size(), 2);
        first->unlock(QStringLiteral("new pass"));
        QTRY_VERIFY_WITH_TIMEOUT(first->state() == State::Unlocked, 10000);
    }
#endif
};

QTEST_MAIN(TestWebDavSync)
#include "test_webdav_sync.moc"
