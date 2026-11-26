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


#include "google_drive_storage.h"

#include "google_drive_client.h"
#include "quick_unlock.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QRandomGenerator>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>

// --- Version ------------------------------------------------------------
// google_drive_storage.cpp 0.8.115

// --- Version history ----------------------------------------------------
// v0.8.115: Initial release: Google Drive as a cloud storage, with the permission drive.file only (D-080)

namespace chrono_vault {

namespace {

constexpr int transferTimeoutMs = 60000;
constexpr int mostEntries = 10000;
constexpr qint64 tokenMarginMs = 60 * 1000;
const QString folderType = QStringLiteral("application/vnd.google-apps.folder");
const QString topFolderName = QStringLiteral("Chrono Vault");
const QByteArray driveFileScope = QByteArrayLiteral("https://www.googleapis.com/auth/drive.file");

struct GoogleSettings
{
    GoogleEndpoints endpoints{QUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth")),
                              QUrl(QStringLiteral("https://oauth2.googleapis.com/token")),
                              QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/")),
                              QUrl(QStringLiteral("https://www.googleapis.com/upload/drive/v3/"))};
    QString clientId = QStringLiteral(CHRONO_VAULT_GOOGLE_CLIENT_ID);
    QString clientSecret = QStringLiteral(CHRONO_VAULT_GOOGLE_CLIENT_SECRET);
};

GoogleSettings& settings()
{
    static GoogleSettings kept;
    return kept;
}

// A name inside a Drive search, with its quotes and backslashes escaped.
QString quoted(QString text)
{
    text.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    text.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    return QLatin1Char('\'') + text + QLatin1Char('\'');
}

QJsonObject jsonOf(const QByteArray& body)
{
    return QJsonDocument::fromJson(body).object();
}

bool passing(const QString& name)
{
    return name.endsWith(QLatin1String(".uploading"));
}

} // namespace

bool googleDriveAvailable()
{
    return !settings().clientId.isEmpty();
}

GoogleEndpoints googleEndpoints()
{
    return settings().endpoints;
}

void setGoogleForTests(const GoogleEndpoints& endpoints, const QString& clientId, const QString& clientSecret)
{
    settings().endpoints = endpoints;
    settings().clientId = clientId;
    settings().clientSecret = clientSecret;
}

GoogleDriveStorage::GoogleDriveStorage(GoogleDriveAccount account, QObject* parent)
    : CloudStorage(parent), where(std::move(account))
{
}

QNetworkRequest GoogleDriveStorage::request(const QUrl& address, const QString& token) const
{
    QNetworkRequest asked(address);
    asked.setTransferTimeout(transferTimeoutMs);
    asked.setRawHeader("Authorization", "Bearer " + token.toUtf8());
    asked.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return asked;
}

QUrl GoogleDriveStorage::apiUrl(const QString& path, const QList<std::pair<QString, QString>>& query) const
{
    QUrl address = settings().endpoints.api.resolved(QUrl(path));
    QUrlQuery items;
    for (const auto& [key, value] : query) {
        items.addQueryItem(key, QString::fromLatin1(QUrl::toPercentEncoding(value)));
    }
    address.setQuery(items);
    return address;
}

QUrl GoogleDriveStorage::uploadUrl(const QString& path, const QList<std::pair<QString, QString>>& query) const
{
    QUrl address = settings().endpoints.upload.resolved(QUrl(path));
    QUrlQuery items;
    for (const auto& [key, value] : query) {
        items.addQueryItem(key, QString::fromLatin1(QUrl::toPercentEncoding(value)));
    }
    address.setQuery(items);
    return address;
}

CloudReply GoogleDriveStorage::replyOf(QNetworkReply* reply)
{
    CloudReply answer;
    answer.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (answer.status == 0) {
        answer.error = CloudError::NoConnection; // no network, or Google not answering
        return answer;
    }
    answer.body = reply->read(largestAnswerBytes + 1);
    if (answer.body.size() > largestAnswerBytes) {
        answer.body.clear();
        answer.error = CloudError::TooLarge;
        return answer;
    }
    if (answer.status >= 200 && answer.status < 300) {
        return answer;
    }
    switch (answer.status) {
    case 401:
        answer.error = CloudError::Unauthorized;
        break;
    case 404:
        answer.error = CloudError::NotFound;
        break;
    case 412:
        answer.error = CloudError::Changed;
        break;
    case 413:
        answer.error = CloudError::TooLarge;
        break;
    case 403:
        // A full Drive answers 403 with its reason (storageQuotaExceeded).
        answer.error = answer.body.contains("storageQuotaExceeded") || answer.body.contains("quotaExceeded")
                           ? CloudError::TooLarge
                           : CloudError::Server;
        break;
    default:
        answer.error = CloudError::Server;
        break;
    }
    return answer;
}

void GoogleDriveStorage::withToken(std::function<void(const CloudReply&)> then)
{
    if (!accessToken.isEmpty() && QDateTime::currentMSecsSinceEpoch() < tokenUntilMs - tokenMarginMs) {
        then(CloudReply{});
        return;
    }
    if (where.refreshToken.isEmpty()) {
        then(CloudReply{CloudError::Unauthorized, 401, {}, {}, {}, -1});
        return;
    }
    QNetworkRequest asked(settings().endpoints.token);
    asked.setTransferTimeout(transferTimeoutMs);
    asked.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), QString::fromLatin1(QUrl::toPercentEncoding(settings().clientId)));
    form.addQueryItem(QStringLiteral("client_secret"),
                      QString::fromLatin1(QUrl::toPercentEncoding(settings().clientSecret)));
    form.addQueryItem(QStringLiteral("refresh_token"),
                      QString::fromLatin1(QUrl::toPercentEncoding(where.refreshToken)));
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    QNetworkReply* reply = network.post(asked, form.toString(QUrl::FullyEncoded).toLatin1());
    QPointer<GoogleDriveStorage> self(this);
    connect(reply, &QNetworkReply::finished, this, [self, reply, then = std::move(then)]() {
        reply->deleteLater();
        if (!self) {
            return;
        }
        CloudReply answer = replyOf(reply);
        if (answer.ok()) {
            const QJsonObject token = jsonOf(answer.body);
            self->accessToken = token.value(QStringLiteral("access_token")).toString();
            const qint64 seconds = token.value(QStringLiteral("expires_in")).toInteger(3600);
            self->tokenUntilMs = QDateTime::currentMSecsSinceEpoch() + std::clamp<qint64>(seconds, 60, 86400) * 1000;
            if (self->accessToken.isEmpty()) {
                answer.error = CloudError::Damaged;
            }
        } else if (answer.status == 400 || answer.status == 401) {
            // The person took Chrono Vault's access away, or the sign-in
            // ran out: signed in again (D-080).
            answer.error = CloudError::Unauthorized;
        }
        answer.body.clear();
        fingerLog(QStringLiteral("google drive: token: status %1, error %2")
                      .arg(answer.status)
                      .arg(static_cast<int>(answer.error)));
        then(answer);
    });
}

