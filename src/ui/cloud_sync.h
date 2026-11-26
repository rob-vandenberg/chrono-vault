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


#pragma once

#include "vault_file.h"
#include "cloud_storage.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>

// --- Version ------------------------------------------------------------
// cloud_sync.h 0.8.114

// --- Version history ----------------------------------------------------
// v0.8.114: Initial release: the WebDAV sync of D-057, for any cloud storage (D-080)

class QThreadPool;

namespace chrono_vault {

enum class SyncOutcome {
    Synced,
    NoConnection,
    Unauthorized,
    NotFound,      // the folder is not there (any more)
    Certificate,   // an untrusted certificate: the person is asked
    OtherPassword, // the server's version has another master password
    Unreadable,    // the server's version cannot be read: nothing goes over it
    Busy,          // another device is writing it now; the next sync tries again
    TooLarge,
    Server,
    LocalTrouble,  // the working copy cannot be read or written
    Interrupted,   // only from a test that stops a sync on purpose
};

// The steps of a sync, for tests that stop it at each to prove nothing is
// lost (OR-15.4).
enum class SyncStep { Downloaded, Merged, Uploaded, Verified, Locked, Moved, BaseWritten };

// One sync of a vault's working copy with its cloud storage (D-057, D-080):
// download when changed, merge as D-054, upload under a temporary name,
// read it back and check it opens, then move it into place under a lock
// where the storage has locks, never over a version another device put
// there meanwhile (OR-15). The same for every storage (the owner's design).
class CloudSync : public QObject
{
    Q_OBJECT

public:
    CloudSync(QString workingCopy, CloudPlace place, std::unique_ptr<CloudStorage> storage, const VaultKey& key,
              QThreadPool* savePool, QObject* parent = nullptr);
    ~CloudSync() override;

    // A sync now; one already running is followed by another when it ends.
    void start();
    [[nodiscard]] bool running() const { return busy; }
    [[nodiscard]] const CloudPlace& place() const { return where; }
    [[nodiscard]] const QString& workingCopy() const { return copy; }
    void setStepCheckForTests(std::function<bool(SyncStep)> check) { stepCheck = std::move(check); }

signals:
    // conflictCopy: with OtherPassword, where this device's version was kept.
    void finished(chrono_vault::SyncOutcome outcome, const QByteArray& certificate, const QString& conflictCopy);
    // The sync wrote the working copy: the vault shown is read again or merged.
    void workingCopyChanged();

private:
    struct Prepared;
    void round();
    void downloaded(const CloudReply& answer);
    void prepared(const Prepared& ready);
    void upload();
    void verifyUpload(const CloudReply& answer);
    void lockForMove();
    void checkTag(const QByteArray& lockToken);
    void changedMeanwhile(const QByteArray& lockToken);
    void moveIntoPlace(const QByteArray& lockToken);
    void recordSynced(const QByteArray& newTag);
    void finish(SyncOutcome outcome, const QByteArray& certificate = {}, const QString& conflictCopy = {});
    void finishFrom(const CloudReply& answer);
    [[nodiscard]] bool goOn(SyncStep step);
    [[nodiscard]] QString uploadName() const;

    QString copy;
    CloudPlace where;
    VaultKey key;
    QThreadPool* pool;
    std::unique_ptr<CloudStorage> client;
    std::function<bool(SyncStep)> stepCheck;
    bool busy = false;
    bool again = false;
    int rounds = 0;
    // This run's state.
    bool remoteMissing = false;
    QByteArray theirs;       // the server's version, when it changed
    QByteArray theirTag;     // the tag the upload expects to replace
    QByteArray uploading;    // the bytes being put on the server
};

[[nodiscard]] QString syncOutcomeText(SyncOutcome outcome);

} // namespace chrono_vault
