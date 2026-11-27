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


#include "cloud_storage.h"

#include "google_drive_storage.h"
#include "settings_files.h"
#include "webdav_storage.h"
#include "webdav_sync.h"

#include <QCoreApplication>
#include <QPointer>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QUrl>

// --- Version ------------------------------------------------------------
// cloud_storage.cpp 0.8.117

// --- Version history ----------------------------------------------------
// v0.8.117: signedInAs answers NotSupported by default; shownCloudAccount (D-080 point 20)
// v0.8.115: Google Drive in the list of storages, when the build has its client (D-080)
// v0.8.114: Initial release: the base of every cloud storage, and where a vault is kept (D-080)

namespace chrono_vault {

namespace {

const QString webDavKind = QStringLiteral("webdav");
const QString googleDriveKind = QStringLiteral("googledrive");

QString placesKey(const QString& kind)
{
    return QStringLiteral("vaults/") + kind;
}

// A place read from the settings is used only when it is whole (OR-4).
bool usable(const CloudPlace& place)
{
    if (place.id.isEmpty() || place.fileName.isEmpty()) {
        return false;
    }
    if (place.kind == webDavKind) {
        return QUrl(place.details.value(QStringLiteral("folder")).toString()).isValid();
    }
    if (place.kind == googleDriveKind) {
        return !place.details.value(QStringLiteral("folderId")).toString().isEmpty();
    }
    return true;
}

} // namespace

void CloudStorage::lock(const QString& name, Done done)
{
    Q_UNUSED(name)
    CloudReply reply;
    reply.error = CloudError::NotSupported;
    answerLater(std::move(done), reply);
}

void CloudStorage::signedInAs(Done done)
{
    CloudReply reply;
    reply.error = CloudError::NotSupported;
    answerLater(std::move(done), reply);
}

void CloudStorage::unlock(const QString& name, const QByteArray& lockToken, Done done)
{
    Q_UNUSED(name)
    Q_UNUSED(lockToken)
    answerLater(std::move(done), CloudReply{});
}

void CloudStorage::answerLater(Done done, CloudReply reply)
{
    QPointer<CloudStorage> self(this);
    QMetaObject::invokeMethod(
        this,
        [self, done = std::move(done), reply = std::move(reply)]() {
            if (self && done) {
                done(reply);
            }
        },
        Qt::QueuedConnection);
}

QVariantMap CloudPlace::toMap() const
{
    QVariantMap map = details;
    map.insert(QStringLiteral("id"), id);
    map.insert(QStringLiteral("file"), fileName);
    map.insert(QStringLiteral("tag"), QString::fromLatin1(tag.toBase64()));
    map.insert(QStringLiteral("stamp"), QString::fromLatin1(syncedStamp.toBase64()));
    map.insert(QStringLiteral("synced"), lastSynced.toUTC().toString(Qt::ISODate));
    return map;
}

CloudPlace CloudPlace::fromMap(const QString& kind, const QVariantMap& map)
{
    CloudPlace place;
    place.kind = kind;
    place.id = map.value(QStringLiteral("id")).toString();
    place.fileName = map.value(QStringLiteral("file")).toString();
    place.tag = QByteArray::fromBase64(map.value(QStringLiteral("tag")).toString().toLatin1());
    place.syncedStamp = QByteArray::fromBase64(map.value(QStringLiteral("stamp")).toString().toLatin1());
    place.lastSynced = QDateTime::fromString(map.value(QStringLiteral("synced")).toString(), Qt::ISODate);
    place.details = map;
    for (const QString& common : {QStringLiteral("id"), QStringLiteral("file"), QStringLiteral("tag"),
                                  QStringLiteral("stamp"), QStringLiteral("synced")}) {
        place.details.remove(common);
    }
    return place;
}

std::optional<CloudPlace> cloudPlaceOf(const QString& workingCopy)
{
    if (workingCopy.isEmpty()) {
        return std::nullopt;
    }
    const VaultsFile settings;
    for (const CloudKind& kind : cloudKinds()) {
        const QVariantMap all = settings.value(placesKey(kind.kind)).toMap();
        if (!all.contains(workingCopy)) {
            continue;
        }
        CloudPlace place = CloudPlace::fromMap(kind.kind, all.value(workingCopy).toMap());
        if (usable(place)) {
            return place;
        }
        return std::nullopt; // damaged settings are not a place (OR-4)
    }
    return std::nullopt;
}

void rememberCloudPlace(const QString& workingCopy, const CloudPlace& place)
{
    VaultsFile settings;
    QVariantMap all = settings.value(placesKey(place.kind)).toMap();
    all.insert(workingCopy, place.toMap());
    settings.setValue(placesKey(place.kind), all);
}

void forgetCloudPlace(const QString& workingCopy)
{
    VaultsFile settings;
    for (const CloudKind& kind : cloudKinds()) {
        QVariantMap all = settings.value(placesKey(kind.kind)).toMap();
        if (all.remove(workingCopy) > 0) {
            settings.setValue(placesKey(kind.kind), all);
        }
    }
}

QString cloudDataFolder(const QString& kind)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/chrono-vault/") +
           kind;
}