void GoogleDriveStorage::send(std::function<QNetworkReply*(const QString& token)> make, Handled handled, bool again)
{
    QPointer<GoogleDriveStorage> self(this);
    withToken([self, make = std::move(make), handled = std::move(handled), again](const CloudReply& token) mutable {
        if (!self) {
            return;
        }
        if (!token.ok()) {
            handled(nullptr, token);
            return;
        }
        QNetworkReply* reply = make(self->accessToken);
        QObject::connect(reply, &QNetworkReply::finished, self,
                         [self, reply, make = std::move(make), handled = std::move(handled), again]() mutable {
                             reply->deleteLater();
                             if (!self) {
                                 return;
                             }
                             const CloudReply answer = replyOf(reply);
                             fingerLog(QStringLiteral("google drive: %1: status %2")
                                           .arg(reply->url().path())
                                           .arg(answer.status));
                             if (answer.error == CloudError::Unauthorized && again) {
                                 self->accessToken.clear(); // ran out early: a fresh one, once
                                 self->send(std::move(make), std::move(handled), false);
                                 return;
                             }
                             handled(reply, answer);
                         });
    });
}

void GoogleDriveStorage::find(const QString& name, bool folder,
                              std::function<void(const CloudReply&, const Found&)> then)
{
    QString search = QStringLiteral("name = %1 and %2 in parents and trashed = false")
                         .arg(quoted(name), quoted(where.folderId.isEmpty() ? QStringLiteral("root") : where.folderId));
    search += folder ? QStringLiteral(" and mimeType = '%1'").arg(folderType)
                     : QStringLiteral(" and mimeType != '%1'").arg(folderType);
    const QUrl address = apiUrl(QStringLiteral("files"),
                                {{QStringLiteral("q"), search},
                                 {QStringLiteral("orderBy"), QStringLiteral("createdTime")},
                                 {QStringLiteral("pageSize"), QStringLiteral("10")},
                                 {QStringLiteral("fields"), QStringLiteral("files(id,headRevisionId,md5Checksum,size)")}});
    send([this, address](const QString& token) { return network.get(request(address, token)); },
         [then = std::move(then)](QNetworkReply*, CloudReply answer) {
             Found found;
             if (answer.ok()) {
                 const QJsonArray files = jsonOf(answer.body).value(QStringLiteral("files")).toArray();
                 if (files.isEmpty()) {
                     answer.error = CloudError::NotFound;
                     answer.status = 404;
                 } else {
                     // The oldest of the same name, should there be two.
                     const QJsonObject file = files.first().toObject();
                     found.id = file.value(QStringLiteral("id")).toString();
                     found.revision = file.value(QStringLiteral("headRevisionId")).toString().toLatin1();
                     found.md5 = file.value(QStringLiteral("md5Checksum")).toString().toLatin1();
                     found.size = file.value(QStringLiteral("size")).toString().toLongLong();
                     if (found.id.isEmpty()) {
                         answer.error = CloudError::Damaged;
                     }
                 }
             }
             answer.body.clear();
             then(answer, found);
         });
}

