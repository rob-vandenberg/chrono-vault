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

#include "cloud_storage.h"
#include "google_drive_storage.h"
#include "google_drive_test_server.h"
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_delete_vault.cpp 0.8.122

// --- Version history ----------------------------------------------------
// v0.8.122: Initial release: Delete a vault, here, on WebDAV and on Google Drive (D-083)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};
const QString vaultName = QStringLiteral("Home.chronovault");

// The system's store, shared so a test can see what is left in it.
struct Kept
{
    QMutex lock;
    QHash<QString, QByteArray> secrets;
};

class MemoryStore : public SecretStore
{
public:
    explicit MemoryStore(std::shared_ptr<Kept> into) : kept(std::move(into)) {}
    bool keep(const QString& name, const QByteArray& secret) override
    {
        const QMutexLocker locker(&kept->lock);
        kept->secrets.insert(name, secret);
        return true;
    }
    std::pair<Result, QByteArray> find(const QString& name) override
    {
        const QMutexLocker locker(&kept->lock);
        return kept->secrets.contains(name) ? std::pair{Result::Done, kept->secrets.value(name)}
                                            : std::pair{Result::NoSecret, QByteArray()};
    }
    bool remove(const QString& name) override
    {
        const QMutexLocker locker(&kept->lock);
        kept->secrets.remove(name);
        return true;
    }

private:
    std::shared_ptr<Kept> kept;
};

int secretsLeft(const std::shared_ptr<Kept>& kept)
{
    const QMutexLocker locker(&kept->lock);
    return static_cast<int>(kept->secrets.size());
}

void signInInBrowser(const QUrl& address)
{
    static QNetworkAccessManager browser;
    const QUrlQuery asked(address);
    QUrl back(asked.queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded));
    QUrlQuery answer;
    answer.addQueryItem(QStringLiteral("code"), QStringLiteral("the-code"));
    answer.addQueryItem(QStringLiteral("state"), asked.queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded));
    back.setQuery(answer);
    QNetworkReply* reply = browser.get(QNetworkRequest(back));
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

std::unique_ptr<VaultController> device(const std::shared_ptr<Kept>& kept = std::make_shared<Kept>())
{
    auto controller = std::make_unique<VaultController>();
    controller->setKdfParameters(quick);
    controller->setSecretStoreForTests(std::make_unique<MemoryStore>(kept));
    controller->setGoogleBrowserForTests(signInInBrowser);
    return controller;
}

bool unlocked(VaultController& controller)
{
    return QTest::qWaitFor(
        [&controller]() { return controller.state() == State::Unlocked && controller.isIdle(); }, 10000);
}

bool made(VaultController& controller, const QString& file, const QString& password, bool remember = false,
          const QUrl& keyfile = QUrl())
{
    controller.createVault(QUrl::fromLocalFile(file), password, password, keyfile, remember);
    return QTest::qWaitFor([&]() { return controller.vaultPath() == file && controller.state() == State::Unlocked; },
                           10000) &&
           unlocked(controller);
}

bool addHere(VaultController& controller, const QString& title)
{
    QVariantMap item = controller.newLogin();
    item.insert(QStringLiteral("title"), title);
    return controller.saveItem(item) && QTest::qWaitFor([&controller]() { return controller.isIdle(); }, 10000);
}

// The vault's own password typed and proven, as the Delete dialog does.
bool proven(VaultController& controller, const QString& file, const QString& password)
{
    QSignalSpy right(&controller, &VaultController::proven);
    controller.askFor(file);
    controller.proveWithPassword(password);
    return right.wait(10000) || !right.isEmpty();
}

// The answer of a deletion.
std::pair<bool, QString> deleted(VaultController& controller, const QString& file)
{
    QSignalSpy done(&controller, &VaultController::deleteDone);
    controller.deleteVault(file);
    if (done.isEmpty() && !done.wait(10000)) {
        return {false, QStringLiteral("no answer")};
    }
    return {done.constLast().at(0).toBool(), done.constLast().at(1).toString()};
}

