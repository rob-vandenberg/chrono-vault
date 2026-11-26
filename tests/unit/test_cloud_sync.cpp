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
#include "cloud_sync.h"
#include "settings_files.h"
#include "vault_contents.h"
#include "vault_file.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QUuid>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_cloud_sync.cpp 0.8.114

// --- Version history ----------------------------------------------------
// v0.8.114: Initial release: CloudSync with a storage of the test's own, stopped at every step (D-080)

using namespace chrono_vault;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};
const QString vaultName = QStringLiteral("Home.chronovault");

// A cloud storage in memory, without locks, as a storage such as Google
// Drive has none: every answer comes later, as from a network.
class MemoryStorage : public CloudStorage
{
public:
    QHash<QString, QByteArray> files;
    QHash<QString, QByteArray> tags;

    void setFile(const QString& name, const QByteArray& data)
    {
        files.insert(name, data);
        tags.insert(name, QByteArray::number(++version));
    }

    void signIn(Done done) override { answerLater(std::move(done), CloudReply{}); }
    void list(Listed done) override
    {
        QList<CloudEntry> entries;
        for (auto file = files.cbegin(); file != files.cend(); ++file) {
            entries.append(CloudEntry{file.key(), false, tags.value(file.key()), file.value().size(), {}});
        }
        QMetaObject::invokeMethod(
            this, [done = std::move(done), entries]() { done(CloudReply{}, entries); }, Qt::QueuedConnection);
    }
    void get(const QString& name, const QByteArray& ifNotTag, Done done) override
    {
        CloudReply reply;
        if (!files.contains(name)) {
            reply.error = CloudError::NotFound;
            reply.status = 404;
        } else if (!ifNotTag.isEmpty() && ifNotTag == tags.value(name)) {
            reply.status = 304;
            reply.tag = tags.value(name);
        } else {
            reply.status = 200;
            reply.body = files.value(name);
            reply.tag = tags.value(name);
        }
        answerLater(std::move(done), reply);
    }
    void put(const QString& name, const QByteArray& data, Done done) override
    {
        setFile(name, data);
        answerLater(std::move(done), CloudReply{CloudError::None, 201, {}, tags.value(name), {}, -1});
    }
    void remove(const QString& name, Done done) override
    {
        CloudReply reply;
        if (files.remove(name) == 0) {
            reply.error = CloudError::NotFound;
        }
        tags.remove(name);
        answerLater(std::move(done), reply);
    }
    void makeFolder(const QString& name, Done done) override
    {
        Q_UNUSED(name)
        answerLater(std::move(done), CloudReply{});
    }
    void tagOf(const QString& name, Done done) override
    {
        CloudReply reply;
        if (!files.contains(name)) {
            reply.error = CloudError::NotFound;
        } else {
            reply.tag = tags.value(name);
            reply.size = files.value(name).size();
        }
        answerLater(std::move(done), reply);
    }
    void move(const QString& from, const QString& to, const QByteArray& lockToken, Done done, bool overwrite) override
    {
        Q_UNUSED(lockToken)
        CloudReply reply;
        if (!files.contains(from)) {
            reply.error = CloudError::NotFound;
        } else if (!overwrite && files.contains(to)) {
            reply.error = CloudError::Changed;
        } else {
            setFile(to, files.take(from));
            tags.remove(from);
        }
        answerLater(std::move(done), reply);
    }

private:
    int version = 0;
};

QByteArray vaultBytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QStringList titlesOf(const QByteArray& data, const VaultKey& key)
{
    OpenResult opened = openVaultDataWithKey(data, key);
    if (opened.status != OpenStatus::Opened) {
        return {QStringLiteral("(does not open)")};
    }
    QStringList titles;
    for (const Item& item : contentsFromJson(opened.contents).contents.items) {
        titles << item.title;
    }
    titles.sort();
    return titles;
}

// Adds an item to the vault in data and gives the new bytes.
QByteArray withItem(const QByteArray& data, const VaultKey& key, const QString& title)
{
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("edit.chronovault"));
    if (saveVaultData(file, data) != SaveStatus::Saved) {
        return {};
    }
    OpenResult opened = openVaultDataWithKey(data, key);
    ContentsResult read = contentsFromJson(opened.contents);
    Item item;
    item.id = QUuid::createUuid();
    item.title = title;
    read.contents.items.append(item);
    if (saveVaultFile(file, contentsToJson(read.contents), key) != SaveStatus::Saved) {
        return {};
    }
    return vaultBytes(file);
}

