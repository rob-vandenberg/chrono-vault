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
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

// --- Version ------------------------------------------------------------
// google_drive_test_server.h 0.8.117

// --- Version history ----------------------------------------------------
// v0.8.117: Drive's "about": the email of the account the access token belongs to
// v0.8.116: Each sign-in its own account when asked, and the refresh tokens used (D-080 point 13)
// v0.8.115: Initial release: Google's sign-in and the part of the Drive API Chrono Vault uses (D-080)

class QTcpServer;
class QTcpSocket;

// Google's token endpoint and the few Drive requests Chrono Vault makes, on
// this computer, for the tests: files with versions, folders, the trash.
class GoogleDriveTestServer : public QObject
{
    Q_OBJECT

public:
    struct Revision
    {
        QString id;
        QByteArray data;
        bool keep = false;
        QString modified;
    };
    struct File
    {
        QString id;
        QString name;
        QString parent;
        bool folder = false;
        bool trashed = false;
        QList<Revision> revisions;
    };

    explicit GoogleDriveTestServer(QObject* parent = nullptr);
    ~GoogleDriveTestServer() override;

    [[nodiscard]] QUrl base() const;
    // The refresh token Google gives at sign-in; another one is refused, as
    // when the person took Chrono Vault's access away.
    QString refreshToken = QStringLiteral("refresh-good");
    // Each sign-in as another Google account: its own refresh token, every
    // one given so far still accepted.
    bool eachSignInItsOwn = false;
    QStringList refreshTokensGiven;
    // Every refresh token a renewal came with, in order.
    QStringList refreshTokensUsed;
    // Drive's "about" answers the email of the sign-in: "<refresh token>@example.com".
    [[nodiscard]] static QString emailOf(const QString& refreshToken) { return refreshToken + QStringLiteral("@example.com"); }
    bool aboutRefused = false;
    int aboutAsked = 0;
    // Every access token given so far stops working (it ran out).
    void expireTokens() { ++tokenGeneration; }
    // Called when a new version of a file is about to be written: a test may
    // write one of its own first, as another device at the same moment.
    std::function<void(const QString& name)> beforeNewVersion;

    QList<File> files;
    int tokensGiven = 0;

    [[nodiscard]] File* byName(const QString& name, const QString& parent);
    [[nodiscard]] File* byId(const QString& id);
    [[nodiscard]] QString folderId(const QString& name, const QString& parent = QStringLiteral("root"));
    // The vault named so in the "Chrono Vault" folder.
    [[nodiscard]] QByteArray vaultData(const QString& name);
    // Another device writes a version, kept as Chrono Vault keeps its own.
    void writeVersion(const QString& id, const QByteArray& data, bool keep = true);
    [[nodiscard]] int keptVersions(const QString& id);

private:
    void serve(QTcpSocket* socket);
    void answer(QTcpSocket* socket, const QByteArray& method, const QUrl& target,
                const QHash<QByteArray, QByteArray>& headers, const QByteArray& body);
    [[nodiscard]] QString newId();
    [[nodiscard]] QString nextTime();

    QTcpServer* server;
    int counter = 0;
    int tokenGeneration = 0;
    QHash<QString, QString> accountOfAccess; // access token to the refresh token it came from
};
