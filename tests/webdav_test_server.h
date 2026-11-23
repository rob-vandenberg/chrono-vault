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
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>

#include <functional>

// --- Version ------------------------------------------------------------
// webdav_test_server.h 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

class QTcpServer;
class QTcpSocket;

// A small WebDAV server of our own, in the test's process, for the tests of
// WebDAV storage (D-057): one folder of files with version tags, Basic
// sign-in, locks, and HTTPS with a self-signed certificate. Only what
// Chrono Vault asks is answered.
class WebDavTestServer : public QObject
{
    Q_OBJECT

public:
    struct File
    {
        QByteArray data;
        QByteArray tag;
        QByteArray lockToken; // empty when not locked
    };

    explicit WebDavTestServer(bool https = false, QObject* parent = nullptr);
    ~WebDavTestServer() override;

    // The folder's address, for example http://127.0.0.1:port/dav/vaults/.
    [[nodiscard]] QUrl folder() const;
    // The certificate's SHA-256, as the client reports it.
    [[nodiscard]] static QByteArray certificateFingerprint();

    QString user = QStringLiteral("person");
    QString password = QStringLiteral("secret");
    bool locksOffered = true;
    // When not 0, every signed-in request is answered with this status.
    int refuseWith = 0;
    // False: no ETag and no time of change, as some simple servers.
    bool tagsOffered = true;
    // Called before each request is answered: a test may change a file
    // there, as another device would.
    std::function<void(const QByteArray& method, const QString& name)> beforeAnswer;

    QHash<QString, File> files; // by name, in the one folder
    QSet<QString> folders;      // the folder's path and its parents exist
    int requests = 0;

    void setFile(const QString& name, const QByteArray& data);
    [[nodiscard]] QByteArray fileData(const QString& name) const { return files.value(name).data; }

private:
    void serve(QTcpSocket* socket);
    void answer(QTcpSocket* socket, const QByteArray& method, const QByteArray& target,
                const QHash<QByteArray, QByteArray>& headers, const QByteArray& body);
    [[nodiscard]] QByteArray newTag();
    [[nodiscard]] QString nameOf(const QByteArray& target, bool* inFolder, bool* isFolder) const;

    QTcpServer* server = nullptr;
    bool secure = false;
    int tagCounter = 0;
    QHash<QTcpSocket*, QByteArray> pending;
};
