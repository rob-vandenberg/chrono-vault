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

#include <QDir>
#include <QFile>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrlQuery>
#include <QUuid>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_google_drive.cpp 0.8.119

// --- Version history ----------------------------------------------------
// v0.8.119: Disconnect leaves the vault on Drive (D-060)
// v0.8.117: Each vault keeps its Google account's email; an older vault gets it at its next sync
// v0.8.116: Two Google accounts in one run, each vault with its own; Google's account chooser always (D-080 point 13)
// v0.8.115: Initial release: a vault on Google Drive, against a Drive of our own (D-080); its interruptions built apart

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

// The person in the browser: signs in, and Google sends the browser back to
// Chrono Vault on this computer with the code.
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

void use(const GoogleDriveTestServer& server)
{
    const QUrl base = server.base();
    setGoogleForTests({base.resolved(QUrl(QStringLiteral("auth"))), base.resolved(QUrl(QStringLiteral("token"))),
                       base.resolved(QUrl(QStringLiteral("drive/v3/"))), base.resolved(QUrl(QStringLiteral("upload/drive/v3/")))},
                      QStringLiteral("test-client"), QStringLiteral("test-secret"));
}

std::unique_ptr<VaultController> device()
{
    auto controller = std::make_unique<VaultController>();
    controller->setKdfParameters(quick);
    controller->setSecretStoreForTests(std::make_unique<MemoryStore>());
    controller->setGoogleBrowserForTests(signInInBrowser);
    return controller;
}

bool signedIn(VaultController& controller)
{
    QSignalSpy connected(&controller, &VaultController::googleDriveConnected);
    QSignalSpy shown(&controller, &VaultController::webDavFolderShown);
    controller.connectGoogleDrive();
    return connected.wait(10000) && connected.constFirst().at(0).toBool() && (shown.size() > 0 || shown.wait(10000));
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

bool addHere(VaultController& controller, const QString& title)
{
    QVariantMap item = controller.newLogin();
    item.insert(QStringLiteral("title"), title);
    return controller.saveItem(item) && QTest::qWaitFor([&controller]() { return controller.isIdle(); }, 10000);
}

QStringList titlesOfData(const QByteArray& data)
{
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("read.chronovault"));
    if (saveVaultData(file, data) != SaveStatus::Saved) {
        return {QStringLiteral("(not a vault)")};
    }
    const OpenResult opened = openVaultFile(file, "pass");
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
    titles.sort();
    return titles;
}

// The vault's bytes with one more item, as another device writes it.
QByteArray withItem(const QByteArray& data, const QString& title)
{
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("other.chronovault"));
    if (saveVaultData(file, data) != SaveStatus::Saved) {
        return {};
    }
    const OpenResult opened = openVaultFile(file, "pass");
    ContentsResult read = contentsFromJson(opened.contents);
    Item item;
    item.id = QUuid::createUuid();
    item.title = title;
    read.contents.items.append(item);
    if (saveVaultFile(file, contentsToJson(read.contents), opened.key) != SaveStatus::Saved) {
        return {};
    }
    QFile written(file);
    return written.open(QIODevice::ReadOnly) ? written.readAll() : QByteArray();
}

QString vaultId(GoogleDriveTestServer& server)
{
    const auto* file = server.byName(vaultName, server.folderId(QStringLiteral("Chrono Vault")));
    return file ? file->id : QString();
}

// Signed in, and a new vault "Home" made on Google Drive.
bool createdOnDrive(VaultController& controller)
{
    if (!signedIn(controller)) {
        return false;
    }
    controller.createGoogleDriveVault(QStringLiteral("Home"), QStringLiteral("pass"), QStringLiteral("pass"));
    return QTest::qWaitFor([&controller]() { return controller.state() == State::Unlocked; }, 10000) &&
           syncedNow(controller);
}

} // namespace

class TestGoogleDrive : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_google_drive"));
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        QDir(cloudDataFolder(QStringLiteral("googledrive"))).removeRecursively();
    }
    void cleanup() { init(); }
    void cleanupTestCase()
    {
        init();
        QFile::remove(SettingsFile().fileName());
        QFile::remove(VaultsFile().fileName());
    }

