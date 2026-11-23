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


#include "webdav_test_server.h"

#include <QCryptographicHash>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUuid>

// --- Version ------------------------------------------------------------
// webdav_test_server.cpp 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

namespace {

const QString folderPath = QStringLiteral("/dav/vaults/");

// A self-signed certificate for 127.0.0.1, made for these tests only.
const char* const certificatePem = R"(-----BEGIN CERTIFICATE-----
MIIBmzCCAUGgAwIBAgIUAot9Xf2B3CBVLrpTDkosabO4FEAwCgYIKoZIzj0EAwIw
FDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MTEyMzE3NDAzNloYDzIxMjYxMDMw
MTc0MDM2WjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwWTATBgcqhkjOPQIBBggqhkjO
PQMBBwNCAASOTwRDTNFIdKkwxa7hNMd2KRTA/klszY4FsbALh47XqguyRYlrMlPs
tUHgdTK0aFLzFPSUFjhhhYc53j1vIn/Qo28wbTAdBgNVHQ4EFgQUWZJlJgffs+8A
nLQOA5cLB3s7oqEwHwYDVR0jBBgwFoAUWZJlJgffs+8AnLQOA5cLB3s7oqEwDwYD
VR0TAQH/BAUwAwEB/zAaBgNVHREEEzARhwR/AAABgglsb2NhbGhvc3QwCgYIKoZI
zj0EAwIDSAAwRQIhANa1xCHKOq9DM9GL062c7m4+kz7grVPWX0eo1xD/l/plAiBT
yjM9aAVbCiiywtqtS8jbwNW+zHiFiYdXY2e1MZ+B2Q==
-----END CERTIFICATE-----
-----END CERTIFICATE-----
)";
const char* const keyPem = R"(-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgq9Vm7YbtFkojNs8Z
yIxI7pLd4YJ34tm0EVHrT6nurxKhRANCAASOTwRDTNFIdKkwxa7hNMd2KRTA/kls
zY4FsbALh47XqguyRYlrMlPstUHgdTK0aFLzFPSUFjhhhYc53j1vIn/Q
-----END PRIVATE KEY-----
)";

QByteArray statusLine(int status)
{
    switch (status) {
    case 200: return "200 OK";
    case 201: return "201 Created";
    case 204: return "204 No Content";
    case 207: return "207 Multi-Status";
    case 304: return "304 Not Modified";
    case 400: return "400 Bad Request";
    case 301: return "301 Moved Permanently";
    case 401: return "401 Unauthorized";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 405: return "405 Method Not Allowed";
    case 409: return "409 Conflict";
    case 412: return "412 Precondition Failed";
    case 423: return "423 Locked";
    default: return QByteArray::number(status) + " Status";
    }
}

QByteArray entryXml(const QString& href, QByteArray tag, qint64 size, bool isFolder)
{
    QByteArray xml = "<d:response><d:href>" + QUrl::toPercentEncoding(href, "/") + "</d:href><d:propstat><d:prop>";
    if (isFolder) {
        xml += "<d:resourcetype><d:collection/></d:resourcetype>";
    } else {
        xml += "<d:resourcetype/>" + (tag.isEmpty() ? QByteArray() : "<d:getetag>\"" + tag + "\"</d:getetag>") +
               "<d:getcontentlength>" +
               QByteArray::number(size) + "</d:getcontentlength>";
    }
    xml += "</d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>";
    return xml;
}

} // namespace

WebDavTestServer::WebDavTestServer(bool https, QObject* parent) : QObject(parent), secure(https)
{
    folders = {QStringLiteral("/"), QStringLiteral("/dav/"), folderPath};
    if (secure) {
        auto* tls = new QSslServer(this);
        QSslConfiguration configuration = QSslConfiguration::defaultConfiguration();
        configuration.setLocalCertificate(QSslCertificate(QByteArray(certificatePem)));
        configuration.setPrivateKey(QSslKey(QByteArray(keyPem), QSsl::Ec));
        configuration.setPeerVerifyMode(QSslSocket::VerifyNone);
        tls->setSslConfiguration(configuration);
        server = tls;
    } else {
        server = new QTcpServer(this);
    }
    connect(server, &QTcpServer::pendingConnectionAvailable, this, [this]() {
        while (QTcpSocket* socket = server->nextPendingConnection()) {
            connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { serve(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
                pending.remove(socket);
                socket->deleteLater();
            });
        }
    });
    server->listen(QHostAddress::LocalHost, 0);
}