void GoogleDriveStorage::signIn(Done done)
{
    QPointer<GoogleDriveStorage> self(this);
    withToken([self, done = std::move(done)](const CloudReply& token) mutable {
        if (!self) {
            return;
        }
        if (!token.ok() || !self->where.folderId.isEmpty()) {
            done(token);
            return;
        }
        // The "Chrono Vault" folder at the top of My Drive (the owner, Q14):
        // found when Chrono Vault made it before, else made now.
        self->find(topFolderName, true, [self, done = std::move(done)](const CloudReply& answer, const Found& found) mutable {
            if (!self) {
                return;
            }
            if (answer.ok()) {
                self->where.folderId = found.id;
                self->where.folderPath = topFolderName;
                done(CloudReply{});
                return;
            }
            if (answer.error != CloudError::NotFound) {
                done(answer);
                return;
            }
            const QJsonObject metadata{{QStringLiteral("name"), topFolderName},
                                       {QStringLiteral("mimeType"), folderType},
                                       {QStringLiteral("parents"), QJsonArray{QStringLiteral("root")}}};
            const QUrl address = self->apiUrl(QStringLiteral("files"), {{QStringLiteral("fields"), QStringLiteral("id")}});
            self->send(
                [self, address, metadata](const QString& token) {
                    QNetworkRequest asked = self->request(address, token);
                    asked.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
                    return self->network.post(asked, QJsonDocument(metadata).toJson(QJsonDocument::Compact));
                },
                [self, done = std::move(done)](QNetworkReply*, CloudReply made) {
                    if (made.ok()) {
                        self->where.folderId = jsonOf(made.body).value(QStringLiteral("id")).toString();
                        self->where.folderPath = topFolderName;
                        if (self->where.folderId.isEmpty()) {
                            made.error = CloudError::Damaged;
                        }
                    }
                    made.body.clear();
                    done(made);
                });
        });
    });
}

