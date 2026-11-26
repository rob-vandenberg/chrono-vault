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
#include "webdav_client.h"

// --- Version ------------------------------------------------------------
// webdav_storage.h 0.8.114

// --- Version history ----------------------------------------------------
// v0.8.114: Initial release: WebDAV as one cloud storage (D-080)

namespace chrono_vault {

// A WebDAV folder as a cloud storage (D-057, D-080): every request is the
// WebDAV client's own, so WebDAV behaves exactly as before.
class WebDavStorage : public CloudStorage
{
    Q_OBJECT

public:
    explicit WebDavStorage(WebDavAccount account, QObject* parent = nullptr);

    [[nodiscard]] const WebDavAccount& account() const { return client.account(); }

    void signIn(Done done) override;
    void list(Listed done) override;
    void get(const QString& name, const QByteArray& ifNotTag, Done done) override;
    void put(const QString& name, const QByteArray& data, Done done) override;
    void remove(const QString& name, Done done) override;
    void makeFolder(const QString& name, Done done) override;
    void tagOf(const QString& name, Done done) override;
    void move(const QString& from, const QString& to, const QByteArray& lockToken, Done done,
              bool overwrite = true) override;
    void lock(const QString& name, Done done) override;
    void unlock(const QString& name, const QByteArray& lockToken, Done done) override;

private:
    WebDavClient client;
};

} // namespace chrono_vault