WebDavTestServer::~WebDavTestServer() = default;

QUrl WebDavTestServer::folder() const
{
    return QUrl(QStringLiteral("%1://127.0.0.1:%2%3")
                    .arg(secure ? QStringLiteral("https") : QStringLiteral("http"))
                    .arg(server->serverPort())
                    .arg(folderPath));
}

QByteArray WebDavTestServer::certificateFingerprint()
{
    return QSslCertificate(QByteArray(certificatePem)).digest(QCryptographicHash::Sha256).toHex();
}

void WebDavTestServer::setFile(const QString& name, const QByteArray& data)
{
    File& file = files[name];
    file.data = data;
    file.tag = newTag();
}

QByteArray WebDavTestServer::newTag()
{
    return "tag-" + QByteArray::number(++tagCounter);
}

QString WebDavTestServer::nameOf(const QByteArray& target, bool* inFolder, bool* isFolder) const
{
    const QString path = QUrl(QString::fromLatin1(target)).path(QUrl::FullyDecoded);
    *isFolder = folders.contains(path) || folders.contains(path + QLatin1Char('/'));
    *inFolder = path.startsWith(folderPath) && !path.mid(folderPath.size()).contains(QLatin1Char('/'));
    return *inFolder ? path.mid(folderPath.size()) : QString();
}

void WebDavTestServer::serve(QTcpSocket* socket)
{
    QByteArray& buffer = pending[socket];
    buffer += socket->readAll();
    for (;;) {
        const qsizetype end = buffer.indexOf("\r\n\r\n");
        if (end < 0) {
            return;
        }
        const QList<QByteArray> lines = buffer.left(end).split('\n');
        const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
        QHash<QByteArray, QByteArray> headers;
        for (qsizetype index = 1; index < lines.size(); ++index) {
            const qsizetype colon = lines[index].indexOf(':');
            if (colon > 0) {
                headers.insert(lines[index].left(colon).trimmed().toLower(), lines[index].mid(colon + 1).trimmed());
            }
        }
        const qsizetype length = headers.value("content-length", "0").toLongLong();
        if (buffer.size() < end + 4 + length) {
            return;
        }
        const QByteArray body = buffer.mid(end + 4, length);
        buffer.remove(0, end + 4 + length);
        answer(socket, first.value(0), first.value(1), headers, body);
    }
}