void GoogleDriveStorage::list(Listed done)
{
    auto entries = std::make_shared<QList<CloudEntry>>();
    auto folders = std::make_shared<QHash<QString, QString>>();
    auto page = std::make_shared<std::function<void(const QString&)>>();
    QPointer<GoogleDriveStorage> self(this);
    *page = [self, entries, folders, page, done = std::move(done)](const QString& pageToken) {
        if (!self) {
            return;
        }
        QList<std::pair<QString, QString>> query{
            {QStringLiteral("q"), QStringLiteral("%1 in parents and trashed = false").arg(quoted(self->where.folderId))},
            {QStringLiteral("pageSize"), QStringLiteral("1000")},
            {QStringLiteral("fields"),
             QStringLiteral("nextPageToken,files(id,name,mimeType,size,modifiedTime,headRevisionId)")}};
        if (!pageToken.isEmpty()) {
            query.append({QStringLiteral("pageToken"), pageToken});
        }
        const QUrl address = self->apiUrl(QStringLiteral("files"), query);
        self->send([self, address](const QString& token) { return self->network.get(self->request(address, token)); },
                   [self, entries, folders, page, done](QNetworkReply*, CloudReply answer) {
                       if (!self) {
                           return;
                       }
                       if (!answer.ok()) {
                           answer.body.clear();
                           done(answer, {});
                           *page = nullptr; // ends the circle of the page function
                           return;
                       }
                       const QJsonObject listed = jsonOf(answer.body);
                       for (const QJsonValue& value : listed.value(QStringLiteral("files")).toArray()) {
                           const QJsonObject file = value.toObject();
                           CloudEntry entry;
                           entry.name = file.value(QStringLiteral("name")).toString();
                           entry.isFolder = file.value(QStringLiteral("mimeType")).toString() == folderType;
                           entry.tag = file.value(QStringLiteral("headRevisionId")).toString().toLatin1();
                           entry.size = entry.isFolder ? -1 : file.value(QStringLiteral("size")).toString().toLongLong();
                           entry.modified = QDateTime::fromString(file.value(QStringLiteral("modifiedTime")).toString(),
                                                                  Qt::ISODateWithMs);
                           if (entry.name.isEmpty() || entries->size() >= mostEntries) {
                               continue; // never more than a folder can sensibly hold (OR-4)
                           }
                           if (entry.isFolder) {
                               folders->insert(entry.name, file.value(QStringLiteral("id")).toString());
                           }
                           entries->append(entry);
                       }
                       const QString next = listed.value(QStringLiteral("nextPageToken")).toString();
                       if (!next.isEmpty() && entries->size() < mostEntries) {
                           (*page)(next);
                           return;
                       }
                       self->subfolders = *folders;
                       done(CloudReply{}, *entries);
                       *page = nullptr;
                   });
    };
    (*page)(QString());
}

bool GoogleDriveStorage::enterFolder(const QString& name)
{
    if (name == QLatin1String("..")) {
        if (parents.isEmpty()) {
            return false;
        }
        const auto [id, path] = parents.takeLast();
        where.folderId = id;
        where.folderPath = path;
        return true;
    }
    if (!subfolders.contains(name)) {
        return false;
    }
    parents.append({where.folderId, where.folderPath});
    where.folderId = subfolders.value(name);
    where.folderPath += QLatin1Char('/') + name;
    return true;
}

void GoogleDriveStorage::get(const QString& name, const QByteArray& ifNotTag, Done done)
{
    download(name, ifNotTag, 0, std::move(done));
}

void GoogleDriveStorage::download(const QString& name, const QByteArray& ifNotTag, int tries, Done done)
{
    QPointer<GoogleDriveStorage> self(this);
    find(name, false, [self, name, ifNotTag, tries, done = std::move(done)](const CloudReply& answer, const Found& found) mutable {
        if (!self) {
            return;
        }
        if (!answer.ok()) {
            done(answer);
            return;
        }
        if (!ifNotTag.isEmpty() && ifNotTag == found.revision) {
            done(CloudReply{CloudError::None, 304, {}, found.revision, {}, found.size});
            return;
        }
        if (found.size > largestAnswerBytes) {
            done(CloudReply{CloudError::TooLarge, 200, {}, {}, {}, found.size}); // never read (OR-4)
            return;
        }
        const QUrl address = self->apiUrl(QStringLiteral("files/") + found.id, {{QStringLiteral("alt"), QStringLiteral("media")}});
        self->send([self, address](const QString& token) { return self->network.get(self->request(address, token)); },
                   [self, name, ifNotTag, tries, found, done = std::move(done)](QNetworkReply*, CloudReply read) mutable {
                       if (!self) {
                           return;
                       }
                       if (!read.ok()) {
                           read.body.clear();
                           done(read);
                           return;
                       }
                       // The bytes belong to the version found only when their
                       // checksum is that version's; else it changed between.
                       const QByteArray md5 = QCryptographicHash::hash(read.body, QCryptographicHash::Md5).toHex();
                       if (!found.md5.isEmpty() && md5 != found.md5.toLower()) {
                           if (tries < 2) {
                               self->download(name, ifNotTag, tries + 1, std::move(done));
                           } else {
                               done(CloudReply{CloudError::Changed, 409, {}, {}, {}, -1});
                           }
                           return;
                       }
                       read.tag = found.revision;
                       read.size = read.body.size();
                       done(read);
                   });
    });
}

