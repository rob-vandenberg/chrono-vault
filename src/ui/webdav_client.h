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
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>
#include <optional>

// --- Version ------------------------------------------------------------
// webdav_client.h 0.5.75

// --- Version history ----------------------------------------------------
// v0.5.75: A move that never replaces a file already there (D-060)
// v0.3.63: A file's time of change, for the file dialog (D-057)
// v0.3.62: Making a folder, for the folder browser (D-057)
// v0.3.60: Initial release (D-057)

class QNetworkReply;
class QNetworkRequest;

namespace chrono_vault {

// Where a vault is kept on a WebDAV server (D-057): the folder, the account,
// and the server's certificate when the person trusted one the system does
// not (its SHA-256, in hexadecimal).
struct WebDavAccount
{
    QUrl folder;
    QString user;
    QString password;
    QByteArray trustedCertificate;
};

enum class WebDavError {
    None,
    NoConnection,   // no network, the server not found or not answering
    Unauthorized,   // the user name or password is wrong
    NotFound,       // the folder or file is not there
    Certificate,    // a certificate the system does not trust, not trusted by the person either
    Changed,        // a condition failed: the file changed meanwhile
    Locked,         // another device holds the file's lock
    NotSupported,   // the server does not offer what was asked (for example LOCK)
    TooLarge,       // larger than a vault may be (OR-4)
    Damaged,        // an answer that cannot be understood
    Server,         // any other refusal of the server
};

// The answer to one request. Nothing in it is trusted beyond its checks.
struct WebDavReply
{
    WebDavError error = WebDavError::None;
    int status = 0;            // the HTTP status, 0 when none came
    QByteArray body;           // a file read, or a lock's token
    QByteArray tag;            // the file's version tag (ETag), when given
    QByteArray certificate;    // the untrusted certificate's SHA-256 (hex), with Certificate
    qint64 size = -1;          // from tagOf: the file's length, -1 when not said
    [[nodiscard]] bool ok() const { return error == WebDavError::None; }
};

// One entry of a folder.
struct WebDavEntry
{
    QString name;
    bool isFolder = false;
    QByteArray tag;
    qint64 size = -1;
    QDateTime modified; // invalid when not said
};

// The few WebDAV requests Chrono Vault needs, made with Qt Network and never
// waited for on the window's thread (OR-5): each answer comes to its
// function on the thread that made the client.
class WebDavClient : public QObject
{
    Q_OBJECT

public:
    using Done = std::function<void(const WebDavReply&)>;
    using Listed = std::function<void(const WebDavReply&, const QList<WebDavEntry>&)>;

    explicit WebDavClient(WebDavAccount account, QObject* parent = nullptr);

    [[nodiscard]] const WebDavAccount& account() const { return place; }

    // The files of the folder (PROPFIND, depth 1).
    void list(Listed done);
    // A file of the folder; with ifNotTag, NotModified comes back as
    // status 304 and no body.
    void get(const QString& name, const QByteArray& ifNotTag, Done done);
    void put(const QString& name, const QByteArray& data, Done done);
    // Moves one file of the folder over another; with a lock's token, the
    // move is made under that lock.
    // With overwrite false, a file already at "to" is never replaced: the
    // server answers 412 (D-060).
    void move(const QString& from, const QString& to, const QByteArray& lockToken, Done done, bool overwrite = true);
    void remove(const QString& name, Done done);
    // A new folder in the folder (MKCOL).
    void makeFolder(const QString& name, Done done);
    // The file's version tag (PROPFIND, depth 0); NotFound when it is not there.
    void tagOf(const QString& name, Done done);
    // An exclusive write lock for a short while; the token comes back in body.
    void lock(const QString& name, Done done);
    void unlock(const QString& name, const QByteArray& lockToken, Done done);

    // The largest answer read: a vault at most, a little more for a folder's list.
    static constexpr qint64 largestAnswerBytes = 256LL * 1024 * 1024 + 1024 * 1024;

    // The address of a file of the folder.
    [[nodiscard]] QUrl addressOf(const QString& name) const;

private:
    QNetworkReply* send(const QByteArray& verb, const QUrl& address, const QByteArray& body,
                        const QList<std::pair<QByteArray, QByteArray>>& headers);
    void whenDone(QNetworkReply* reply, std::function<void(WebDavReply, QNetworkReply*)> done);

    WebDavAccount place;
    QNetworkAccessManager network;
};

// Reads a PROPFIND answer; nothing when it cannot be understood (OR-4). The
// folder's own entry is left out.
[[nodiscard]] std::optional<QList<WebDavEntry>> readFolderList(const QByteArray& xml, const QUrl& folder);

// A folder address as the client wants it: ending in "/".
[[nodiscard]] QUrl folderAddress(const QUrl& given);

} // namespace chrono_vault