// Anything of the vault left in the vaults file: a key named after its
// place, or a value that names it.
QStringList leftOf(const QString& vaultFile)
{
    const QString hashed =
        QString::fromLatin1(QCryptographicHash::hash(vaultFile.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    VaultsFile settings;
    QStringList left;
    for (const QString& key : settings.allKeys()) {
        const QVariant value = settings.value(key);
        const QString said = value.typeId() == QMetaType::QVariantMap
                                 ? QStringList(value.toMap().keys()).join(QLatin1Char(' '))
                                       + QLatin1Char(' ') + QVariant(value.toMap().values()).toStringList().join(QLatin1Char(' '))
                                 : value.toStringList().join(QLatin1Char(' ')) + QLatin1Char(' ') + value.toString();
        if (key.contains(hashed) || said.contains(vaultFile)) {
            left << key;
        }
    }
    return left;
}

QString remembered(const QString& primary, const QByteArray& password, const QString& vaultFile)
{
    const OpenResult opened = openVaultFile(primary, password);
    for (const Item& item : contentsFromJson(opened.contents).contents.items) {
        QString names;
        QString kept;
        for (const Field& field : item.fields) {
            if (field.role == QLatin1String("vaultfile")) {
                names = field.value;
            } else if (field.role == QLatin1String("password")) {
                kept = field.value;
            }
        }
        if (names == vaultFile) {
            return kept;
        }
    }
    return QStringLiteral("(no item)");
}

bool connected(VaultController& controller, const WebDavTestServer& server)
{
    QSignalSpy answered(&controller, &VaultController::webDavConnected);
    controller.connectWebDav(server.folder().toString(), server.user, server.password, QString());
    return answered.wait(10000) && answered.constFirst().at(0).toBool();
}

bool synced(VaultController& controller)
{
    return QTest::qWaitFor(
        [&controller]() {
            return controller.isIdle() && controller.syncForTests() && !controller.syncForTests()->running();
        },
        10000);
}

// A primary here, then a vault "Home" on WebDAV, locked.
bool onWebDav(VaultController& controller, const WebDavTestServer& server, const QString& primary)
{
    if (!made(controller, primary, QStringLiteral("pass")) || !connected(controller, server)) {
        return false;
    }
    controller.createWebDavVault(QStringLiteral("Home"), QStringLiteral("home"), QStringLiteral("home"));
    if (!QTest::qWaitFor([&]() { return controller.vaultPath() != primary && controller.state() == State::Unlocked; },
                         10000) ||
        !synced(controller)) {
        return false;
    }
    controller.lock();
    return controller.state() == State::Locked;
}

void use(const GoogleDriveTestServer& server)
{
    const QUrl base = server.base();
    setGoogleForTests({base.resolved(QUrl(QStringLiteral("auth"))), base.resolved(QUrl(QStringLiteral("token"))),
                       base.resolved(QUrl(QStringLiteral("drive/v3/"))), base.resolved(QUrl(QStringLiteral("upload/drive/v3/")))},
                      QStringLiteral("test-client"), QStringLiteral("test-secret"));
}

} // namespace

class TestDeleteVault : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_delete_vault"));
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        QDir(cloudDataFolder(QStringLiteral("webdav"))).removeRecursively();
        QDir(cloudDataFolder(QStringLiteral("googledrive"))).removeRecursively();
    }
    void cleanup() { init(); }
    void cleanupTestCase()
    {
        init();
        QFile::remove(SettingsFile().fileName());
        QFile::remove(VaultsFile().fileName());
    }

    // The owner's order (D-083): the primary while other vaults exist, then
    // open, then closed; only a locked vault may be deleted.
    void the_checks_come_in_the_owners_order()
    {
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const QString other = folder.filePath(QStringLiteral("Other.chronovault"));
        auto controller = device();
        QVERIFY(made(*controller, primary, QStringLiteral("pass")));
        QCOMPARE(controller->deleteRefusal(primary), 2); // the only vault, open
        QVERIFY(made(*controller, other, QStringLiteral("other"), true));
        QCOMPARE(controller->deleteRefusal(primary), 1); // even closed: the primary first
        QCOMPARE(controller->deleteRefusal(other), 2);   // open
        controller->closeOpenVault(other);
        QTRY_COMPARE(controller->state(), State::Locked);
        QCOMPARE(controller->deleteRefusal(other), 3); // closed: its password kept in the open key store
        controller->lockVault(other);
        QTRY_COMPARE(controller->deleteRefusal(other), 0); // locked

        // Refused without its own password just proven, and never while open.
        auto [ok, problem] = deleted(*controller, other);
        QVERIFY(!ok && !problem.isEmpty());
        QVERIFY(QFileInfo::exists(other));
        QVERIFY(proven(*controller, primary, QStringLiteral("pass")));
        std::tie(ok, problem) = deleted(*controller, other); // another vault's proof
        QVERIFY(!ok);
        QVERIFY(QFileInfo::exists(other));
        QCOMPARE(controller->vaults(), (QStringList{primary, other}));
    }

    // A vault on this computer: its file and everything of it here; its
    // password in the primary emptied; a backup stays.
    void a_local_vault_goes_with_everything_of_it_on_this_computer()
    {
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const QString other = folder.filePath(QStringLiteral("Other.chronovault"));
        const QString keyfile = folder.filePath(QStringLiteral("other.key"));
        const QString backup = folder.filePath(QStringLiteral("Other backup.chronovault"));
        {
            QFile key(keyfile);
            QVERIFY(key.open(QIODevice::WriteOnly) && key.write("a keyfile of its own") > 0);
        }
        auto controller = device();
        QVERIFY(made(*controller, primary, QStringLiteral("pass")));
        QVERIFY(made(*controller, other, QStringLiteral("other"), true, QUrl::fromLocalFile(keyfile)));
        QVERIFY(addHere(*controller, QStringLiteral("Mine")));
        controller->usedItem(controller->items()->idAt(0));
        controller->setSortBy(1);
        controller->setRemembersFor(other, false);
        QVERIFY(QFile::copy(other, backup));
        QVERIFY(proven(*controller, other, QStringLiteral("other")));
        QSignalSpy pin(controller.get(), &VaultController::pinChanged);
        controller->setPinFor(other, QStringLiteral("1234"));
        QTRY_VERIFY(controller->hasPin(other));
        QVERIFY(remembered(primary, "pass", other) == QStringLiteral("other"));
        QVERIFY(!leftOf(other).isEmpty()); // there is something to delete
        controller->lock(); // the primary's key store too: the password stays in the primary until it is unlocked

        // The wrong password deletes nothing.
        controller->askFor(other);
        controller->proveWithPassword(QStringLiteral("wrong"));
        QTRY_VERIFY(controller->askTrouble());
        auto [ok, problem] = deleted(*controller, other);
        QVERIFY(!ok);
        QVERIFY(QFileInfo::exists(other));

        QVERIFY(proven(*controller, other, QStringLiteral("other")));
        std::tie(ok, problem) = deleted(*controller, other);
        QVERIFY2(ok, qPrintable(problem));
        QVERIFY(!QFileInfo::exists(other));
        QVERIFY(QFileInfo::exists(backup));  // the person's backup stays
        QVERIFY(QFileInfo::exists(keyfile)); // and the keyfile, the person's own file
        QCOMPARE(controller->vaults(), QStringList{primary});
        QCOMPARE(controller->vaultPath(), primary);
        QCOMPARE(controller->state(), State::Locked);
        QCOMPARE(leftOf(other), QStringList{QStringLiteral("vaults/deleted")});

        // The primary unlocked: the deleted vault's password emptied there.
        controller->unlock(QStringLiteral("pass"));
        QVERIFY(unlocked(*controller));
        QTRY_COMPARE(remembered(primary, "pass", other), QString());
        QCOMPARE(leftOf(other), QStringList());
    }

    void a_missing_keyfile_deletes_nothing()
    {
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const QString other = folder.filePath(QStringLiteral("Other.chronovault"));
        const QString keyfile = folder.filePath(QStringLiteral("other.key"));
        {
            QFile key(keyfile);
            QVERIFY(key.open(QIODevice::WriteOnly) && key.write("a keyfile of its own") > 0);
        }
        auto controller = device();
        QVERIFY(made(*controller, primary, QStringLiteral("pass")));
        QVERIFY(made(*controller, other, QStringLiteral("other"), false, QUrl::fromLocalFile(keyfile)));
        controller->lock();
        QVERIFY(QFile::remove(keyfile));
        controller->askFor(other);
        QVERIFY(controller->askKeyfileNeeded());
        controller->proveWithPassword(QStringLiteral("other"));
        QTRY_VERIFY(controller->askTrouble());
        auto [ok, problem] = deleted(*controller, other);
        QVERIFY(!ok);
        QVERIFY(QFileInfo::exists(other));
        QCOMPARE(controller->vaults(), (QStringList{primary, other}));
    }

    // The only vault: the start page (the owner, Q10).
    void the_only_vault_leads_to_the_start_page()
    {
        QTemporaryDir folder;
        const QString only = folder.filePath(QStringLiteral("Only.chronovault"));
        auto controller = device();
        QVERIFY(made(*controller, only, QStringLiteral("pass")));
        controller->lock();
        QCOMPARE(controller->deleteRefusal(only), 0);
        QVERIFY(proven(*controller, only, QStringLiteral("pass")));
        auto [ok, problem] = deleted(*controller, only);
        QVERIFY2(ok, qPrintable(problem));
        QVERIFY(!QFileInfo::exists(only));
        QVERIFY(controller->vaults().isEmpty());
        QCOMPARE(controller->state(), State::NoVault);
        QVERIFY(controller->primaryVault().isEmpty());
        QCOMPARE(leftOf(only), QStringList());
    }

    // WebDAV: on the server first, then here.
    void a_webdav_vault_goes_from_the_server_and_from_here()
    {
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const auto kept = std::make_shared<Kept>();
        auto controller = device(kept);
        QVERIFY(onWebDav(*controller, server, primary));
        const QString workingCopy = controller->vaults().constLast();
        const auto place = cloudPlaceOf(workingCopy);
        QVERIFY(place && QFileInfo::exists(cloudBasePath(*place)));
        QVERIFY(secretsLeft(kept) > 0);
        QVERIFY(proven(*controller, workingCopy, QStringLiteral("home")));
        auto [ok, problem] = deleted(*controller, workingCopy);
        QVERIFY2(ok, qPrintable(problem));
        QVERIFY(!server.files.contains(vaultName));
        QVERIFY(!QFileInfo::exists(workingCopy));
        QVERIFY(!QFileInfo::exists(cloudBasePath(*place)));
        QVERIFY(!cloudPlaceOf(workingCopy));
        QTRY_COMPARE(secretsLeft(kept), 0);
        QCOMPARE(controller->vaults(), QStringList{primary});
    }

    // The server refuses: nothing at all is deleted, the reason is said.
    void a_server_that_fails_deletes_nothing()
    {
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        auto controller = device();
        QVERIFY(onWebDav(*controller, server, primary));
        const QString workingCopy = controller->vaults().constLast();
        QVERIFY(proven(*controller, workingCopy, QStringLiteral("home")));
        server.refuseWith = 500;
        auto [ok, problem] = deleted(*controller, workingCopy);
        QVERIFY(!ok);
        QVERIFY(problem.contains(QStringLiteral("Nothing was deleted")));
        server.refuseWith = 0;
        QVERIFY(server.files.contains(vaultName));
        QVERIFY(QFileInfo::exists(workingCopy));
        QVERIFY(cloudPlaceOf(workingCopy));
        QCOMPARE(controller->vaults(), (QStringList{primary, workingCopy}));
    }

    // Stopped at every step (OR-15.4): the other vaults and their settings
    // stay; what is not deleted yet stays as it was, and Delete can be
    // done again.
    void a_deletion_stopped_at_any_step_loses_nothing_it_should_keep_data()
    {
        QTest::addColumn<int>("step");
        QTest::newRow("proven") << static_cast<int>(DeleteStep::Proven);
        QTest::newRow("server deleted") << static_cast<int>(DeleteStep::ServerDeleted);
        QTest::newRow("files deleted") << static_cast<int>(DeleteStep::FilesDeleted);
    }
    void a_deletion_stopped_at_any_step_loses_nothing_it_should_keep()
    {
        QFETCH(int, step);
        WebDavTestServer server;
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        auto controller = device();
        QVERIFY(onWebDav(*controller, server, primary));
        const QString workingCopy = controller->vaults().constLast();
        controller->setDeleteStepCheckForTests([step](DeleteStep at) { return static_cast<int>(at) != step; });
        QVERIFY(proven(*controller, workingCopy, QStringLiteral("home")));
        auto [ok, problem] = deleted(*controller, workingCopy);
        QVERIFY(!ok);
        QVERIFY(!controller->deleting());
        // The primary is untouched, whatever step it stopped at.
        QVERIFY(QFileInfo::exists(primary));
        QCOMPARE(openVaultFile(primary, "pass").status, OpenStatus::Opened);
        QVERIFY(controller->vaults().contains(primary));
        QVERIFY(controller->vaults().contains(workingCopy)); // still listed, to delete again
        QCOMPARE(server.files.contains(vaultName), step == static_cast<int>(DeleteStep::Proven));
        QCOMPARE(QFileInfo::exists(workingCopy), step != static_cast<int>(DeleteStep::FilesDeleted));
        if (step == static_cast<int>(DeleteStep::FilesDeleted)) {
            return; // its files are gone: Forget takes it off the list
        }
        // Delete again: done, the server's copy already gone counts as deleted.
        controller->setDeleteStepCheckForTests({});
        QVERIFY(proven(*controller, workingCopy, QStringLiteral("home")));
        std::tie(ok, problem) = deleted(*controller, workingCopy);
        QVERIFY2(ok, qPrintable(problem));
        QVERIFY(!server.files.contains(vaultName));
        QVERIFY(!QFileInfo::exists(workingCopy));
        QCOMPARE(controller->vaults(), QStringList{primary});
    }

    // Google Drive: to Drive's trash (D-080), then everything here.
    void a_google_drive_vault_goes_to_drives_trash()
    {
        GoogleDriveTestServer server;
        use(server);
        QTemporaryDir folder;
        const QString primary = folder.filePath(QStringLiteral("Primary.chronovault"));
        const auto kept = std::make_shared<Kept>();
        auto controller = device(kept);
        QVERIFY(made(*controller, primary, QStringLiteral("pass")));
        QSignalSpy signedIn(controller.get(), &VaultController::googleDriveConnected);
        QSignalSpy shown(controller.get(), &VaultController::webDavFolderShown);
        controller->connectGoogleDrive();
        QVERIFY(signedIn.wait(10000) && signedIn.constFirst().at(0).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(!shown.isEmpty(), 10000);
        controller->createGoogleDriveVault(QStringLiteral("Home"), QStringLiteral("home"), QStringLiteral("home"));
        QTRY_VERIFY_WITH_TIMEOUT(controller->vaultPath() != primary && controller->state() == State::Unlocked, 10000);
        QVERIFY(synced(*controller));
        const QString workingCopy = controller->vaultPath();
        const auto* onDrive = server.byName(vaultName, server.folderId(QStringLiteral("Chrono Vault")));
        QVERIFY(onDrive);
        const QString id = onDrive->id;
        controller->lock();
        QVERIFY(proven(*controller, workingCopy, QStringLiteral("home")));
        auto [ok, problem] = deleted(*controller, workingCopy);
        QVERIFY2(ok, qPrintable(problem));
        QVERIFY(server.byId(id) && server.byId(id)->trashed);
        QVERIFY(!QFileInfo::exists(workingCopy));
        QVERIFY(!cloudPlaceOf(workingCopy));
        QTRY_COMPARE(secretsLeft(kept), 0);
        QCOMPARE(controller->vaults(), QStringList{primary});
        // Its password in the primary is emptied at the primary's next unlock.
        QCOMPARE(leftOf(workingCopy), QStringList{QStringLiteral("vaults/deleted")});
    }
};

QTEST_MAIN(TestDeleteVault)
#include "test_delete_vault.moc"