void GoogleDriveStorage::upload(const QString& id, const QString& name, const QByteArray& data, bool keep,
                                std::function<void(const CloudReply&, const QByteArray&)> done)
{
    const QString keepWord = keep ? QStringLiteral("true") : QStringLiteral("false");
    QPointer<GoogleDriveStorage> self(this);
    const auto answered = [done = std::move(done)](QNetworkReply*, CloudReply answer) {
        QByteArray revision;
        if (answer.ok()) {
            revision = jsonOf(answer.body).value(QStringLiteral("headRevisionId")).toString().toLatin1();
        }
        answer.body.clear();
        answer.tag = revision;
        done(answer, revision);
    };
    if (!id.isEmpty()) {
        // A new version of the same file, its old versions kept by Drive.
        const QUrl address = uploadUrl(QStringLiteral("files/") + id,
                                       {{QStringLiteral("uploadType"), QStringLiteral("media")},
                                        {QStringLiteral("keepRevisionForever"), keepWord},
                                        {QStringLiteral("fields"), QStringLiteral("id,headRevisionId")}});
        send(
            [self, address, data](const QString& token) {
                QNetworkRequest asked = self->request(address, token);
                asked.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));
                return self->network.sendCustomRequest(asked, "PATCH", data);
            },
            answered);
        return;
    }
    // A new file in the folder: its name and folder, and its bytes, in one request.
    const QByteArray boundary = "chrono-vault-" + QByteArray::number(QRandomGenerator::system()->generate64(), 16);
    const QJsonObject metadata{{QStringLiteral("name"), name},
                               {QStringLiteral("parents"), QJsonArray{where.folderId}}};
    QByteArray body;
    body += "--" + boundary + "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n";
    body += QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    body += "\r\n--" + boundary + "\r\nContent-Type: application/octet-stream\r\n\r\n";
    body += data;
    body += "\r\n--" + boundary + "--\r\n";
    const QUrl address = uploadUrl(QStringLiteral("files"), {{QStringLiteral("uploadType"), QStringLiteral("multipart")},
                                                             {QStringLiteral("keepRevisionForever"), keepWord},
                                                             {QStringLiteral("fields"), QStringLiteral("id,headRevisionId")}});
    send(
        [self, address, body, boundary](const QString& token) {
            QNetworkRequest asked = self->request(address, token);
            asked.setHeader(QNetworkRequest::ContentTypeHeader,
                            QString::fromLatin1("multipart/related; boundary=" + boundary));
            return self->network.post(asked, body);
        },
        answered);
}

void GoogleDriveStorage::put(const QString& name, const QByteArray& data, Done done)
{
    sent.insert(name, data);
    QPointer<GoogleDriveStorage> self(this);
    find(name, false, [self, name, data, done = std::move(done)](const CloudReply& answer, const Found& found) mutable {
        if (!self) {
            return;
        }
        if (!answer.ok() && answer.error != CloudError::NotFound) {
            done(answer);
            return;
        }
        self->upload(answer.ok() ? found.id : QString(), name, data, !passing(name),
                     [done = std::move(done)](const CloudReply& put, const QByteArray&) { done(put); });
    });
}

void GoogleDriveStorage::deleteFile(const QString& id, Done done)
{
    const QUrl address = apiUrl(QStringLiteral("files/") + id);
    QPointer<GoogleDriveStorage> self(this);
    send([self, address](const QString& token) { return self->network.deleteResource(self->request(address, token)); },
         [done = std::move(done)](QNetworkReply*, CloudReply answer) {
             answer.body.clear();
             done(answer);
         });
}

void GoogleDriveStorage::remove(const QString& name, Done done)
{
    if (passing(name)) {
        sent.remove(name);
    }
    QPointer<GoogleDriveStorage> self(this);
    find(name, false, [self, name, done = std::move(done)](const CloudReply& answer, const Found& found) mutable {
        if (!self) {
            return;
        }
        if (!answer.ok()) {
            done(answer);
            return;
        }
        if (passing(name)) {
            self->deleteFile(found.id, std::move(done)); // an upload of our own, half done
            return;
        }
        // A vault goes to Drive's trash, where the person can still get it back.
        const QUrl address = self->apiUrl(QStringLiteral("files/") + found.id);
        self->send(
            [self, address](const QString& token) {
                QNetworkRequest asked = self->request(address, token);
                asked.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
                return self->network.sendCustomRequest(asked, "PATCH", QByteArrayLiteral("{\"trashed\":true}"));
            },
            [done = std::move(done)](QNetworkReply*, CloudReply trashed) {
                trashed.body.clear();
                done(trashed);
            });
    });
}

