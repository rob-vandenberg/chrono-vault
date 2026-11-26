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


#include "webdav_sync.h"

#include "settings_files.h"

// --- Version ------------------------------------------------------------
// webdav_sync.cpp 0.8.114

// --- Version history ----------------------------------------------------
// v0.8.114: Only where a vault is kept on a WebDAV server; the sync moved to cloud_sync.cpp (D-080)
// v0.5.78: The merge told this device's id (D-061)
// v0.3.62: Its own leftover upload removed first; the server's bytes equal to the base are no change (D-057)
// v0.3.61: A lock's empty placeholder is no version (pCloud); every step in the --debug log (D-057)
// v0.3.60: Initial release (D-057)

namespace chrono_vault {

namespace {

const QString placesKey = QStringLiteral("vaults/webdav");

} // namespace

QVariantMap WebDavPlace::toMap() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("folder"), folder.toString(QUrl::RemoveUserInfo)},
        {QStringLiteral("file"), fileName},
        {QStringLiteral("user"), user},
        {QStringLiteral("certificate"), QString::fromLatin1(trustedCertificate)},
        {QStringLiteral("tag"), QString::fromLatin1(tag.toBase64())},
        {QStringLiteral("stamp"), QString::fromLatin1(syncedStamp.toBase64())},
        {QStringLiteral("synced"), lastSynced.toUTC().toString(Qt::ISODate)},
    };
}

WebDavPlace WebDavPlace::fromMap(const QVariantMap& map)
{
    WebDavPlace place;
    place.id = map.value(QStringLiteral("id")).toString();
    place.folder = QUrl(map.value(QStringLiteral("folder")).toString());
    place.fileName = map.value(QStringLiteral("file")).toString();
    place.user = map.value(QStringLiteral("user")).toString();
    place.trustedCertificate = map.value(QStringLiteral("certificate")).toString().toLatin1();
    place.tag = QByteArray::fromBase64(map.value(QStringLiteral("tag")).toString().toLatin1());
    place.syncedStamp = QByteArray::fromBase64(map.value(QStringLiteral("stamp")).toString().toLatin1());
    place.lastSynced = QDateTime::fromString(map.value(QStringLiteral("synced")).toString(), Qt::ISODate);
    return place;
}

QString WebDavPlace::shownAddress() const
{
    return folderAddress(folder).resolved(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(fileName))))
        .toDisplayString(QUrl::RemoveUserInfo);
}

std::optional<WebDavPlace> webDavPlaceOf(const QString& workingCopy)
{
    const QVariantMap all = VaultsFile().value(placesKey).toMap();
    if (workingCopy.isEmpty() || !all.contains(workingCopy)) {
        return std::nullopt;
    }
    WebDavPlace place = WebDavPlace::fromMap(all.value(workingCopy).toMap());
    if (place.id.isEmpty() || !place.folder.isValid() || place.fileName.isEmpty()) {
        return std::nullopt; // damaged settings are not a place (OR-4)
    }
    return place;
}

void rememberWebDavPlace(const QString& workingCopy, const WebDavPlace& place)
{
    VaultsFile settings;
    QVariantMap all = settings.value(placesKey).toMap();
    all.insert(workingCopy, place.toMap());
    settings.setValue(placesKey, all);
}

void forgetWebDavPlace(const QString& workingCopy)
{
    VaultsFile settings;
    QVariantMap all = settings.value(placesKey).toMap();
    all.remove(workingCopy);
    settings.setValue(placesKey, all);
}

QString webDavDataFolder()
{
    return cloudDataFolder(QStringLiteral("webdav"));
}

QString webDavBasePath(const QString& id)
{
    return webDavDataFolder() + QStringLiteral("/") + id + QStringLiteral(".base.chronovault");
}

QString newWebDavId()
{
    return newCloudId();
}

CloudPlace WebDavPlace::toCloud() const
{
    return CloudPlace::fromMap(QStringLiteral("webdav"), toMap());
}

WebDavPlace WebDavPlace::fromCloud(const CloudPlace& place)
{
    return fromMap(place.toMap());
}

} // namespace chrono_vault
