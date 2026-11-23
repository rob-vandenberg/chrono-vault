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


#include "webdav_client.h"

#include "quick_unlock.h"

#include <QAuthenticator>
#include <QCryptographicHash>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslError>
#include <QXmlStreamReader>

// --- Version ------------------------------------------------------------
// webdav_client.cpp 0.3.62

// --- Version history ----------------------------------------------------
// v0.3.62: Making a folder, for the folder browser (D-057)
// v0.3.61: Every request and its answer in the --debug log, never a password or a vault's bytes (D-057)
// v0.3.60: Initial release (D-057)

namespace chrono_vault {

namespace {

constexpr int transferTimeoutMs = 60000;
constexpr int mostEntries = 10000;
const char* const askedProperty = "chronoVaultAsked";
const char* const untrustedProperty = "chronoVaultUntrusted";
const char* const tooLargeProperty = "chronoVaultTooLarge";

const QByteArray propertiesWanted = QByteArrayLiteral(
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<d:propfind xmlns:d=\"DAV:\"><d:prop><d:getetag/><d:getlastmodified/><d:getcontentlength/><d:resourcetype/></d:prop></d:propfind>");

const QByteArray lockWanted = QByteArrayLiteral(
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<d:lockinfo xmlns:d=\"DAV:\"><d:lockscope><d:exclusive/></d:lockscope>"
    "<d:locktype><d:write/></d:locktype><d:owner>Chrono Vault</d:owner></d:lockinfo>");

// A tag from the ETag, or else from the time of the last change, so a server
// that gives no ETag still shows a change.
QByteArray tagFrom(const QByteArray& etag, const QByteArray& lastModified)
{
    QByteArray tag = etag.trimmed();
    if (tag.startsWith("W/")) {
        tag = tag.mid(2);
    }
    if (tag.size() >= 2 && tag.startsWith('"') && tag.endsWith('"')) {
        tag = tag.mid(1, tag.size() - 2);
    }
    if (!tag.isEmpty()) {
        return tag; // the same from a GET and a PROPFIND, however the server quotes it
    }
    return lastModified.trimmed().isEmpty() ? QByteArray() : QByteArrayLiteral("lm:") + lastModified.trimmed();
}

QString lastSegment(QString path)
{
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path.mid(path.lastIndexOf(QLatin1Char('/')) + 1);
}

QString withoutEndSlash(QString path)
{
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

} // namespace

QUrl folderAddress(const QUrl& given)
{
    QUrl folder = given.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment);
    if (!folder.path().endsWith(QLatin1Char('/'))) {
        folder.setPath(folder.path() + QLatin1Char('/'));
    }
    return folder;
}

std::optional<QList<WebDavEntry>> readFolderList(const QByteArray& xml, const QUrl& folder)
{
    QXmlStreamReader reader(xml);
    QList<WebDavEntry> entries;
    const QString own = withoutEndSlash(folder.path(QUrl::FullyDecoded));
    const QString dav = QStringLiteral("DAV:");
    bool sawMultistatus = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.namespaceUri() != dav) {
            continue;
        }
        if (reader.name() == QLatin1String("multistatus")) {
            sawMultistatus = true;
            continue;
        }
        if (reader.name() != QLatin1String("response")) {
            continue;
        }
        QString href;
        QByteArray etag;
        QByteArray modified;
        qint64 size = -1;
        bool isFolder = false;
        int depth = 1;
        while (!reader.atEnd() && depth > 0) {
            reader.readNext();
            if (reader.isEndElement()) {
                --depth;
                continue;
            }
            if (!reader.isStartElement()) {
                continue;
            }
            ++depth;
            if (reader.namespaceUri() != dav) {
                continue;
            }
            const auto name = reader.name();
            if (name == QLatin1String("href")) {
                href = reader.readElementText();
                --depth;
            } else if (name == QLatin1String("getetag")) {
                etag = reader.readElementText().toUtf8();
                --depth;
            } else if (name == QLatin1String("getlastmodified")) {
                modified = reader.readElementText().toUtf8();
                --depth;
            } else if (name == QLatin1String("getcontentlength")) {
                bool number = false;
                const qint64 length = reader.readElementText().trimmed().toLongLong(&number);
                size = number && length >= 0 ? length : -1;
                --depth;
            } else if (name == QLatin1String("collection")) {
                isFolder = true;
            }
        }
        if (href.isEmpty()) {
            continue;
        }
        const QString path = withoutEndSlash(QUrl(href).path(QUrl::FullyDecoded));
        if (path == own || path.isEmpty()) {
            continue;
        }
        WebDavEntry entry;
        entry.name = lastSegment(path);
        entry.isFolder = isFolder;
        entry.tag = tagFrom(etag, modified);
        entry.size = size;
        if (!entry.name.isEmpty()) {
            entries.append(entry);
        }
        if (entries.size() > mostEntries) {
            return std::nullopt;
        }
    }
    if (reader.hasError() || !sawMultistatus) {
        return std::nullopt;
    }
    return entries;
}