void GoogleDriveStorage::makeFolder(const QString& name, Done done)
{
    QPointer<GoogleDriveStorage> self(this);
    find(name, true, [self, name, done = std::move(done)](const CloudReply& answer, const Found&) mutable {
        if (!self) {
            return;
        }
        if (answer.ok()) {
            done(CloudReply{CloudError::Server, 405, {}, {}, {}, -1}); // already there
            return;
        }
        if (answer.error != CloudError::NotFound) {
            done(answer);
            return;
        }
        const QJsonObject metadata{{QStringLiteral("name"), name},
                                   {QStringLiteral("mimeType"), folderType},
                                   {QStringLiteral("parents"), QJsonArray{self->where.folderId}}};
        const QUrl address = self->apiUrl(QStringLiteral("files"), {{QStringLiteral("fields"), QStringLiteral("id")}});
        self->send(
            [self, address, metadata](const QString& token) {
                QNetworkRequest asked = self->request(address, token);
                asked.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
                return self->network.post(asked, QJsonDocument(metadata).toJson(QJsonDocument::Compact));
            },
            [self, name, done = std::move(done)](QNetworkReply*, CloudReply made) {
                if (self && made.ok()) {
                    self->subfolders.insert(name, jsonOf(made.body).value(QStringLiteral("id")).toString());
                }
                made.body.clear();
                done(made);
            });
    });
}

void GoogleDriveStorage::tagOf(const QString& name, Done done)
{
    QPointer<GoogleDriveStorage> self(this);
    find(name, false, [self, name, done = std::move(done)](CloudReply answer, const Found& found) {
        if (!self) {
            return;
        }
        if (answer.ok()) {
            answer.tag = found.revision;
            answer.size = found.size;
            self->checked.insert(name, found.revision); // what the write after it must follow
        } else if (answer.error == CloudError::NotFound) {
            self->checked.remove(name);
        }
        done(answer);
    });
}

void GoogleDriveStorage::move(const QString& from, const QString& to, const QByteArray& lockToken, Done done,
                              bool overwrite)
{
    Q_UNUSED(lockToken) // Drive has no locks
    QPointer<GoogleDriveStorage> self(this);
    find(from, false, [self, from, to, overwrite, done = std::move(done)](const CloudReply& source, const Found& fromFile) mutable {
        if (!self) {
            return;
        }
        if (!source.ok()) {
            done(source);
            return;
        }
        self->find(to, false, [self, from, to, overwrite, fromFile, done = std::move(done)](const CloudReply& target,
                                                                                            const Found& toFile) mutable {
            if (!self) {
                return;
            }
            if (target.error == CloudError::NotFound) {
                // Nothing there yet: the upload is given the vault's name.
                const QUrl address = self->apiUrl(QStringLiteral("files/") + fromFile.id,
                                                  {{QStringLiteral("fields"), QStringLiteral("id,headRevisionId")}});
                const QJsonObject renamed{{QStringLiteral("name"), to}};
                self->send(
                    [self, address, renamed](const QString& token) {
                        QNetworkRequest asked = self->request(address, token);
                        asked.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
                        return self->network.sendCustomRequest(asked, "PATCH",
                                                               QJsonDocument(renamed).toJson(QJsonDocument::Compact));
                    },
                    [self, from, done = std::move(done)](QNetworkReply*, CloudReply moved) {
                        if (self) {
                            self->sent.remove(from);
                        }
                        moved.body.clear();
                        done(moved);
                    });
                return;
            }
            if (!target.ok()) {
                done(target);
                return;
            }
            if (!overwrite) {
                done(CloudReply{CloudError::Changed, 412, {}, {}, {}, -1}); // never over a vault already there
                return;
            }
            const QByteArray expected = self->checked.value(to);
            if (self->sent.contains(from)) {
                self->replaceChecked(toFile, fromFile.id, self->sent.value(from), expected, std::move(done));
                self->sent.remove(from);
                return;
            }
            const QUrl address = self->apiUrl(QStringLiteral("files/") + fromFile.id, {{QStringLiteral("alt"), QStringLiteral("media")}});
            self->send([self, address](const QString& token) { return self->network.get(self->request(address, token)); },
                       [self, toFile, fromFile, expected, done = std::move(done)](QNetworkReply*, CloudReply read) mutable {
                           if (!self) {
                               return;
                           }
                           if (!read.ok()) {
                               read.body.clear();
                               done(read);
                               return;
                           }
                           self->replaceChecked(toFile, fromFile.id, read.body, expected, std::move(done));
                       });
        });
    });
}

