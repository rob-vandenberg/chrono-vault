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


#include "google_drive_test_server.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimeZone>
#include <QUrlQuery>

// --- Version ------------------------------------------------------------
// google_drive_test_server.cpp 0.8.116

// --- Version history ----------------------------------------------------
// v0.8.116: Each sign-in its own account when asked, and the refresh tokens used (D-080 point 13)
// v0.8.115: Initial release: Google's sign-in and the part of the Drive API Chrono Vault uses (D-080)

namespace {

const QString folderType = QStringLiteral("application/vnd.google-apps.folder");
const char* const bufferProperty = "testServerBuffer";

QByteArray reply(int status, const QByteArray& body, const QByteArray& type = "application/json")
{
    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + " X\r\n";
    head += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n";
    return head + body;
}

QByteArray json(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QString unescaped(QString text)
{
    text.replace(QStringLiteral("\\'"), QStringLiteral("'"));
    text.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
    return text;
}

} // namespace

GoogleDriveTestServer::GoogleDriveTestServer(QObject* parent) : QObject(parent), server(new QTcpServer(this))
{
    connect(server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket* socket = server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { serve(socket); });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    server->listen(QHostAddress::LocalHost, 0);
}

GoogleDriveTestServer::~GoogleDriveTestServer() = default;

QUrl GoogleDriveTestServer::base() const
{
    return QUrl(QStringLiteral("http://127.0.0.1:%1/").arg(server->serverPort()));
}

QString GoogleDriveTestServer::newId()
{
    return QString::number(++counter);
}

QString GoogleDriveTestServer::nextTime()
{
    static const qint64 start = QDateTime(QDate(2026, 11, 27), QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch();
    return QDateTime::fromMSecsSinceEpoch(start + (++counter), QTimeZone::UTC).toString(Qt::ISODateWithMs);
}

GoogleDriveTestServer::File* GoogleDriveTestServer::byName(const QString& name, const QString& parent)
{
    for (File& file : files) {
        if (file.name == name && file.parent == parent && !file.trashed) {
            return &file;
        }
    }
    return nullptr;
}

GoogleDriveTestServer::File* GoogleDriveTestServer::byId(const QString& id)
{
    for (File& file : files) {
        if (file.id == id) {
            return &file;
        }
    }
    return nullptr;
}

QString GoogleDriveTestServer::folderId(const QString& name, const QString& parent)
{
    const File* folder = byName(name, parent);
    return folder && folder->folder ? folder->id : QString();
}

QByteArray GoogleDriveTestServer::vaultData(const QString& name)
{
    const File* vault = byName(name, folderId(QStringLiteral("Chrono Vault")));
    return vault && !vault->revisions.isEmpty() ? vault->revisions.last().data : QByteArray();
}

void GoogleDriveTestServer::writeVersion(const QString& id, const QByteArray& data, bool keep)
{
    if (File* file = byId(id)) {
        file->revisions.append(Revision{QStringLiteral("r") + newId(), data, keep, nextTime()});
    }
}

int GoogleDriveTestServer::keptVersions(const QString& id)
{
    int kept = 0;
    if (const File* file = byId(id)) {
        for (const Revision& revision : file->revisions) {
            kept += revision.keep ? 1 : 0;
        }
    }
    return kept;
}

void GoogleDriveTestServer::serve(QTcpSocket* socket)
{
    QByteArray buffer = socket->property(bufferProperty).toByteArray() + socket->readAll();
    for (;;) {
        const qsizetype headEnd = buffer.indexOf("\r\n\r\n");
        if (headEnd < 0) {
            break;
        }
        const QList<QByteArray> lines = buffer.left(headEnd).split('\n');
        const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
        QHash<QByteArray, QByteArray> headers;
        for (qsizetype index = 1; index < lines.size(); ++index) {
            const qsizetype colon = lines.at(index).indexOf(':');
            if (colon > 0) {
                headers.insert(lines.at(index).left(colon).trimmed().toLower(), lines.at(index).mid(colon + 1).trimmed());
            }
        }
        const qsizetype length = headers.value("content-length").toLongLong();
        if (buffer.size() < headEnd + 4 + length) {
            break;
        }
        const QByteArray body = buffer.mid(headEnd + 4, length);
        buffer.remove(0, headEnd + 4 + length);
        answer(socket, first.value(0), base().resolved(QUrl(QString::fromLatin1(first.value(1)))), headers, body);
    }
    socket->setProperty(bufferProperty, buffer);
}

void GoogleDriveTestServer::answer(QTcpSocket* socket, const QByteArray& method, const QUrl& target,
                                   const QHash<QByteArray, QByteArray>& headers, const QByteArray& body)
{
    const QString path = target.path();
    const QUrlQuery query(target);

    if (path == QLatin1String("/token")) {
        const QUrlQuery form(QString::fromLatin1(body));
        const QString grant = form.queryItemValue(QStringLiteral("grant_type"));
        QJsonObject token{{QStringLiteral("access_token"),
                           QStringLiteral("access-%1-%2").arg(tokenGeneration).arg(++tokensGiven)},
                          {QStringLiteral("expires_in"), 3600},
                          {QStringLiteral("token_type"), QStringLiteral("Bearer")},
                          {QStringLiteral("scope"), QStringLiteral("https://www.googleapis.com/auth/drive.file")}};
        if (grant == QLatin1String("authorization_code")) {
            QString given = refreshToken;
            if (eachSignInItsOwn) {
                given = QStringLiteral("refresh-account-%1").arg(refreshTokensGiven.size() + 1);
                refreshTokensGiven.append(given);
            }
            token.insert(QStringLiteral("refresh_token"), given);
        } else {
            const QString used = form.queryItemValue(QStringLiteral("refresh_token"), QUrl::FullyDecoded);
            refreshTokensUsed.append(used);
            if (used != refreshToken && !(eachSignInItsOwn && refreshTokensGiven.contains(used))) {
                socket->write(reply(400, json({{QStringLiteral("error"), QStringLiteral("invalid_grant")}})));
                return;
            }
        }
        socket->write(reply(200, json(token)));
        return;
    }

    // Every Drive request carries an access token that has not run out.
    const QByteArray authorization = headers.value("authorization");
    if (!authorization.startsWith("Bearer access-" + QByteArray::number(tokenGeneration) + "-")) {
        socket->write(reply(401, json({{QStringLiteral("error"), QStringLiteral("unauthorized")}})));
        return;
    }
    const auto described = [](const File& file) {
        QJsonObject object{{QStringLiteral("id"), file.id},
                           {QStringLiteral("name"), file.name},
                           {QStringLiteral("mimeType"), file.folder ? folderType : QStringLiteral("application/octet-stream")}};
        if (!file.folder && !file.revisions.isEmpty()) {
            const Revision& head = file.revisions.last();
            object.insert(QStringLiteral("headRevisionId"), head.id);
            object.insert(QStringLiteral("size"), QString::number(head.data.size()));
            object.insert(QStringLiteral("md5Checksum"),
                          QString::fromLatin1(QCryptographicHash::hash(head.data, QCryptographicHash::Md5).toHex()));
            object.insert(QStringLiteral("modifiedTime"), head.modified);
        }
        return object;
    };
    const bool upload = path.startsWith(QLatin1String("/upload/drive/v3/files"));
    const QString rest = path.mid(upload ? QStringLiteral("/upload/drive/v3/files").size()
                                         : QStringLiteral("/drive/v3/files").size());
    const QStringList parts = rest.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    if (method == "GET" && parts.isEmpty()) {
        // A search: by name, folder and kind, as Chrono Vault asks.
        const QString search = query.queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded);
        static const QRegularExpression named(QStringLiteral("name = '((?:[^'\\\\]|\\\\.)*)'"));
        static const QRegularExpression within(QStringLiteral("'([^']*)' in parents"));
        const auto nameMatch = named.match(search);
        const QString parent = within.match(search).captured(1);
        const bool onlyFolders = search.contains(QStringLiteral("mimeType = "));
        const bool onlyFiles = search.contains(QStringLiteral("mimeType != "));
        QJsonArray found;
        for (const File& file : std::as_const(files)) {
            if (file.trashed || file.parent != parent || (onlyFolders && !file.folder) || (onlyFiles && file.folder) ||
                (nameMatch.hasMatch() && file.name != unescaped(nameMatch.captured(1)))) {
                continue;
            }
            found.append(described(file));
        }
        socket->write(reply(200, json({{QStringLiteral("files"), found}})));
        return;
    }
    if (method == "POST" && parts.isEmpty() && !upload) {
        const QJsonObject metadata = QJsonDocument::fromJson(body).object();
        File made{newId(), metadata.value(QStringLiteral("name")).toString(),
                  metadata.value(QStringLiteral("parents")).toArray().at(0).toString(),
                  metadata.value(QStringLiteral("mimeType")).toString() == folderType, false, {}};
        files.append(made);
        socket->write(reply(200, json(described(made))));
        return;
    }
    if (method == "POST" && parts.isEmpty() && upload) {
        // A new file: its metadata and its bytes, as multipart/related.
        const QByteArray type = headers.value("content-type");
        const QByteArray boundary = "--" + type.mid(type.indexOf("boundary=") + 9);
        const qsizetype firstPart = body.indexOf("\r\n\r\n") + 4;
        const qsizetype firstEnd = body.indexOf("\r\n" + boundary, firstPart);
        const QJsonObject metadata = QJsonDocument::fromJson(body.mid(firstPart, firstEnd - firstPart)).object();
        const qsizetype secondPart = body.indexOf("\r\n\r\n", firstEnd) + 4;
        const qsizetype secondEnd = body.indexOf("\r\n" + boundary + "--", secondPart);
        if (beforeNewVersion) {
            beforeNewVersion(metadata.value(QStringLiteral("name")).toString());
        }
        File made{newId(), metadata.value(QStringLiteral("name")).toString(),
                  metadata.value(QStringLiteral("parents")).toArray().at(0).toString(), false, false, {}};
        made.revisions.append(Revision{QStringLiteral("r") + newId(), body.mid(secondPart, secondEnd - secondPart),
                                       query.queryItemValue(QStringLiteral("keepRevisionForever")) == QLatin1String("true"),
                                       nextTime()});
        files.append(made);
        socket->write(reply(200, json(described(made))));
        return;
    }
    File* file = parts.isEmpty() ? nullptr : byId(parts.first());
    if (!file) {
        socket->write(reply(404, json({{QStringLiteral("error"), QStringLiteral("notFound")}})));
        return;
    }
    if (parts.size() == 1 && method == "GET") {
        if (query.queryItemValue(QStringLiteral("alt")) == QLatin1String("media") && !file->revisions.isEmpty()) {
            socket->write(reply(200, file->revisions.last().data, "application/octet-stream"));
        } else {
            socket->write(reply(200, json(described(*file))));
        }
        return;
    }
    if (parts.size() == 1 && method == "PATCH" && upload) {
        if (beforeNewVersion) {
            beforeNewVersion(file->name);
            file = byId(parts.first());
        }
        file->revisions.append(Revision{QStringLiteral("r") + newId(), body,
                                        query.queryItemValue(QStringLiteral("keepRevisionForever")) == QLatin1String("true"),
                                        nextTime()});
        socket->write(reply(200, json(described(*file))));
        return;
    }
    if (parts.size() == 1 && method == "PATCH") {
        const QJsonObject change = QJsonDocument::fromJson(body).object();
        if (change.contains(QStringLiteral("name"))) {
            file->name = change.value(QStringLiteral("name")).toString();
        }
        if (change.contains(QStringLiteral("trashed"))) {
            file->trashed = change.value(QStringLiteral("trashed")).toBool();
        }
        socket->write(reply(200, json(described(*file))));
        return;
    }
    if (parts.size() == 1 && method == "DELETE") {
        const QString id = file->id;
        files.removeIf([&id](const File& one) { return one.id == id; });
        socket->write(reply(204, QByteArray()));
        return;
    }
    if (parts.size() == 2 && parts.at(1) == QLatin1String("revisions") && method == "GET") {
        QJsonArray listed;
        for (const Revision& revision : std::as_const(file->revisions)) {
            listed.append(QJsonObject{{QStringLiteral("id"), revision.id},
                                      {QStringLiteral("keepForever"), revision.keep},
                                      {QStringLiteral("modifiedTime"), revision.modified}});
        }
        socket->write(reply(200, json({{QStringLiteral("revisions"), listed}})));
        return;
    }
    if (parts.size() == 3 && parts.at(1) == QLatin1String("revisions")) {
        const auto at = std::find_if(file->revisions.begin(), file->revisions.end(),
                                     [&parts](const Revision& one) { return one.id == parts.at(2); });
        if (at == file->revisions.end()) {
            socket->write(reply(404, json({{QStringLiteral("error"), QStringLiteral("notFound")}})));
            return;
        }
        if (method == "DELETE") {
            if (file->revisions.size() > 1) {
                file->revisions.erase(at);
            }
            socket->write(reply(204, QByteArray()));
            return;
        }
        // Only a version kept forever can be read (as Google's documentation says).
        socket->write(at->keep ? reply(200, at->data, "application/octet-stream")
                               : reply(403, json({{QStringLiteral("error"), QStringLiteral("notKept")}})));
        return;
    }
    socket->write(reply(400, json({{QStringLiteral("error"), QStringLiteral("badRequest")}})));
}