#if defined(CHRONO_VAULT_DRIVE_INTERRUPTIONS)
    // Built as test_google_drive_interruptions, so each test stays under 20 seconds (OR-11).
    void a_sync_with_drive_stopped_at_any_step_loses_nothing_data()
    {
        QTest::addColumn<int>("step");
        for (const SyncStep step : {SyncStep::Downloaded, SyncStep::Merged, SyncStep::Uploaded, SyncStep::Verified,
                                    SyncStep::Locked, SyncStep::Moved, SyncStep::BaseWritten}) {
            QTest::newRow(QByteArray::number(static_cast<int>(step)).constData()) << static_cast<int>(step);
        }
    }
    void a_sync_with_drive_stopped_at_any_step_loses_nothing()
    {
        QFETCH(int, step);
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        std::optional<SyncStep> stopAt;
        first->setSyncStepCheckForTests([&stopAt](SyncStep reached) {
            if (stopAt && *stopAt == reached) {
                stopAt.reset();
                return false;
            }
            return true;
        });
        QVERIFY(createdOnDrive(*first));
        const QString id = vaultId(server);
        server.writeVersion(id, withItem(server.vaultData(vaultName), QStringLiteral("Theirs")));
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        stopAt = static_cast<SyncStep>(step);
        QVERIFY(syncedNow(*first));
        QVERIFY(!stopAt.has_value());
        server.writeVersion(id, withItem(server.vaultData(vaultName), QStringLiteral("Theirs again")));
        QVERIFY(syncedNow(*first));
        QVERIFY(syncedNow(*first));
        const QStringList all{QStringLiteral("Mine"), QStringLiteral("Theirs"), QStringLiteral("Theirs again")};
        QCOMPARE(titlesOfData(server.vaultData(vaultName)), all);
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*first), all, 10000);
        QVERIFY(first->syncProblem().isEmpty());
        // No upload of our own left behind.
        for (const auto& file : std::as_const(server.files)) {
            QVERIFY2(!file.name.endsWith(QLatin1String(".uploading")), qPrintable(file.name));
        }
    }

    // A vault without its account in the place record (made before 0.8.117,
    // or Google did not answer) gets it quietly at its next sync, and once
    // known it is not asked again.
    void an_older_vault_gets_its_account_at_its_next_sync()
    {
        GoogleDriveTestServer server;
        server.aboutRefused = true;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        const QString home = first->vaultPath();
        QCOMPARE(first->vaultPlace(home).value(QStringLiteral("account")).toString(), QString());
        QVERIFY(first->syncProblem().isEmpty()); // the sync itself went on
        server.aboutRefused = false;
        QVERIFY(syncedNow(*first));
        QCOMPARE(first->vaultPlace(home).value(QStringLiteral("account")).toString(),
                 GoogleDriveTestServer::emailOf(QStringLiteral("refresh-good")));
        const int asked = server.aboutAsked;
        QVERIFY(syncedNow(*first));
        QCOMPARE(server.aboutAsked, asked);
    }