QString cloudBasePath(const CloudPlace& place)
{
    return cloudDataFolder(place.kind) + QStringLiteral("/") + place.id + QStringLiteral(".base.chronovault");
}

QString newCloudId()
{
    return QString::number(QRandomGenerator::system()->generate64(), 16);
}

QString shownCloudAddress(const CloudPlace& place)
{
    if (place.kind == webDavKind) {
        return WebDavPlace::fromCloud(place).shownAddress();
    }
    if (place.kind == googleDriveKind) {
        return place.details.value(QStringLiteral("folderPath")).toString() + QLatin1Char('/') + place.fileName;
    }
    return place.fileName;
}

QString shownCloudAccount(const CloudPlace& place)
{
    const QString kept = place.details.value(cloudAccountKey).toString();
    if (!kept.isEmpty()) {
        return kept;
    }
    if (place.kind == webDavKind) {
        // The user and the server are in the place record already.
        const WebDavPlace webDav = WebDavPlace::fromCloud(place);
        const QString server = webDav.folder.host();
        if (webDav.user.isEmpty()) {
            return server;
        }
        return server.isEmpty() ? webDav.user
                                : QCoreApplication::translate("CloudStorage", "%1 on %2").arg(webDav.user, server);
    }
    return {};
}

QList<CloudKind> cloudKinds()
{
    QList<CloudKind> kinds{
        CloudKind{webDavKind, QStringLiteral("WebDav"), QCoreApplication::translate("CloudStorage", "WebDAV"),
                  QStringLiteral("qrc:/icons/storage-webdav.svg"), QStringLiteral("qrc:/icons/places-webdav.svg")},
    };
    if (googleDriveAvailable()) {
        // Only in a build that has the program's Google client (the owner, Q6).
        kinds.append(CloudKind{googleDriveKind, QStringLiteral("GoogleDrive"),
                               QCoreApplication::translate("CloudStorage", "Google Drive"),
                               QStringLiteral("qrc:/icons/storage-googledrive.svg"),
                               QStringLiteral("qrc:/icons/places-googledrive.svg")});
    }
    return kinds;
}

std::optional<CloudKind> cloudKindOf(const QString& kind)
{
    for (const CloudKind& one : cloudKinds()) {
        if (one.kind == kind) {
            return one;
        }
    }
    return std::nullopt;
}

QVariantList cloudKindsForWindows()
{
    QVariantList list;
    for (const CloudKind& one : cloudKinds()) {
        list.append(QVariantMap{
            {QStringLiteral("kind"), one.kind},
            {QStringLiteral("identifier"), one.identifier},
            {QStringLiteral("name"), one.name},
            {QStringLiteral("tile"), one.tilePicture},
            {QStringLiteral("place"), one.placePicture},
        });
    }
    return list;
}

std::unique_ptr<CloudStorage> makeCloudStorage(const CloudPlace& place, const QString& secret)
{
    if (place.kind == webDavKind) {
        const WebDavPlace where = WebDavPlace::fromCloud(place);
        return std::make_unique<WebDavStorage>(
            WebDavAccount{where.folder, where.user, secret, where.trustedCertificate});
    }
    if (place.kind == googleDriveKind && googleDriveAvailable()) {
        return std::make_unique<GoogleDriveStorage>(
            GoogleDriveAccount{secret, place.details.value(QStringLiteral("folderId")).toString(),
                               place.details.value(QStringLiteral("folderPath")).toString()});
    }
    return nullptr;
}

} // namespace chrono_vault
