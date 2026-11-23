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
#include "webdav_client.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <functional>
#include <optional>

// --- Version ------------------------------------------------------------
// webdav_sync.h 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

class QThreadPool;

namespace chrono_vault {

// Where one vault is kept on a WebDAV server, and how far this device got
// with it (D-057). Kept per working copy in chrono-vault.conf; the password
// is not here but in the system's store (SecretStore), under "webdav-<id>".
struct WebDavPlace
{
    QString id;               // names its password, its base file and its upload
    QUrl folder;
    QString fileName;         // the vault's file in that folder
    QString user;
    QByteArray trustedCertificate;
    QByteArray tag;           // the server's version tag at the last sync
    QByteArray syncedStamp;   // the working copy's stamp at the last sync
    QDateTime lastSynced;

    [[nodiscard]] QVariantMap toMap() const;
    [[nodiscard]] static WebDavPlace fromMap(const QVariantMap& map);
    [[nodiscard]] QString secretName() const { return QStringLiteral("webdav-") + id; }
    // The address shown to the person, without any password.
    [[nodiscard]] QString shownAddress() const;
};

[[nodiscard]] std::optional<WebDavPlace> webDavPlaceOf(const QString& workingCopy);
void rememberWebDavPlace(const QString& workingCopy, const WebDavPlace& place);
void forgetWebDavPlace(const QString& workingCopy);
// The folder of the working copies and base files, in the program's data.
[[nodiscard]] QString webDavDataFolder();
// The last version this device and the server agreed on, for merging (D-054).
[[nodiscard]] QString webDavBasePath(const QString& id);
[[nodiscard]] QString newWebDavId();

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

// One sync of a vault's working copy with its WebDAV server (D-057):
// download when changed, merge as D-054, upload under a temporary name,
// read it back and check it opens, then move it into place under a lock,
// never over a version another device put there meanwhile (OR-15).
class WebDavSync : public QObject
{
    Q_OBJECT

public:
    WebDavSync(QString workingCopy, WebDavPlace place, QString password, const VaultKey& key, QThreadPool* savePool,
               QObject* parent = nullptr);
    ~WebDavSync() override;

    // A sync now; one already running is followed by another when it ends.
    void start();
    [[nodiscard]] bool running() const { return busy; }
    [[nodiscard]] const WebDavPlace& place() const { return where; }
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
    void downloaded(const WebDavReply& answer);
    void prepared(const Prepared& ready);
    void upload();
    void verifyUpload(const WebDavReply& answer);
    void lockForMove();
    void checkTag(const QByteArray& lockToken);
    void changedMeanwhile(const QByteArray& lockToken);
    void moveIntoPlace(const QByteArray& lockToken);
    void recordSynced(const QByteArray& newTag);
    void finish(SyncOutcome outcome, const QByteArray& certificate = {}, const QString& conflictCopy = {});
    void finishFrom(const WebDavReply& answer);
    [[nodiscard]] bool goOn(SyncStep step);
    [[nodiscard]] QString uploadName() const;

    QString copy;
    WebDavPlace where;
    VaultKey key;
    QThreadPool* pool;
    WebDavClient* client;
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