#else
    void the_tile_is_there_only_with_the_programs_google_client()
    {
        GoogleDriveTestServer server;
        setGoogleForTests({}, QString(), QString());
        bool shown = false;
        for (const CloudKind& kind : cloudKinds()) {
            shown = shown || kind.kind == QLatin1String("googledrive");
        }
        QVERIFY(!shown);
        use(server);
        shown = false;
        for (const CloudKind& kind : cloudKinds()) {
            shown = shown || kind.kind == QLatin1String("googledrive");
        }
        QVERIFY(shown);
    }

    void a_new_vault_goes_into_the_chrono_vault_folder()
    {
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        // Made by Chrono Vault at the top of My Drive (the owner, Q14).
        QVERIFY(!server.folderId(QStringLiteral("Chrono Vault")).isEmpty());
        QCOMPARE(titlesOfData(server.vaultData(vaultName)), QStringList());
        QCOMPARE(first->vaultPlace(first->vaultPath()).value(QStringLiteral("kind")).toString(),
                 QStringLiteral("googledrive"));
        QCOMPARE(first->storageName(), QStringLiteral("Google Drive"));
        QVERIFY(first->syncProblem().isEmpty());
    }

    void two_devices_keep_each_others_changes()
    {
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        auto second = device();
        QVERIFY(signedIn(*second));
        QSignalSpy done(second.get(), &VaultController::webDavDone);
        second->openGoogleDriveVault(vaultName);
        QVERIFY(done.wait(10000));
        QVERIFY(done.constFirst().at(0).toBool());
        QTRY_VERIFY_WITH_TIMEOUT(second->state() == State::Locked, 10000);
        second->unlock(QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(second->state() == State::Unlocked, 10000);
        QVERIFY(syncedNow(*second));

        QVERIFY(addHere(*first, QStringLiteral("From the first")));
        QVERIFY(addHere(*second, QStringLiteral("From the second")));
        QVERIFY(syncedNow(*first));
        QVERIFY(syncedNow(*second));
        QVERIFY(syncedNow(*first));
        const QStringList both{QStringLiteral("From the first"), QStringLiteral("From the second")};
        QCOMPARE(titlesOfData(server.vaultData(vaultName)), both);
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*first), both, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*second), both, 10000);
    }

    // The owner's Q2, A: Drive cannot refuse a write when the file changed
    // meanwhile, so a version that came in between is found and merged.
    void a_version_that_came_in_at_the_same_moment_is_merged()
    {
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        const QString id = vaultId(server);
        bool once = true;
        server.beforeNewVersion = [&server, &once, id](const QString& name) {
            if (once && name == vaultName) {
                once = false;
                server.writeVersion(id, withItem(server.vaultData(vaultName), QStringLiteral("Theirs")));
            }
        };
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QVERIFY(!once);
        const QStringList both{QStringLiteral("Mine"), QStringLiteral("Theirs")};
        QCOMPARE(titlesOfData(server.vaultData(vaultName)), both);
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*first), both, 10000);
        QVERIFY(first->syncProblem().isEmpty());
    }

    void a_sign_in_that_ran_out_is_renewed_without_asking()
    {
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        server.expireTokens();
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QCOMPARE(titlesOfData(server.vaultData(vaultName)), QStringList{QStringLiteral("Mine")});
        QVERIFY(!first->cloudSignInNeeded());
        QVERIFY(first->syncProblem().isEmpty());
    }

    void access_taken_away_asks_to_sign_in_again()
    {
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        server.refreshToken = QStringLiteral("refresh-after-the-person-took-access-away");
        server.expireTokens();
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QVERIFY(first->cloudSignInNeeded());
        QVERIFY(first->syncProblem().contains(QStringLiteral("renewed")));
        // Signed in again in the browser: the change goes up.
        first->signInToCloudAgain();
        QTRY_VERIFY_WITH_TIMEOUT(!first->cloudSignInNeeded() && first->syncForTests() &&
                                     !first->syncForTests()->running() && first->syncProblem().isEmpty(),
                                 10000);
        QTRY_COMPARE_WITH_TIMEOUT(titlesOfData(server.vaultData(vaultName)), QStringList{QStringLiteral("Mine")}, 10000);
    }

    void no_network_is_said_and_nothing_is_lost()
    {
        auto server = std::make_unique<GoogleDriveTestServer>();
        use(*server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        server.reset(); // Google cannot be reached
        QVERIFY(addHere(*first, QStringLiteral("Mine")));
        QVERIFY(syncedNow(*first));
        QVERIFY(first->syncProblem().contains(QStringLiteral("Google Drive cannot be reached")));
        QTRY_COMPARE_WITH_TIMEOUT(shownTitles(*first), QStringList{QStringLiteral("Mine")}, 10000);
    }

    void only_the_newest_versions_are_kept()
    {
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        for (int round = 0; round < GoogleDriveStorage::keptVersions + 3; ++round) {
            QVERIFY(addHere(*first, QStringLiteral("Item %1").arg(round)));
            QVERIFY(syncedNow(*first));
        }
        QTRY_VERIFY_WITH_TIMEOUT(server.keptVersions(vaultId(server)) <= GoogleDriveStorage::keptVersions + 1, 10000);
    }

    void disconnect_leaves_the_vault_on_drive()
    {
        GoogleDriveTestServer server;
        use(server);
        auto first = device();
        QVERIFY(createdOnDrive(*first));
        const QString workingCopy = first->vaultPath();
        const QString id = vaultId(server);
        QSignalSpy done(first.get(), &VaultController::webDavDone);
        first->disconnectVault(workingCopy);
        QCOMPARE(done.count(), 1); // done at once: nothing is sent to Drive
        QVERIFY(!cloudPlaceOf(workingCopy));
        QVERIFY(QFile::exists(workingCopy));                    // this device's copy stays
        QVERIFY(server.byId(id) && !server.byId(id)->trashed); // and the one on Drive (D-060)
    }

    void a_vault_of_this_device_moves_to_drive()
    {
        GoogleDriveTestServer server;
        use(server);
        QTemporaryDir folder;
        const QString from = folder.filePath(vaultName);
        auto controller = device();
        controller->createVault(QUrl::fromLocalFile(from), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);
        QVERIFY(addHere(*controller, QStringLiteral("Mine")));
        QVERIFY(signedIn(*controller));
        QSignalSpy done(controller.get(), &VaultController::moveDone);
        controller->moveVaultToGoogleDrive(from);
        QVERIFY(done.wait(10000));
        QVERIFY2(done.constFirst().at(0).toBool(), qPrintable(done.constFirst().at(1).toString()));
        QCOMPARE(titlesOfData(server.vaultData(vaultName)), QStringList{QStringLiteral("Mine")});
        QTRY_VERIFY_WITH_TIMEOUT(!QFile::exists(from), 10000);
        QCOMPARE(controller->vaults().size(), 1);
        QCOMPARE(cloudPlaceOf(controller->vaults().constFirst())->kind, QStringLiteral("googledrive"));
    }

    // A vault keeps the Google account it was made with; every sign-in in the
    // Google Drive window opens the browser with Google's account chooser
    // (the owner, P86; D-080 point 13).
    void two_google_accounts_in_one_run_each_vault_with_its_own()
    {
        GoogleDriveTestServer server;
        server.eachSignInItsOwn = true;
        use(server);
        auto controller = device();
        QList<QUrl> addresses;
        controller->setGoogleBrowserForTests([&addresses](const QUrl& address) {
            addresses.append(address);
            signInInBrowser(address);
        });
        QVERIFY(createdOnDrive(*controller));
        const QString home = controller->vaultPath();
        QVERIFY(signedIn(*controller));
        controller->createGoogleDriveVault(QStringLiteral("Work"), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked && controller->vaultPath() != home, 10000);
        QVERIFY(syncedNow(*controller));
        const QString work = controller->vaultPath();

        QCOMPARE(addresses.size(), 2);
        for (const QUrl& address : addresses) {
            const QString prompt = QUrlQuery(address).queryItemValue(QStringLiteral("prompt"), QUrl::FullyDecoded);
            QVERIFY2(prompt.split(QLatin1Char(' ')).contains(QStringLiteral("select_account")), qPrintable(prompt));
        }
        QCOMPARE(server.refreshTokensGiven,
                 (QStringList{QStringLiteral("refresh-account-1"), QStringLiteral("refresh-account-2")}));

        // "Work" syncs with the second account.
        server.expireTokens();
        server.refreshTokensUsed.clear();
        QVERIFY(addHere(*controller, QStringLiteral("At work")));
        QVERIFY(syncedNow(*controller));
        QVERIFY(!server.refreshTokensUsed.isEmpty());
        QCOMPARE(server.refreshTokensUsed.filter(QStringLiteral("refresh-account-2")).size(),
                 server.refreshTokensUsed.size());
        QCOMPARE(titlesOfData(server.vaultData(QStringLiteral("Work.chronovault"))), QStringList{QStringLiteral("At work")});

        // "Home" syncs with the first, without asking again.
        controller->switchToVault(home);
        QTRY_VERIFY_WITH_TIMEOUT(controller->vaultPath() == home && (controller->state() == State::Unlocked ||
                                                                     controller->state() == State::Locked),
                                 10000);
        if (controller->state() == State::Locked) {
            controller->unlock(QStringLiteral("pass"));
        }
        QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);
        server.expireTokens();
        server.refreshTokensUsed.clear();
        QVERIFY(addHere(*controller, QStringLiteral("At home")));
        QVERIFY(syncedNow(*controller));
        QVERIFY(!server.refreshTokensUsed.isEmpty());
        QCOMPARE(server.refreshTokensUsed.filter(QStringLiteral("refresh-account-1")).size(),
                 server.refreshTokensUsed.size());
        QCOMPARE(titlesOfData(server.vaultData(vaultName)), QStringList{QStringLiteral("At home")});
        QCOMPARE(addresses.size(), 2);
        QVERIFY(controller->syncProblem().isEmpty());

        // Each row shows its own account (the owner, 27 November 2026).
        QTRY_COMPARE_WITH_TIMEOUT(controller->vaultPlace(home).value(QStringLiteral("account")).toString(),
                                  GoogleDriveTestServer::emailOf(QStringLiteral("refresh-account-1")), 10000);
        QCOMPARE(controller->vaultPlace(work).value(QStringLiteral("account")).toString(),
                 GoogleDriveTestServer::emailOf(QStringLiteral("refresh-account-2")));
    }
#endif
};

QTEST_MAIN(TestGoogleDrive)
#include "test_google_drive.moc"