void GoogleDriveStorage::replaceChecked(const Found& target, const QString& fromId, const QByteArray& data,
                                        const QByteArray& expected, Done done)
{
    QPointer<GoogleDriveStorage> self(this);
    upload(target.id, QString(), data, true,
           [self, target, fromId, expected, done = std::move(done)](const CloudReply& written, const QByteArray& head) mutable {
               if (!self) {
                   return;
               }
               if (!written.ok()) {
                   done(written);
                   return;
               }
               self->checkHistory(target, fromId, expected, head, 0, std::move(done));
           });
}

void GoogleDriveStorage::checkHistory(const Found& target, const QString& fromId, const QByteArray& expected,
                                      const QByteArray& head, int tries, Done done)
{
    // Drive's history, right after the write: the version just before ours
    // must be the one the sync checked (Q2: A).
    const QUrl address = apiUrl(QStringLiteral("files/") + target.id + QStringLiteral("/revisions"),
                                {{QStringLiteral("pageSize"), QStringLiteral("1000")},
                                 {QStringLiteral("fields"), QStringLiteral("revisions(id,keepForever,modifiedTime)")}});
    QPointer<GoogleDriveStorage> self(this);
    send([self, address](const QString& token) { return self->network.get(self->request(address, token)); },
         [self, target, fromId, expected, head, tries, done = std::move(done)](QNetworkReply*, CloudReply listed) mutable {
             if (!self) {
                 return;
             }
             const QJsonArray revisions = jsonOf(listed.body).value(QStringLiteral("revisions")).toArray();
             QList<QJsonObject> sorted;
             for (const QJsonValue& value : revisions) {
                 sorted.append(value.toObject());
             }
             std::stable_sort(sorted.begin(), sorted.end(), [](const QJsonObject& one, const QJsonObject& other) {
                 return one.value(QStringLiteral("modifiedTime")).toString() <
                        other.value(QStringLiteral("modifiedTime")).toString();
             });
             qsizetype ours = -1;
             for (qsizetype index = 0; index < sorted.size(); ++index) {
                 if (sorted.at(index).value(QStringLiteral("id")).toString().toLatin1() == head) {
                     ours = index;
                 }
             }
             fingerLog(QStringLiteral("google drive: check after the write: try %1, status %2, ours at %3, expected '%4'")
                           .arg(tries)
                           .arg(listed.status)
                           .arg(ours)
                           .arg(QString::fromLatin1(expected)));
             if (!listed.ok() || ours < 0) {
                 // Not read, or ours not in it yet: read again; never taken
                 // as "nothing came in between" (OR-15.3).
                 if (tries < 2) {
                     // A moment later: Drive's list may lag just behind a write.
                     QTimer::singleShot(1000, self, [self, target, fromId, expected, head, tries,
                                                     done = std::move(done)]() mutable {
                         if (self) {
                             self->checkHistory(target, fromId, expected, head, tries + 1, std::move(done));
                         }
                     });
                 } else {
                     done(listed.ok() ? CloudReply{CloudError::Server, listed.status, {}, {}, {}, -1} : listed);
                 }
                 return;
             }
             const QByteArray before =
                 ours > 0 ? sorted.at(ours - 1).value(QStringLiteral("id")).toString().toLatin1() : QByteArray();
             if (ours > 0 && !expected.isEmpty() && before != expected) {
                 // Another device's version came in between: on top again.
                 self->putBack(target.id, before, fromId, std::move(done));
                 return;
             }
             self->deleteFile(fromId, [](const CloudReply&) {});
             self->prune(target.id, revisions, head);
             done(CloudReply{CloudError::None, 200, {}, head, {}, -1});
         });
}

void GoogleDriveStorage::putBack(const QString& id, const QByteArray& revision, const QString& fromId, Done done)
{
    const QUrl address = apiUrl(QStringLiteral("files/") + id + QStringLiteral("/revisions/") +
                                    QString::fromLatin1(revision),
                                {{QStringLiteral("alt"), QStringLiteral("media")}});
    QPointer<GoogleDriveStorage> self(this);
    send([self, address](const QString& token) { return self->network.get(self->request(address, token)); },
         [self, id, fromId, done = std::move(done)](QNetworkReply*, CloudReply read) mutable {
             if (!self) {
                 return;
             }
             if (!read.ok()) {
                 // Not to be read back (not kept by Chrono Vault): said, never
                 // passed over (OR-15.3); it stays in Drive's version history.
                 done(CloudReply{CloudError::Server, read.status, {}, {}, {}, -1});
                 return;
             }
             self->upload(id, QString(), read.body, true,
                          [self, fromId, done = std::move(done)](const CloudReply& restored, const QByteArray&) mutable {
                              if (!self) {
                                  return;
                              }
                              if (!restored.ok()) {
                                  done(restored);
                                  return;
                              }
                              self->deleteFile(fromId, [](const CloudReply&) {});
                              done(CloudReply{CloudError::Superseded, 409, {}, {}, {}, -1});
                          });
         });
}

