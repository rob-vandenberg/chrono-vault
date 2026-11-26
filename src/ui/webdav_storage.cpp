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


#include "webdav_storage.h"

// --- Version ------------------------------------------------------------
// webdav_storage.cpp 0.8.114

// --- Version history ----------------------------------------------------
// v0.8.114: Initial release: WebDAV as one cloud storage (D-080)

namespace chrono_vault {

WebDavStorage::WebDavStorage(WebDavAccount account, QObject* parent)
    : CloudStorage(parent), client(std::move(account), this)
{
}

void WebDavStorage::signIn(Done done)
{
    // Listing the folder proves the address, the account and the folder.
    client.list([done = std::move(done)](const CloudReply& answer, const QList<CloudEntry>&) { done(answer); });
}

void WebDavStorage::list(Listed done)
{
    client.list(std::move(done));
}

void WebDavStorage::get(const QString& name, const QByteArray& ifNotTag, Done done)
{
    client.get(name, ifNotTag, std::move(done));
}

void WebDavStorage::put(const QString& name, const QByteArray& data, Done done)
{
    client.put(name, data, std::move(done));
}

void WebDavStorage::remove(const QString& name, Done done)
{
    client.remove(name, std::move(done));
}

void WebDavStorage::makeFolder(const QString& name, Done done)
{
    client.makeFolder(name, std::move(done));
}

void WebDavStorage::tagOf(const QString& name, Done done)
{
    client.tagOf(name, std::move(done));
}

void WebDavStorage::move(const QString& from, const QString& to, const QByteArray& lockToken, Done done,
                         bool overwrite)
{
    client.move(from, to, lockToken, std::move(done), overwrite);
}

void WebDavStorage::lock(const QString& name, Done done)
{
    client.lock(name, std::move(done));
}

void WebDavStorage::unlock(const QString& name, const QByteArray& lockToken, Done done)
{
    client.unlock(name, lockToken, std::move(done));
}

} // namespace chrono_vault