void WebDavTestServer::answer(QTcpSocket* socket, const QByteArray& method, const QByteArray& target,
                              const QHash<QByteArray, QByteArray>& headers, const QByteArray& body)
{
    ++requests;
    QByteArray replyBody;
    QList<QByteArray> replyHeaders;
    int status = 200;
    bool inFolder = false;
    bool isFolder = false;
    const QString name = nameOf(target, &inFolder, &isFolder);
    if (beforeAnswer) {
        beforeAnswer(method, name);
    }
    const QByteArray expected = "Basic " + (user + QLatin1Char(':') + password).toUtf8().toBase64();
    if (headers.value("authorization") != expected) {
        status = 401;
        replyHeaders << "WWW-Authenticate: Basic realm=\"test\"";
    } else if (refuseWith != 0) {
        status = refuseWith;
        if (status >= 300 && status < 400) {
            replyHeaders << "Location: http://127.0.0.1:1/elsewhere/";
        }
    } else if (method == "PROPFIND") {
        const QByteArray depth = headers.value("depth", "1");
        QByteArray xml = "<?xml version=\"1.0\" encoding=\"utf-8\"?><d:multistatus xmlns:d=\"DAV:\">";
        if (isFolder) {
            xml += entryXml(folderPath, {}, 0, true);
            if (depth != "0") {
                for (auto file = files.cbegin(); file != files.cend(); ++file) {
                    if (!file->data.isEmpty() || !file->lockToken.isEmpty()) {
                        xml += entryXml(folderPath + file.key(), file->tag, file->data.size(), false);
                    }
                }
            }
            status = 207;
        } else if (inFolder && files.contains(name)) {
            const File& file = files[name];
            xml += entryXml(folderPath + name, file.tag, file.data.size(), false);
            status = 207;
        } else {
            status = 404;
        }
        if (status == 207) {
            replyBody = xml + "</d:multistatus>";
            replyHeaders << "Content-Type: application/xml; charset=utf-8";
        }
    } else if (!inFolder) {
        status = 409; // only files in the one folder
    } else if (method == "GET") {
        if (!files.contains(name) || files[name].data.isEmpty()) {
            status = 404;
        } else if (headers.value("if-none-match") == "\"" + files[name].tag + "\"") {
            status = 304;
            replyHeaders << "ETag: \"" + files[name].tag + "\"";
        } else {
            replyBody = files[name].data;
            replyHeaders << "ETag: \"" + files[name].tag + "\"";
        }
    } else if (method == "PUT") {
        if (files.contains(name) && !files[name].lockToken.isEmpty() &&
            !headers.value("if").contains(files[name].lockToken)) {
            status = 423;
        } else {
            status = files.contains(name) && !files[name].data.isEmpty() ? 204 : 201;
            setFile(name, body);
            replyHeaders << "ETag: \"" + files[name].tag + "\"";
        }
    } else if (method == "DELETE") {
        status = files.remove(name) ? 204 : 404;
    } else if (method == "MOVE") {
        bool destinationInFolder = false;
        bool destinationIsFolder = false;
        const QString destination = nameOf(headers.value("destination"), &destinationInFolder, &destinationIsFolder);
        if (!files.contains(name) || !destinationInFolder) {
            status = !files.contains(name) ? 404 : 409;
        } else if (files.contains(destination) && !files[destination].lockToken.isEmpty() &&
                   !headers.value("if").contains(files[destination].lockToken)) {
            status = 423;
        } else {
            const bool existed = files.contains(destination) && !files[destination].data.isEmpty();
            const QByteArray token = files.value(destination).lockToken;
            File moved = files.take(name);
            moved.tag = newTag();
            moved.lockToken = token; // the lock stays on its address
            files.insert(destination, moved);
            status = existed ? 204 : 201;
        }
    } else if (method == "LOCK") {
        if (!locksOffered) {
            status = 405;
        } else if (files.contains(name) && !files[name].lockToken.isEmpty()) {
            status = 423;
        } else {
            File& file = files[name]; // a lock on a missing file makes an empty one
            if (file.tag.isEmpty()) {
                file.tag = newTag();
            }
            file.lockToken = "<opaquelocktoken:" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces) + ">";
            replyHeaders << "Lock-Token: " + file.lockToken;
            replyBody = "<?xml version=\"1.0\" encoding=\"utf-8\"?><d:prop xmlns:d=\"DAV:\"/>";
            replyHeaders << "Content-Type: application/xml; charset=utf-8";
        }
    } else if (method == "UNLOCK") {
        if (files.contains(name) && files[name].lockToken == headers.value("lock-token")) {
            files[name].lockToken.clear();
            if (files[name].data.isEmpty()) {
                files.remove(name);
            }
            status = 204;
        } else {
            status = 409;
        }
    } else {
        status = 405;
    }
    if (!tagsOffered) {
        replyHeaders.removeIf([](const QByteArray& header) { return header.startsWith("ETag:"); });
        replyBody.replace(QByteArrayLiteral("<d:getetag>"), QByteArrayLiteral("<d:x>"));
        replyBody.replace(QByteArrayLiteral("</d:getetag>"), QByteArrayLiteral("</d:x>"));
    }
    QByteArray response = "HTTP/1.1 " + statusLine(status) + "\r\n";
    for (const QByteArray& header : std::as_const(replyHeaders)) {
        response += header + "\r\n";
    }
    response += "Content-Length: " + QByteArray::number(replyBody.size()) + "\r\n\r\n" + replyBody;
    socket->write(response);
}
