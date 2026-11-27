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

#include "cloud_storage.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>

#include <functional>

// --- Version ------------------------------------------------------------
// google_drive_storage.h 0.8.117

// --- Version history ----------------------------------------------------
// v0.8.117: signedInAs (D-080 point 20)
// v0.8.115: Initial release: Google Drive as a cloud storage, with the permission drive.file only (D-080)

class QNetworkReply;
class QNetworkRequest;
class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;

namespace chrono_vault {

// Where Chrono Vault talks to Google; a test points them at a server of its own.
struct GoogleEndpoints
{
    QUrl authorize;
    QUrl token;
    QUrl api;      // ".../drive/v3/"
    QUrl upload;   // ".../upload/drive/v3/"
};

// Google Drive in this build: the program's client from the build, and the
// endpoints. Without a client the build has no Google Drive (the owner, Q6).
[[nodiscard]] bool googleDriveAvailable();
[[nodiscard]] GoogleEndpoints googleEndpoints();
void setGoogleForTests(const GoogleEndpoints& endpoints, const QString& clientId, const QString& clientSecret);

// A signed-in Google account and the folder a vault is kept in. The refresh
// token is the secret: kept in the system's store, never in the settings.
struct GoogleDriveAccount
{
    QString refreshToken;
    QString folderId;    // "" until signIn found or made the "Chrono Vault" folder
    QString folderPath;  // as the person reads it: "Chrono Vault/Work"
};

// Google Drive as a cloud storage (D-080). Only the permission drive.file:
// Chrono Vault sees the folders and vaults it made itself, nothing else of
// the person's Drive (the owner, Q15). Files are found by their name in the
// folder. Drive has no write that is refused when the file changed meanwhile,
// so every vault written keeps its version ("Keep Forever"), and right after
// a write Drive's version history is read: when another device's version
// came in between the check and the write, it is put back on top and the
// sync merges it in a new round (Superseded) (the owner, Q2: A).
class GoogleDriveStorage : public CloudStorage
{
    Q_OBJECT

public:
    explicit GoogleDriveStorage(GoogleDriveAccount account, QObject* parent = nullptr);

    [[nodiscard]] const GoogleDriveAccount& account() const { return where; }

    // Gets a fresh access token, then finds the "Chrono Vault" folder at the
    // top of My Drive, or makes it, when no folder was given.
    void signIn(Done done) override;
    void list(Listed done) override;
    void get(const QString& name, const QByteArray& ifNotTag, Done done) override;
    void put(const QString& name, const QByteArray& data, Done done) override;
    void remove(const QString& name, Done done) override;
    void makeFolder(const QString& name, Done done) override;
    void tagOf(const QString& name, Done done) override;
    void move(const QString& from, const QString& to, const QByteArray& lockToken, Done done,
              bool overwrite = true) override;
    // The Google account's email, from Drive's "about" (drive.file is enough).
    void signedInAs(Done done) override;

    // For the file dialog: a subfolder of the folder shown (as the last list
    // named it), ".." for its parent, never above the "Chrono Vault" folder.
    [[nodiscard]] bool enterFolder(const QString& name);
    [[nodiscard]] bool atTop() const { return parents.isEmpty(); }

    // The versions kept of a vault: the newest this many, Chrono Vault's
    // own older ones are deleted.
    static constexpr int keptVersions = 10;

private:
    struct Found
    {
        QString id;
        QByteArray revision;
        QByteArray md5;
        qint64 size = -1;
    };
    using Handled = std::function<void(QNetworkReply*, const CloudReply&)>;

    void withToken(std::function<void(const CloudReply&)> then);
    void send(std::function<QNetworkReply*(const QString& token)> make, Handled handled, bool again = true);
    void find(const QString& name, bool folder, std::function<void(const CloudReply&, const Found&)> then);
    void download(const QString& name, const QByteArray& ifNotTag, int tries, Done done);
    void upload(const QString& id, const QString& name, const QByteArray& data, bool keep,
                std::function<void(const CloudReply&, const QByteArray&)> done);
    void replaceChecked(const Found& target, const QString& fromId, const QByteArray& data, const QByteArray& expected,
                        Done done);
    void checkHistory(const Found& target, const QString& fromId, const QByteArray& expected, const QByteArray& head,
                      int tries, Done done);
    void putBack(const QString& id, const QByteArray& revision, const QString& fromId, Done done);
    void prune(const QString& id, const QJsonArray& revisions, const QByteArray& head);
    void deleteFile(const QString& id, Done done);
    [[nodiscard]] QNetworkRequest request(const QUrl& address, const QString& token) const;
    [[nodiscard]] QUrl apiUrl(const QString& path, const QList<std::pair<QString, QString>>& query = {}) const;
    [[nodiscard]] QUrl uploadUrl(const QString& path, const QList<std::pair<QString, QString>>& query = {}) const;
    [[nodiscard]] static CloudReply replyOf(QNetworkReply* reply);

    GoogleDriveAccount where;
    QNetworkAccessManager network;
    QString accessToken;
    qint64 tokenUntilMs = 0;
    QList<std::pair<QString, QString>> parents; // the folders above the one shown: id, path
    QHash<QString, QString> subfolders;          // the last list's folders: name to id
    QHash<QString, QByteArray> sent;             // the last upload of each name, for move
    QHash<QString, QByteArray> checked;          // the version tagOf found, for the check after a write
};

// The person signs in to Google in the browser; the answer comes back to
// Chrono Vault on this computer (Qt Network Authorization). Only the
// permission drive.file is asked for.
class GoogleSignIn : public QObject
{
    Q_OBJECT

public:
    explicit GoogleSignIn(QObject* parent = nullptr);
    ~GoogleSignIn() override;
    void start();
    // Tests: the address the browser would open goes here, not to a browser.
    void setBrowserForTests(std::function<void(const QUrl&)> open) { browser = std::move(open); }

signals:
    void finished(bool ok, const QString& refreshToken, const QString& problem);

private:
    void end(bool ok, const QString& refreshToken, const QString& problem);
    QOAuth2AuthorizationCodeFlow* flow = nullptr;
    QOAuthHttpServerReplyHandler* handler = nullptr;
    std::function<void(const QUrl&)> browser;
    bool done = false;
};

} // namespace chrono_vault
