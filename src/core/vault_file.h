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

#include "crypto.h"

#include <QByteArray>
#include <QString>

#include <functional>

// --- Version ------------------------------------------------------------
// vault_file.h 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

// The vault file (D-014): a short header, then the contents sealed with
// AES-256-GCM. The header is covered by the seal, so a change to it is
// detected too.
//
//   offset  size  what
//        0     8  "CHRVAULT"
//        8     2  format version, 1 (big-endian, as every number here)
//       10     1  key derivation, 1 = Argon2id
//       11     1  cipher, 1 = AES-256-GCM
//       12     4  Argon2id memory in KiB
//       16     4  Argon2id iterations
//       20     4  Argon2id lanes
//       24    16  salt
//       40     …  nonce (12), sealed contents, tag (16)
namespace chrono_vault {

// What is needed to save an unlocked vault again without asking for the
// master password: the derived key, and the salt and cost it came from.
struct VaultKey
{
    QByteArray key;
    QByteArray salt;
    crypto::KdfParameters parameters;

    [[nodiscard]] bool isValid() const { return key.size() == crypto::keyLength; }
    void wipe() { crypto::wipe(key); }
};

enum class OpenStatus {
    Opened,
    FileMissing,
    CannotRead,
    NotAVault,
    NewerFormat,
    WrongPasswordOrDamaged,
    TooLarge,
};

struct OpenResult
{
    OpenStatus status = OpenStatus::CannotRead;
    QByteArray contents; // the decrypted contents, when opened
    VaultKey key;        // to save again, when opened
};

enum class SaveStatus {
    Saved,
    CannotWrite,
    VerifyFailed,
    CannotReplace,
    Interrupted, // only from a test that stops the save on purpose
};

// The steps of a save, in order. A test may stop a save at any of them to
// prove the old vault survives (OR-15).
enum class SaveStep {
    WritingHalf,
    Written,
    Flushed,
    Verified,
};

using SaveStepCheck = std::function<bool(SaveStep)>;

// The largest vault file that is read at all (OR-4).
inline constexpr qint64 largestVaultFileBytes = 256LL * 1024 * 1024;

// The cost of deriving the key for a new vault.
[[nodiscard]] crypto::KdfParameters standardKdfParameters();

// A new key, from the password with a fresh salt; invalid when it fails.
[[nodiscard]] VaultKey makeVaultKey(const QByteArray& password, const crypto::KdfParameters& parameters);

// Reads and decrypts the vault at path. Slow on purpose: it derives the key.
[[nodiscard]] OpenResult openVaultFile(const QString& path, const QByteArray& password);

// Writes the contents to path, safely: a new file next to it, flushed to the
// disk, read back and decrypted, and only then put in the old one's place.
// The old vault is never touched before the new one is verified (OR-15).
// The check, when given, may stop the save at a step (tests only).
[[nodiscard]] SaveStatus saveVaultFile(const QString& path, const QByteArray& contents, const VaultKey& key,
                                       const SaveStepCheck& check = {});

// The name of the file a save writes before it replaces the vault.
[[nodiscard]] QString savingPathFor(const QString& path);

} // namespace chrono_vault