WebDavClient::WebDavClient(WebDavAccount account, QObject* parent) : QObject(parent), place(std::move(account))
{
    place.folder = folderAddress(place.folder);
    connect(&network, &QNetworkAccessManager::authenticationRequired, this,
            [this](QNetworkReply* reply, QAuthenticator* asked) {
                // Given once; asked again means the server refused them, and
                // the request then ends as Unauthorized.
                if (reply->property(askedProperty).toBool()) {
                    return;
                }
                reply->setProperty(askedProperty, true);
                asked->setUser(place.user);
                asked->setPassword(place.password);
            });
    connect(&network, &QNetworkAccessManager::sslErrors, this,
            [this](QNetworkReply* reply, const QList<QSslError>& errors) {
                QSslCertificate certificate = reply->sslConfiguration().peerCertificate();
                if (certificate.isNull() && !errors.isEmpty()) {
                    certificate = errors.constFirst().certificate();
                }
                const QByteArray fingerprint =
                    certificate.isNull() ? QByteArray() : certificate.digest(QCryptographicHash::Sha256).toHex();
                if (!fingerprint.isEmpty() && fingerprint == place.trustedCertificate) {
                    reply->ignoreSslErrors(errors); // the certificate the person trusted (D-057)
                    return;
                }
                reply->setProperty(untrustedProperty, fingerprint);
            });
}

QUrl WebDavClient::addressOf(const QString& name) const
{
    return place.folder.resolved(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(name)), QUrl::StrictMode));
}

QNetworkReply* WebDavClient::send(const QByteArray& verb, const QUrl& address, const QByteArray& body,
                                  const QList<std::pair<QByteArray, QByteArray>>& headers)
{
    QNetworkRequest request(address);
    request.setTransferTimeout(transferTimeoutMs);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    for (const auto& [name, value] : headers) {
        request.setRawHeader(name, value);
    }
    if (!body.isEmpty() && !request.hasRawHeader("Content-Type")) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/xml; charset=utf-8"));
    }
    QNetworkReply* reply = network.sendCustomRequest(request, verb, body);
    fingerLog(QStringLiteral("webdav: %1 %2 (%3 bytes sent)")
                  .arg(QString::fromLatin1(verb), address.toDisplayString(QUrl::RemoveUserInfo))
                  .arg(body.size()));
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > largestAnswerBytes) {
            reply->setProperty(tooLargeProperty, true);
            reply->abort();
        }
    });
    return reply;
}

void WebDavClient::whenDone(QNetworkReply* reply, std::function<void(WebDavReply, QNetworkReply*)> done)
{
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)]() {
        reply->deleteLater();
        WebDavReply answer;
        answer.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        answer.tag = tagFrom(reply->rawHeader("ETag"), reply->rawHeader("Last-Modified"));
        const QByteArray untrusted = reply->property(untrustedProperty).toByteArray();
        if (reply->property(tooLargeProperty).toBool()) {
            answer.error = WebDavError::TooLarge;
        } else if (answer.status == 0) {
            if (reply->property(untrustedProperty).isValid() ||
                reply->error() == QNetworkReply::SslHandshakeFailedError) {
                answer.error = WebDavError::Certificate;
                answer.certificate = untrusted;
            } else if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
                answer.error = WebDavError::Unauthorized;
            } else {
                answer.error = WebDavError::NoConnection;
            }
        } else if (answer.status == 401) {
            // 403 is a refusal (read-only, full), not a wrong password.
            answer.error = WebDavError::Unauthorized;
        } else if (answer.status == 404 || answer.status == 409 || answer.status == 410) {
            answer.error = WebDavError::NotFound;
        } else if (answer.status == 412) {
            answer.error = WebDavError::Changed;
        } else if (answer.status == 423) {
            answer.error = WebDavError::Locked;
        } else if (answer.status == 405 || answer.status == 501) {
            answer.error = WebDavError::NotSupported;
        } else if (answer.status == 413 || answer.status == 507) {
            answer.error = WebDavError::TooLarge;
        } else if (answer.status >= 400 || answer.status < 200 || (answer.status >= 300 && answer.status != 304)) {
            // A redirect Qt did not follow (to a less safe address) is no answer.
            answer.error = WebDavError::Server;
        }
        if (answer.ok()) {
            answer.body = reply->readAll();
            if (answer.body.size() > largestAnswerBytes) {
                answer.body.clear();
                answer.error = WebDavError::TooLarge;
            }
        }
        {
            // The answer, for --debug: never a password or a vault's bytes.
            QString headers;
            for (const auto& [name, value] : reply->rawHeaderPairs()) {
                const QByteArray lower = name.toLower();
                if (lower != "set-cookie" && lower != "authorization" && lower != "www-authenticate") {
                    headers += QStringLiteral(" | ") + QString::fromLatin1(name) + QStringLiteral(": ") +
                               QString::fromLatin1(value);
                }
            }
            QString shown = QStringLiteral("webdav:   -> status %1, error %2, qt error %3, %4 bytes%5")
                                .arg(answer.status)
                                .arg(static_cast<int>(answer.error))
                                .arg(static_cast<int>(reply->error()))
                                .arg(answer.body.size())
                                .arg(headers);
            if (reply->header(QNetworkRequest::ContentTypeHeader).toString().contains(QLatin1String("xml"))) {
                shown += QStringLiteral("\nwebdav:   xml: ") + QString::fromUtf8(answer.body.left(3000));
            }
            fingerLog(shown);
        }
        done(answer, reply);
    });
}

