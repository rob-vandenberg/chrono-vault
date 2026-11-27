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

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>

// --- Version ------------------------------------------------------------
// cloud_storage.h 0.8.117

// --- Version history ----------------------------------------------------
// v0.8.117: signedInAs, shownCloudAccount and cloudAccountKey: the account in the place record (D-080 point 20)
// v0.8.115: Superseded; Google Drive in the list of storages (D-080)
// v0.8.114: Initial release: the base of every cloud storage, and where a vault is kept (D-080)

namespace chrono_vault {

// Why a request to a storage failed. The same for every storage, so the
// sync and the windows never ask which storage it was (D-080).
enum class CloudError {
    None,
    NoConnection,   // no network, the server not found or not answering
    Unauthorized,   // the sign-in is refused or no longer valid
    NotFound,       // the folder or file is not there
    Certificate,    // a certificate the system does not trust, not trusted by the person either
    Changed,        // a condition failed: the file changed meanwhile
    Locked,         // another device holds the file's lock
    NotSupported,   // the storage does not offer what was asked (for example a lock)
    TooLarge,       // larger than a vault may be (OR-4)
    Damaged,        // an answer that cannot be understood
    Server,         // any other refusal of the storage
    Superseded,     // another device's version came in at the same moment: put back, merged in a new round
};

// The answer to one request. Nothing in it is trusted beyond its checks.
struct CloudReply
{
    CloudError error = CloudError::None;
    int status = 0;            // the HTTP status, 0 when none came
    QByteArray body;           // a file read, or a lock's token
    QByteArray tag;            // the file's version tag, when given
    QByteArray certificate;    // the untrusted certificate's SHA-256 (hex), with Certificate
    qint64 size = -1;          // from tagOf: the file's length, -1 when not said
    [[nodiscard]] bool ok() const { return error == CloudError::None; }
};

// One entry of a folder.
struct CloudEntry
{
    QString name;
    bool isFolder = false;
    QByteArray tag;
    qint64 size = -1;
    QDateTime modified; // invalid when not said
};

// One cloud storage, as the sync and the windows use it (the owner's design,
// D-080): every storage is a class derived from this one, and the sync and
// merge are one class, CloudSync, that works with any of them. Each request
// answers its function later, on the thread that made the storage, and is
// never waited for on the window's thread (OR-5).
class CloudStorage : public QObject
{
    Q_OBJECT

public:
    using Done = std::function<void(const CloudReply&)>;
    using Listed = std::function<void(const CloudReply&, const QList<CloudEntry>&)>;

    explicit CloudStorage(QObject* parent = nullptr) : QObject(parent) {}

    // Signs in with what the storage was given, and answers whether it
    // works; the folder it is in is checked at the same time.
    virtual void signIn(Done done) = 0;
    // The files and folders of its folder.
    virtual void list(Listed done) = 0;
    // A file of the folder; with ifNotTag, an unchanged file comes back as
    // status 304 and no body.
    virtual void get(const QString& name, const QByteArray& ifNotTag, Done done) = 0;
    virtual void put(const QString& name, const QByteArray& data, Done done) = 0;
    virtual void remove(const QString& name, Done done) = 0;
    virtual void makeFolder(const QString& name, Done done) = 0;
    // The file's version tag; NotFound when it is not there.
    virtual void tagOf(const QString& name, Done done) = 0;
    // Puts the file "from" in place of "to". With a lock's token, under that
    // lock; with overwrite false, a file already at "to" is never replaced
    // (Changed). The sync calls it only after the version tag of "to" was
    // found unchanged, so "replace only if unchanged" is lock, tagOf, move.
    virtual void move(const QString& from, const QString& to, const QByteArray& lockToken, Done done,
                      bool overwrite = true) = 0;
    // A short exclusive lock, the token in body. A storage without locks
    // answers NotSupported, and the version tag is checked right before the
    // move all the same.
    virtual void lock(const QString& name, Done done);
    virtual void unlock(const QString& name, const QByteArray& lockToken, Done done);
    // The account signed in, as the person reads it, in body (UTF-8), for the
    // second line of a vault's row (the owner, 27 November 2026). A storage
    // whose account is already in its place record answers NotSupported.
    virtual void signedInAs(Done done);

    // The largest answer read: a vault at most, a little more for a folder's list.
    static constexpr qint64 largestAnswerBytes = 256LL * 1024 * 1024 + 1024 * 1024;

protected:
    // An answer given later, never inside the call that asked (as a network
    // answer comes).
    void answerLater(Done done, CloudReply reply);
};

// Where one vault is kept in a cloud storage, and how far this device got
// with it. Kept per working copy in chrono-vault.conf under
// "vaults/<kind>"; what the sign-in needs (a password, a token) is not here
// but in the system's store, under "<kind>-<id>".
struct CloudPlace
{
    QString kind;             // the storage: "webdav", ...
    QString id;               // names its secret, its base file and its upload
    QString fileName;         // the vault's file in its folder
    QByteArray tag;           // the storage's version tag at the last sync
    QByteArray syncedStamp;   // the working copy's stamp at the last sync
    QDateTime lastSynced;
    QVariantMap details;      // what only that storage needs, as it keeps it

    [[nodiscard]] QVariantMap toMap() const;
    [[nodiscard]] static CloudPlace fromMap(const QString& kind, const QVariantMap& map);
    [[nodiscard]] QString secretName() const { return kind + QLatin1Char('-') + id; }
};

[[nodiscard]] std::optional<CloudPlace> cloudPlaceOf(const QString& workingCopy);
void rememberCloudPlace(const QString& workingCopy, const CloudPlace& place);
void forgetCloudPlace(const QString& workingCopy);
// The folder of a storage's working copies and base files, in the program's data.
[[nodiscard]] QString cloudDataFolder(const QString& kind);
// The last version this device and the storage agreed on, for merging (D-054).
[[nodiscard]] QString cloudBasePath(const CloudPlace& place);
[[nodiscard]] QString newCloudId();
// Where the vault is, as the person reads it, never with a password.
[[nodiscard]] QString shownCloudAddress(const CloudPlace& place);
// The account the vault is reached with, as the person reads it; "" when not
// known yet. Kept in the place record on this computer, never in the vault.
[[nodiscard]] QString shownCloudAccount(const CloudPlace& place);
// The key of the account in a place's details.
inline const QString cloudAccountKey = QStringLiteral("account");

// One kind of cloud storage, for every window that names one: the Storage
// tiles, "Move to …", the line under the vault's name (D-080). A kind not
// available in this build is not in the list at all (the owner, Q6).
struct CloudKind
{
    QString kind;
    QString identifier;    // the kind in one capitalized word, for the windows' object names
    QString name;          // as the person reads it
    QString tilePicture;   // the Storage tile's picture
    QString placePicture;  // the picture in lists and dropdowns (D-079)
};

[[nodiscard]] QList<CloudKind> cloudKinds();
[[nodiscard]] std::optional<CloudKind> cloudKindOf(const QString& kind);
// The same list for the windows: kind, name, tile, place.
[[nodiscard]] QVariantList cloudKindsForWindows();

// The storage a place's vault is kept in, signed in with its secret; nothing
// when the kind is not in this build.
[[nodiscard]] std::unique_ptr<CloudStorage> makeCloudStorage(const CloudPlace& place, const QString& secret);

} // namespace chrono_vault
