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


#include "secret_store.h"

#include <windows.h>
#include <wincred.h>

#include <string>

// --- Version ------------------------------------------------------------
// secret_store_windows.cpp 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

// The Windows Credential Manager: a generic credential per secret, kept for
// this person on this computer.

namespace chrono_vault {

namespace {

std::wstring targetOf(const QString& name)
{
    return (QStringLiteral("chrono-vault/") + name).toStdWString();
}

class CredentialStore : public SecretStore
{
public:
    bool keep(const QString& name, const QByteArray& secret) override
    {
        std::wstring target = targetOf(name);
        std::wstring user = L"Chrono Vault";
        CREDENTIALW credential{};
        credential.Type = CRED_TYPE_GENERIC;
        credential.TargetName = target.data();
        credential.CredentialBlobSize = static_cast<DWORD>(secret.size());
        credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.constData()));
        credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
        credential.UserName = user.data();
        return CredWriteW(&credential, 0) != FALSE;
    }

    std::pair<Result, QByteArray> find(const QString& name) override
    {
        const std::wstring target = targetOf(name);
        PCREDENTIALW found = nullptr;
        if (CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &found) == FALSE) {
            return {GetLastError() == ERROR_NOT_FOUND ? Result::NoSecret : Result::Unavailable, {}};
        }
        QByteArray secret(reinterpret_cast<const char*>(found->CredentialBlob),
                          static_cast<qsizetype>(found->CredentialBlobSize));
        SecureZeroMemory(found->CredentialBlob, found->CredentialBlobSize);
        CredFree(found);
        return {Result::Done, secret};
    }

    bool remove(const QString& name) override
    {
        const std::wstring target = targetOf(name);
        return CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) != FALSE || GetLastError() == ERROR_NOT_FOUND;
    }
};

} // namespace

std::unique_ptr<SecretStore> makeSecretStore()
{
    return std::make_unique<CredentialStore>();
}

} // namespace chrono_vault
