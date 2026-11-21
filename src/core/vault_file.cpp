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

#include "vault_file.h"

#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <filesystem>
#include <system_error>

#ifdef Q_OS_WIN
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// --- Version ------------------------------------------------------------
// vault_file.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

namespace chrono_vault {

namespace {

constexpr char magic[] = {'C', 'H', 'R', 'V', 'A', 'U', 'L', 'T'};
constexpr quint16 formatVersion = 1;
constexpr quint8 kdfArgon2id = 1;
constexpr quint8 cipherAes256Gcm = 1;
constexpr int headerLength = 40;

QByteArray makeHeader(const VaultKey& key)
{
    QByteArray header(headerLength, '\0');
    auto* bytes = reinterpret_cast<uchar*>(header.data());
    std::copy(std::begin(magic), std::end(magic), header.data());
    qToBigEndian<quint16>(formatVersion, bytes + 8);
    bytes[10] = kdfArgon2id;
    bytes[11] = cipherAes256Gcm;
    qToBigEndian<quint32>(key.parameters.memoryKiB, bytes + 12);
    qToBigEndian<quint32>(key.parameters.iterations, bytes + 16);
    qToBigEndian<quint32>(key.parameters.lanes, bytes + 20);
    std::copy(key.salt.cbegin(), key.salt.cend(), header.data() + 24);
    return header;
}

struct Header
{
    quint16 version = 0;
    quint8 kdf = 0;
    quint8 cipher = 0;
    crypto::KdfParameters parameters;
    QByteArray salt;
};

bool startsWithMagic(const QByteArray& data)
{
    return data.size() >= static_cast<qsizetype>(sizeof magic) &&
           std::equal(std::begin(magic), std::end(magic), data.constData());
}

Header readHeader(const QByteArray& data)
{
    const auto* bytes = reinterpret_cast<const uchar*>(data.constData());
    Header header;
    header.version = qFromBigEndian<quint16>(bytes + 8);
    header.kdf = bytes[10];
    header.cipher = bytes[11];
    header.parameters.memoryKiB = qFromBigEndian<quint32>(bytes + 12);
    header.parameters.iterations = qFromBigEndian<quint32>(bytes + 16);
    header.parameters.lanes = qFromBigEndian<quint32>(bytes + 20);
    header.salt = data.mid(24, crypto::saltLength);
    return header;
}

// Flushes the file's data to the disk itself, not only to the system's cache.
bool flushToDisk(QFile& file)
{
    if (!file.flush()) {
        return false;
    }
#ifdef Q_OS_WIN
    return _commit(file.handle()) == 0;
#else
    return ::fsync(file.handle()) == 0;
#endif
}

// After a rename, the folder's own entry must reach the disk too (Linux).
void flushFolder(const QString& path)
{
#ifndef Q_OS_WIN
    const QByteArray folder = QFile::encodeName(QFileInfo(path).absolutePath());
    const int handle = ::open(folder.constData(), O_RDONLY);
    if (handle >= 0) {
        ::fsync(handle);
        ::close(handle);
    }
#else
    Q_UNUSED(path);
#endif
}

} // namespace

crypto::KdfParameters standardKdfParameters()
{
    return crypto::KdfParameters{};
}

VaultKey makeVaultKey(const QByteArray& password, const crypto::KdfParameters& parameters)
{
    VaultKey vaultKey;
    vaultKey.parameters = parameters;
    vaultKey.salt = crypto::randomBytes(crypto::saltLength);
    if (vaultKey.salt.size() != crypto::saltLength) {
        return {};
    }
    if (auto key = crypto::deriveKey(password, vaultKey.salt, parameters)) {
        vaultKey.key = *key;
        crypto::wipe(*key);
    }
    return vaultKey;
}

QString savingPathFor(const QString& path)
{
    return path + QStringLiteral(".saving");
}

OpenResult openVaultFile(const QString& path, const QByteArray& password)
{
    OpenResult result;
    QFile file(path);
    if (!file.exists()) {
        result.status = OpenStatus::FileMissing;
        return result;
    }
    if (file.size() > largestVaultFileBytes) {
        result.status = OpenStatus::TooLarge;
        return result;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        result.status = OpenStatus::CannotRead;
        return result;
    }
    const QByteArray data = file.read(largestVaultFileBytes + 1);
    if (data.size() > largestVaultFileBytes) {
        result.status = OpenStatus::TooLarge;
        return result;
    }
    if (!startsWithMagic(data)) {
        result.status = OpenStatus::NotAVault;
        return result;
    }
    if (data.size() < headerLength + crypto::nonceLength + crypto::tagLength) {
        result.status = OpenStatus::WrongPasswordOrDamaged;
        return result;
    }
    const Header header = readHeader(data);
    if (header.version > formatVersion) {
        result.status = OpenStatus::NewerFormat;
        return result;
    }
    if (header.version != formatVersion || header.kdf != kdfArgon2id || header.cipher != cipherAes256Gcm ||
        !header.parameters.isWithinLimits()) {
        result.status = OpenStatus::WrongPasswordOrDamaged;
        return result;
    }

    VaultKey key;
    key.salt = header.salt;
    key.parameters = header.parameters;
    auto derived = crypto::deriveKey(password, key.salt, key.parameters);
    if (!derived) {
        result.status = OpenStatus::WrongPasswordOrDamaged;
        return result;
    }
    key.key = *derived;
    crypto::wipe(*derived);

    auto contents = crypto::open(key.key, data.mid(headerLength), data.left(headerLength));
    if (!contents) {
        key.wipe();
        result.status = OpenStatus::WrongPasswordOrDamaged;
        return result;
    }
    result.status = OpenStatus::Opened;
    result.contents = *contents;
    crypto::wipe(*contents);
    result.key = key;
    return result;
}

SaveStatus saveVaultFile(const QString& path, const QByteArray& contents, const VaultKey& key,
                         const SaveStepCheck& check)
{
    const auto goOn = [&check](SaveStep step) { return !check || check(step); };

    if (!key.isValid()) {
        return SaveStatus::CannotWrite;
    }
    const QByteArray header = makeHeader(key);
    const QByteArray sealed = crypto::seal(key.key, contents, header);
    if (sealed.isEmpty()) {
        return SaveStatus::CannotWrite;
    }
    const QByteArray whole = header + sealed;

    const QString savingPath = savingPathFor(path);
    {
        QFile saving(savingPath);
        if (!saving.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return SaveStatus::CannotWrite;
        }
        const qsizetype half = whole.size() / 2;
        if (saving.write(whole.constData(), half) != half) {
            saving.close();
            QFile::remove(savingPath);
            return SaveStatus::CannotWrite;
        }
        if (!goOn(SaveStep::WritingHalf)) {
            return SaveStatus::Interrupted;
        }
        if (saving.write(whole.constData() + half, whole.size() - half) != whole.size() - half) {
            saving.close();
            QFile::remove(savingPath);
            return SaveStatus::CannotWrite;
        }
        if (!goOn(SaveStep::Written)) {
            return SaveStatus::Interrupted;
        }
        if (!flushToDisk(saving)) {
            saving.close();
            QFile::remove(savingPath);
            return SaveStatus::CannotWrite;
        }
        saving.close();
        if (!goOn(SaveStep::Flushed)) {
            return SaveStatus::Interrupted;
        }
    }

    // Read the new file back from the disk and decrypt it before the old
    // vault is touched.
    {
        QFile written(savingPath);
        const QByteArray readBack = written.open(QIODevice::ReadOnly) ? written.readAll() : QByteArray();
        written.close();
        auto opened = readBack.size() > headerLength
                          ? crypto::open(key.key, readBack.mid(headerLength), readBack.left(headerLength))
                          : std::nullopt;
        const bool same = opened && *opened == contents && readBack.left(headerLength) == header;
        if (opened) {
            crypto::wipe(*opened);
        }
        if (!same) {
            QFile::remove(savingPath);
            return SaveStatus::VerifyFailed;
        }
    }
    if (!goOn(SaveStep::Verified)) {
        return SaveStatus::Interrupted;
    }

    // One step replaces the old vault with the new one.
    std::error_code error;
    std::filesystem::rename(std::filesystem::path(savingPath.toStdU16String()),
                            std::filesystem::path(path.toStdU16String()), error);
    if (error) {
        QFile::remove(savingPath);
        return SaveStatus::CannotReplace;
    }
    flushFolder(path);
    return SaveStatus::Saved;
}

} // namespace chrono_vault