void GoogleDriveStorage::prune(const QString& id, const QJsonArray& revisions, const QByteArray& head)
{
    QList<QJsonObject> kept;
    for (const QJsonValue& value : revisions) {
        const QJsonObject revision = value.toObject();
        if (revision.value(QStringLiteral("keepForever")).toBool() &&
            revision.value(QStringLiteral("id")).toString().toLatin1() != head) {
            kept.append(revision);
        }
    }
    std::stable_sort(kept.begin(), kept.end(), [](const QJsonObject& one, const QJsonObject& other) {
        return one.value(QStringLiteral("modifiedTime")).toString() < other.value(QStringLiteral("modifiedTime")).toString();
    });
    // Drive keeps at most 200 per file; Chrono Vault keeps its newest few.
    for (qsizetype index = 0; index + keptVersions < kept.size(); ++index) {
        const QUrl address = apiUrl(QStringLiteral("files/") + id + QStringLiteral("/revisions/") +
                                    kept.at(index).value(QStringLiteral("id")).toString());
        QPointer<GoogleDriveStorage> self(this);
        send([self, address](const QString& token) { return self->network.deleteResource(self->request(address, token)); },
             [](QNetworkReply*, const CloudReply&) {});
    }
}

GoogleSignIn::GoogleSignIn(QObject* parent) : QObject(parent) {}

GoogleSignIn::~GoogleSignIn() = default;

void GoogleSignIn::start()
{
    done = false;
    delete flow;
    delete handler;
    flow = new QOAuth2AuthorizationCodeFlow(this);
    handler = new QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, 0, this);
    if (!handler->isListening()) {
        end(false, QString(), tr("Chrono Vault cannot wait for Google's answer on this computer."));
        return;
    }
    handler->setCallbackText(tr("Chrono Vault is signed in to Google Drive. You can close this page."));
    const GoogleEndpoints endpoints = googleEndpoints();
    flow->setAuthorizationUrl(endpoints.authorize);
    flow->setTokenUrl(endpoints.token);
    flow->setClientIdentifier(settings().clientId);
    flow->setClientIdentifierSharedKey(settings().clientSecret);
    flow->setRequestedScopeTokens({driveFileScope});
    flow->setReplyHandler(handler);
    flow->setModifyParametersFunction([](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* parameters) {
        if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
            // A refresh token too, so the sign-in lasts (D-080).
            parameters->insert(QStringLiteral("access_type"), QStringLiteral("offline"));
            parameters->insert(QStringLiteral("prompt"), QStringLiteral("consent"));
        }
    });
    connect(flow, &QAbstractOAuth::authorizeWithBrowser, this, [this](const QUrl& address) {
        if (browser) {
            browser(address);
        } else if (!QDesktopServices::openUrl(address)) {
            end(false, QString(), tr("No web browser could be opened for the Google sign-in."));
        }
    });
    connect(flow, &QAbstractOAuth::granted, this, [this]() {
        const QString refresh = flow->refreshToken();
        if (refresh.isEmpty()) {
            end(false, QString(), tr("Google did not give Chrono Vault a lasting sign-in. Try again."));
            return;
        }
        end(true, refresh, QString());
    });
    connect(flow, &QAbstractOAuth2::serverReportedErrorOccurred, this,
            [this](const QString& error, const QString&, const QUrl&) {
                end(false, QString(),
                    error == QLatin1String("access_denied") ? tr("The sign-in to Google was cancelled.")
                                                            : tr("Google refused the sign-in (%1).").arg(error));
            });
    connect(flow, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error) {
        end(false, QString(), tr("The sign-in to Google failed. Check the network and try again."));
    });
    flow->grant();
}

void GoogleSignIn::end(bool ok, const QString& refreshToken, const QString& problem)
{
    if (done) {
        return;
    }
    done = true;
    if (handler) {
        handler->close();
    }
    emit finished(ok, refreshToken, problem);
}

} // namespace chrono_vault
