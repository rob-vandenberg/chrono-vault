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

#include <QByteArray>
#include <QtGlobal>

#include <optional>

// --- Version ------------------------------------------------------------
// crypto.h 0.0.8

// --- Version history ----------------------------------------------------
// v0.0.8: The version of OpenSSL in use, for the About box
// v0.0.2: Initial release

// The encryption of Chrono Vault (D-013): the key is derived from the master
// password with Argon2id, and the vault is sealed with AES-256-GCM, both
// from OpenSSL 3.
namespace chrono_vault::crypto {

inline constexpr int keyLength = 32;   // AES-256
inline constexpr int saltLength = 16;
inline constexpr int nonceLength = 12; // GCM
inline constexpr int tagLength = 16;   // GCM

// The cost of deriving the key. Stored in the vault file, so it can be
// raised later without losing anything.
struct KdfParameters
{
    quint32 memoryKiB = 64 * 1024; // 64 MB per derivation
    quint32 iterations = 3;
    quint32 lanes = 4;

    // A vault file can carry any numbers; these bounds keep a damaged or
    // hostile file from asking for gigabytes of memory (OR-4).
    [[nodiscard]] bool isWithinLimits() const;
};

// Bytes from the system's secure random source; empty when it fails.
[[nodiscard]] QByteArray randomBytes(int count);

// The 32-byte key for this password, salt and cost; nothing when the
// parameters are outside the limits or OpenSSL fails.
[[nodiscard]] std::optional<QByteArray> deriveKey(const QByteArray& password, const QByteArray& salt,
                                                  const KdfParameters& parameters);

// Seals plaintext: a fresh nonce, the ciphertext and the tag, in that
// order. The associated data is not encrypted but is covered by the tag, so
// a change to it is detected. Empty when it fails.
[[nodiscard]] QByteArray seal(const QByteArray& key, const QByteArray& plaintext,
                              const QByteArray& associatedData);

// Opens what seal() made; nothing when the key is wrong or a single byte of
// the sealed data or the associated data was changed.
[[nodiscard]] std::optional<QByteArray> open(const QByteArray& key, const QByteArray& sealed,
                                             const QByteArray& associatedData);

// Overwrites the bytes with zeros in a way the compiler cannot leave out.
void wipe(QByteArray& secret);

// The version of OpenSSL in use, as "3.5.9", for the About box.
[[nodiscard]] QByteArray openSslVersion();

} // namespace chrono_vault::crypto
