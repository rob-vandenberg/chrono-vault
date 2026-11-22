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

#include <QCryptographicHash>
#include <QSaveFile>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QtEndian>

#include <algorithm>
#include <filesystem>
#include <system_error>

#ifdef Q_OS_WIN
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

// --- Version ------------------------------------------------------------
// vault_file.cpp 0.0.22

// --- Version history ----------------------------------------------------
// v0.0.22: A new keyfile readable by the person alone
// v0.0.18: On Windows, putting the new file in place is tried again briefly while another program holds the vault
//          (the owner, Q37)
// v0.0.16: Vault files read so that another program, or a save, can replace them meanwhile (Windows);
//          a vault file's stamp, different after every save, to notice a change made elsewhere (D-032)
// v0.0.15: A keyfile beside the master password: header format 2 (D-031); opening with a key kept from before (D-030)
// v0.0.2: Initial release

namespace chrono_vault {

namespace {

constexpr char magic[] = {'C', 'H', 'R', 'V', 'A', 'U', 'L', 'T'};
constexpr quint16 formatVersion = 1;
constexpr quint16 keyfileFormatVersion = 2;
constexpr quint8 kdfArgon2id = 1;
constexpr quint8 kdfArgon2idWithKeyfile = 2;
constexpr int keyfileLength = 64;

// What Argon2id is given: the password, then the keyfile's fingerprint.
QByteArray secretOf(const QByteArray& password, const QByteArray& keyfile)
{
    return keyfile.isEmpty() ? password : password + keyfile;
}
constexpr quint8 cipherAes256Gcm = 1;
constexpr int headerLength = 40;

QByteArray makeHeader(const VaultKey& key)
{
    QByteArray header(headerLength, '\0');
    auto* bytes = reinterpret_cast<uchar*>(header.data());
    std::copy(std::begin(magic), std::end(magic), header.data());
    // A vault without a keyfile stays format 1, so an older Chrono Vault
    // still opens it; one with a keyfile is format 2 (D-031).
    qToBigEndian<quint16>(key.usesKeyfile ? keyfileFormatVersion : formatVersion, bytes + 8);
    bytes[10] = key.usesKeyfile ? kdfArgon2idWithKeyfile : kdfArgon2id;
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

// Reads at most the first `most` bytes of a file, leaving it free to be
// replaced meanwhile: on Windows, a file another program holds open cannot be
// replaced unless it was opened to allow that, so a sync program's or our own
// save's replace would fail while a vault is read (D-032).
std::optional<QByteArray> readStartOf(const QString& path, qint64 most)
{
#ifdef Q_OS_WIN
    const std::wstring name = QDir::toNativeSeparators(path).toStdWString();
    HANDLE handle = CreateFileW(name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }
    QByteArray data;
    constexpr DWORD piece = 1024 * 1024;
    bool failed = false;
    while (data.size() < most) {
        const qsizetype before = data.size();
        const DWORD wanted = static_cast<DWORD>(std::min<qint64>(piece, most - before));
        data.resize(before + wanted);
        DWORD got = 0;
        if (!ReadFile(handle, data.data() + before, wanted, &got, nullptr)) {
            failed = true;
            data.resize(before);
            break;
        }
        data.resize(before + got);
        if (got == 0) {
            break;
        }
    }
    CloseHandle(handle);
    if (failed) {
        return std::nullopt;
    }
    return data;
#else
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QByteArray data = file.read(most);
    if (file.error() != QFileDevice::NoError) {
        return std::nullopt;
    }
    return data;
#endif
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

VaultKey makeVaultKey(const QByteArray& password, const crypto::KdfParameters& parameters, const QByteArray& keyfile)
{
    VaultKey vaultKey;
    vaultKey.parameters = parameters;
    vaultKey.usesKeyfile = !keyfile.isEmpty();
    vaultKey.salt = crypto::randomBytes(crypto::saltLength);
    if (vaultKey.salt.size() != crypto::saltLength) {
        return {};
    }
    QByteArray secret = secretOf(password, keyfile);
    if (auto key = crypto::deriveKey(secret, vaultKey.salt, parameters)) {
        vaultKey.key = *key;
        crypto::wipe(*key);
    }
    crypto::wipe(secret);
    return vaultKey;
}

std::optional<QByteArray> keyfileFingerprint(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > largestKeyfileBytes) {
        return std::nullopt;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 total = 0;
    while (!file.atEnd()) {
        const QByteArray piece = file.read(1024 * 1024);
        if (piece.isEmpty() && file.error() != QFileDevice::NoError) {
            return std::nullopt;
        }
        total += piece.size();
        if (total > largestKeyfileBytes) {
            return std::nullopt;
        }
        hash.addData(piece);
    }
    if (file.error() != QFileDevice::NoError) {
        return std::nullopt;
    }
    return hash.result();
}

QByteArray vaultFileStamp(const QString& path)
{
    const auto stamp = readStartOf(path, headerLength + crypto::nonceLength);
    return stamp && stamp->size() == headerLength + crypto::nonceLength ? *stamp : QByteArray();
}

bool makeKeyfile(const QString& path)
{
    const QByteArray bytes = crypto::randomBytes(keyfileLength);
    if (bytes.size() != keyfileLength) {
        return false;
    }
    QSaveFile file(path);
    // A keyfile is a secret: readable by the person alone.
    return file.open(QIODevice::WriteOnly) && file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
           file.write(bytes) == bytes.size() && file.commit();
}

QString savingPathFor(const QString& path)
{
    return path + QStringLiteral(".saving");
}

namespace {

// The file read and its header checked; status Opened means "go on".
struct ReadFile
{
    OpenStatus status = OpenStatus::CannotRead;
    QByteArray data;
    Header header;
};

ReadFile readVaultFile(const QString& path)
{
    ReadFile read;
    if (!QFileInfo::exists(path)) {
        read.status = OpenStatus::FileMissing;
        return read;
    }
    if (QFileInfo(path).size() > largestVaultFileBytes) {
        read.status = OpenStatus::TooLarge;
        return read;
    }
    const auto data = readStartOf(path, largestVaultFileBytes + 1);
    if (!data) {
        read.status = OpenStatus::CannotRead;
        return read;
    }
    read.data = *data;
    if (read.data.size() > largestVaultFileBytes) {
        read.status = OpenStatus::TooLarge;
        return read;
    }
    if (!startsWithMagic(read.data)) {
        read.status = OpenStatus::NotAVault;
        return read;
    }
    if (read.data.size() < headerLength + crypto::nonceLength + crypto::tagLength) {
        read.status = OpenStatus::WrongPasswordOrDamaged;
        return read;
    }
    read.header = readHeader(read.data);
    if (read.header.version > keyfileFormatVersion) {
        read.status = OpenStatus::NewerFormat;
        return read;
    }
    const bool plain = read.header.version == formatVersion && read.header.kdf == kdfArgon2id;
    const bool withKeyfile =
        read.header.version == keyfileFormatVersion && read.header.kdf == kdfArgon2idWithKeyfile;
    if ((!plain && !withKeyfile) || read.header.cipher != cipherAes256Gcm ||
        !read.header.parameters.isWithinLimits()) {
        read.status = OpenStatus::WrongPasswordOrDamaged;
        return read;
    }
    read.status = OpenStatus::Opened;
    return read;
}

OpenResult decrypt(const ReadFile& read, VaultKey key)
{
    OpenResult result;
    auto contents = crypto::open(key.key, read.data.mid(headerLength), read.data.left(headerLength));
    if (!contents) {
        key.wipe();
        result.status = OpenStatus::WrongPasswordOrDamaged;
        return result;
    }
    result.status = OpenStatus::Opened;
    result.contents = *contents;
    crypto::wipe(*contents);
    result.key = key;
    result.stamp = read.data.left(headerLength + crypto::nonceLength);
    return result;
}

} // namespace

OpenResult openVaultFile(const QString& path, const QByteArray& password, const QByteArray& keyfile)
{
    OpenResult result;
    const ReadFile read = readVaultFile(path);
    if (read.status != OpenStatus::Opened) {
        result.status = read.status;
        return result;
    }
    VaultKey key;
    key.salt = read.header.salt;
    key.parameters = read.header.parameters;
    key.usesKeyfile = read.header.kdf == kdfArgon2idWithKeyfile;
    if (key.usesKeyfile && keyfile.isEmpty()) {
        result.status = OpenStatus::KeyfileNeeded;
        return result;
    }
    // A keyfile given for a vault that has none is not used.
    QByteArray secret = secretOf(password, key.usesKeyfile ? keyfile : QByteArray());
    auto derived = crypto::deriveKey(secret, key.salt, key.parameters);
    crypto::wipe(secret);
    if (!derived) {
        result.status = OpenStatus::WrongPasswordOrDamaged;
        return result;
    }
    key.key = *derived;
    crypto::wipe(*derived);
    return decrypt(read, key);
}

OpenResult openVaultFileWithKey(const QString& path, const VaultKey& key)
{
    OpenResult result;
    const ReadFile read = readVaultFile(path);
    if (read.status != OpenStatus::Opened) {
        result.status = read.status;
        return result;
    }
    if (read.header.salt != key.salt || read.header.parameters.memoryKiB != key.parameters.memoryKiB ||
        read.header.parameters.iterations != key.parameters.iterations ||
        read.header.parameters.lanes != key.parameters.lanes ||
        (read.header.kdf == kdfArgon2idWithKeyfile) != key.usesKeyfile) {
        result.status = OpenStatus::KeyChanged;
        return result;
    }
    return decrypt(read, key);
}

SaveStatus saveVaultFile(const QString& path, const QByteArray& contents, const VaultKey& key,
                         const SaveStepCheck& check, QByteArray* writtenStamp)
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
#ifdef Q_OS_WIN
    // On Windows a file another program holds open (a virus scanner reading
    // the fresh vault, a sync program) cannot be replaced for that moment:
    // the step is tried again, at most ten times within about a second, only
    // for "access denied" or "in use" (the owner's decision, Q37). The old
    // vault is untouched meanwhile (OR-15).
    for (int attempt = 0; error && attempt < 10 &&
                          (error.value() == ERROR_ACCESS_DENIED || error.value() == ERROR_SHARING_VIOLATION);
         ++attempt) {
        QThread::msleep(100);
        error.clear();
        std::filesystem::rename(std::filesystem::path(savingPath.toStdU16String()),
                                std::filesystem::path(path.toStdU16String()), error);
    }
#endif
    if (error) {
        QFile::remove(savingPath);
        return SaveStatus::CannotReplace;
    }
    flushFolder(path);
    if (writtenStamp) {
        *writtenStamp = whole.left(headerLength + crypto::nonceLength);
    }
    return SaveStatus::Saved;
}

} // namespace chrono_vault