void WebDavClient::list(Listed done)
{
    QNetworkReply* reply = send("PROPFIND", place.folder, propertiesWanted, {{"Depth", "1"}});
    const QUrl folder = place.folder;
    whenDone(reply, [done = std::move(done), folder](WebDavReply answer, QNetworkReply*) {
        if (!answer.ok()) {
            done(answer, {});
            return;
        }
        const auto entries = readFolderList(answer.body, folder);
        answer.body.clear();
        if (!entries) {
            answer.error = WebDavError::Damaged;
            done(answer, {});
            return;
        }
        done(answer, *entries);
    });
}

void WebDavClient::get(const QString& name, const QByteArray& ifNotTag, Done done)
{
    QList<std::pair<QByteArray, QByteArray>> headers;
    if (!ifNotTag.isEmpty() && !ifNotTag.startsWith("lm:")) {
        headers.append({"If-None-Match", '"' + ifNotTag + '"'});
    }
    QNetworkReply* reply = send("GET", addressOf(name), {}, headers);
    whenDone(reply, [done = std::move(done)](WebDavReply answer, QNetworkReply*) { done(answer); });
}

void WebDavClient::put(const QString& name, const QByteArray& data, Done done)
{
    QNetworkReply* reply =
        send("PUT", addressOf(name), data, {{"Content-Type", "application/octet-stream"}});
    whenDone(reply, [done = std::move(done)](WebDavReply answer, QNetworkReply*) { done(answer); });
}

void WebDavClient::move(const QString& from, const QString& to, const QByteArray& lockToken, Done done)
{
    const QUrl target = addressOf(to);
    QList<std::pair<QByteArray, QByteArray>> headers{{"Destination", target.toEncoded()}, {"Overwrite", "T"}};
    if (!lockToken.isEmpty()) {
        headers.append({"If", "<" + target.toEncoded() + "> (" + lockToken + ")"});
    }
    QNetworkReply* reply = send("MOVE", addressOf(from), {}, headers);
    whenDone(reply, [done = std::move(done)](WebDavReply answer, QNetworkReply*) { done(answer); });
}

void WebDavClient::remove(const QString& name, Done done)
{
    QNetworkReply* reply = send("DELETE", addressOf(name), {}, {});
    whenDone(reply, [done = std::move(done)](WebDavReply answer, QNetworkReply*) { done(answer); });
}

void WebDavClient::makeFolder(const QString& name, Done done)
{
    QUrl address = addressOf(name);
    address.setPath(address.path() + QLatin1Char('/'));
    QNetworkReply* reply = send("MKCOL", address, {}, {});
    whenDone(reply, [done = std::move(done)](WebDavReply answer, QNetworkReply*) { done(answer); });
}

void WebDavClient::tagOf(const QString& name, Done done)
{
    const QUrl address = addressOf(name);
    QNetworkReply* reply = send("PROPFIND", address, propertiesWanted, {{"Depth", "0"}});
    whenDone(reply, [done = std::move(done), address](WebDavReply answer, QNetworkReply*) {
        if (!answer.ok()) {
            done(answer);
            return;
        }
        // The file's own entry is the one the list leaves out: read it as a
        // list of its parent folder.
        const auto entries = readFolderList(answer.body, address.resolved(QUrl(QStringLiteral("./"))));
        answer.body.clear();
        answer.tag.clear();
        if (!entries) {
            answer.error = WebDavError::Damaged;
        } else {
            const QString name = lastSegment(address.path(QUrl::FullyDecoded));
            for (const WebDavEntry& entry : *entries) {
                if (entry.name == name) {
                    answer.tag = entry.tag;
                    answer.size = entry.size;
                }
            }
        }
        done(answer);
    });
}

void WebDavClient::lock(const QString& name, Done done)
{
    QNetworkReply* reply = send("LOCK", addressOf(name), lockWanted, {{"Depth", "0"}, {"Timeout", "Second-120"}});
    whenDone(reply, [done = std::move(done)](WebDavReply answer, QNetworkReply* finished) {
        answer.body.clear();
        if (answer.ok()) {
            answer.body = finished->rawHeader("Lock-Token").trimmed();
            if (answer.body.isEmpty()) {
                answer.error = WebDavError::NotSupported; // a lock without a token cannot be used
            }
        }
        done(answer);
    });
}

void WebDavClient::unlock(const QString& name, const QByteArray& lockToken, Done done)
{
    QNetworkReply* reply = send("UNLOCK", addressOf(name), {}, {{"Lock-Token", lockToken}});
    whenDone(reply, [done = std::move(done)](WebDavReply answer, QNetworkReply*) { done(answer); });
}

} // namespace chrono_vault
