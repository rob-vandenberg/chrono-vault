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

#include "cloud_sync.h"
#include "webdav_client.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <functional>
#include <optional>

// --- Version ------------------------------------------------------------
// webdav_sync.h 0.8.114

// --- Version history ----------------------------------------------------
// v0.8.114: Only where a vault is kept on a WebDAV server; the sync is CloudSync's (D-080)
// v0.3.60: Initial release (D-057)

namespace chrono_vault {

// Where one vault is kept on a WebDAV server, and how far this device got
// with it (D-057). Kept per working copy in chrono-vault.conf; the password
// is not here but in the system's store (SecretStore), under "webdav-<id>".
struct WebDavPlace
{
    QString id;               // names its password, its base file and its upload
    QUrl folder;
    QString fileName;         // the vault's file in that folder
    QString user;
    QByteArray trustedCertificate;
    QByteArray tag;           // the server's version tag at the last sync
    QByteArray syncedStamp;   // the working copy's stamp at the last sync
    QDateTime lastSynced;

    [[nodiscard]] QVariantMap toMap() const;
    [[nodiscard]] static WebDavPlace fromMap(const QVariantMap& map);
    [[nodiscard]] QString secretName() const { return QStringLiteral("webdav-") + id; }
    // The address shown to the person, without any password.
    [[nodiscard]] QString shownAddress() const;
    // The same place as every cloud storage keeps it (D-080).
    [[nodiscard]] CloudPlace toCloud() const;
    [[nodiscard]] static WebDavPlace fromCloud(const CloudPlace& place);
};

[[nodiscard]] std::optional<WebDavPlace> webDavPlaceOf(const QString& workingCopy);
void rememberWebDavPlace(const QString& workingCopy, const WebDavPlace& place);
void forgetWebDavPlace(const QString& workingCopy);
// The folder of the working copies and base files, in the program's data.
[[nodiscard]] QString webDavDataFolder();
// The last version this device and the server agreed on, for merging (D-054).
[[nodiscard]] QString webDavBasePath(const QString& id);
[[nodiscard]] QString newWebDavId();

} // namespace chrono_vault