bool synced(CloudSync& sync, SyncOutcome expected)
{
    QSignalSpy finished(&sync, &CloudSync::finished);
    sync.start();
    if (!finished.wait(10000)) {
        return false;
    }
    return finished.constFirst().at(0).value<SyncOutcome>() == expected;
}

} // namespace

class TestCloudSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_cloud_sync"));
        qRegisterMetaType<SyncOutcome>();
    }
    void init()
    {
        VaultsFile().clear();
        QDir(cloudDataFolder(QStringLiteral("memory"))).removeRecursively();
    }
    void cleanup() { init(); }
    void cleanupTestCase()
    {
        init();
        QFile::remove(VaultsFile().fileName());
    }

    void a_sync_with_any_storage_stopped_at_any_step_loses_nothing_data()
    {
        QTest::addColumn<int>("step");
        for (const SyncStep step : {SyncStep::Downloaded, SyncStep::Merged, SyncStep::Uploaded, SyncStep::Verified,
                                    SyncStep::Locked, SyncStep::Moved, SyncStep::BaseWritten}) {
            QTest::newRow(QByteArray::number(static_cast<int>(step)).constData()) << static_cast<int>(step);
        }
    }
    void a_sync_with_any_storage_stopped_at_any_step_loses_nothing()
    {
        QFETCH(int, step);
        QTemporaryDir folder;
        const QString workingCopy = folder.filePath(vaultName);
        const VaultKey key = makeVaultKey("pass", quick);
        QVERIFY(key.isValid());
        QCOMPARE(saveVaultFile(workingCopy, contentsToJson({}), key), SaveStatus::Saved);

        auto owned = std::make_unique<MemoryStorage>();
        MemoryStorage* storage = owned.get();
        CloudPlace place;
        place.kind = QStringLiteral("memory");
        place.id = newCloudId();
        place.fileName = vaultName;
        QThreadPool pool;
        CloudSync sync(workingCopy, place, std::move(owned), key, &pool);
        std::optional<SyncStep> stopAt;
        sync.setStepCheckForTests([&stopAt](SyncStep reached) {
            if (stopAt && *stopAt == reached) {
                stopAt.reset();
                return false;
            }
            return true;
        });

        // The first upload, then a change here and one on another device.
        QVERIFY(synced(sync, SyncOutcome::Synced));
        QVERIFY(storage->files.contains(vaultName));
        storage->setFile(vaultName, withItem(storage->files.value(vaultName), key, QStringLiteral("Theirs")));
        const QByteArray mine = withItem(vaultBytes(workingCopy), key, QStringLiteral("Mine"));
        QVERIFY(!mine.isEmpty());
        QCOMPARE(saveVaultData(workingCopy, mine), SaveStatus::Saved);

        // Stopped at the step: both copies still open, and nothing of
        // either is lost (OR-15.4).
        stopAt = static_cast<SyncStep>(step);
        QVERIFY(synced(sync, SyncOutcome::Interrupted));
        QVERIFY(!stopAt.has_value());
        QVERIFY(!titlesOf(vaultBytes(workingCopy), key).contains(QStringLiteral("(does not open)")));
        QVERIFY(!titlesOf(storage->files.value(vaultName), key).contains(QStringLiteral("(does not open)")));

        // Another device changes it again; the next syncs finish.
        storage->setFile(vaultName, withItem(storage->files.value(vaultName), key, QStringLiteral("Theirs again")));
        QVERIFY(synced(sync, SyncOutcome::Synced));
        QVERIFY(synced(sync, SyncOutcome::Synced));
        const QStringList all{QStringLiteral("Mine"), QStringLiteral("Theirs"), QStringLiteral("Theirs again")};
        QCOMPARE(titlesOf(storage->files.value(vaultName), key), all);
        QCOMPARE(titlesOf(vaultBytes(workingCopy), key), all);
        QCOMPARE(storage->files.size(), 1); // no upload left behind
    }
};

QTEST_MAIN(TestCloudSync)
#include "test_cloud_sync.moc"
