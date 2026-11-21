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

#include "crypto.h"

#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <memory>

// --- Version ------------------------------------------------------------
// crypto.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

namespace chrono_vault::crypto {

namespace {

struct KdfDeleter
{
    void operator()(EVP_KDF* kdf) const { EVP_KDF_free(kdf); }
};
struct KdfContextDeleter
{
    void operator()(EVP_KDF_CTX* context) const { EVP_KDF_CTX_free(context); }
};
struct CipherContextDeleter
{
    void operator()(EVP_CIPHER_CTX* context) const { EVP_CIPHER_CTX_free(context); }
};

unsigned char* bytesOf(QByteArray& array)
{
    return reinterpret_cast<unsigned char*>(array.data());
}

const unsigned char* bytesOf(const QByteArray& array)
{
    return reinterpret_cast<const unsigned char*>(array.constData());
}

} // namespace

bool KdfParameters::isWithinLimits() const
{
    constexpr quint32 largestMemoryKiB = 1024 * 1024; // 1 GB
    return lanes >= 1 && lanes <= 16 && iterations >= 1 && iterations <= 64 && memoryKiB >= 8 * lanes &&
           memoryKiB <= largestMemoryKiB;
}

QByteArray randomBytes(int count)
{
    if (count <= 0) {
        return {};
    }
    QByteArray bytes(count, Qt::Uninitialized);
    if (RAND_bytes(bytesOf(bytes), count) != 1) {
        return {};
    }
    return bytes;
}

std::optional<QByteArray> deriveKey(const QByteArray& password, const QByteArray& salt,
                                    const KdfParameters& parameters)
{
    if (!parameters.isWithinLimits() || salt.size() < 8) {
        return std::nullopt;
    }
    const std::unique_ptr<EVP_KDF, KdfDeleter> kdf(EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr));
    if (!kdf) {
        return std::nullopt;
    }
    const std::unique_ptr<EVP_KDF_CTX, KdfContextDeleter> context(EVP_KDF_CTX_new(kdf.get()));
    if (!context) {
        return std::nullopt;
    }

    // OpenSSL wants a pointer even for an empty password.
    static const char nothing = 0;
    void* passwordBytes = const_cast<char*>(password.isEmpty() ? &nothing : password.constData());
    quint32 iterations = parameters.iterations;
    quint32 lanes = parameters.lanes;
    quint32 memoryKiB = parameters.memoryKiB;
    OSSL_PARAM settings[] = {
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD, passwordBytes,
                                          static_cast<size_t>(password.size())),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<char*>(salt.constData()),
                                          static_cast<size_t>(salt.size())),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &memoryKiB),
        OSSL_PARAM_construct_end(),
    };

    QByteArray key(keyLength, Qt::Uninitialized);
    if (EVP_KDF_derive(context.get(), bytesOf(key), static_cast<size_t>(key.size()), settings) != 1) {
        wipe(key);
        return std::nullopt;
    }
    return key;
}

QByteArray seal(const QByteArray& key, const QByteArray& plaintext, const QByteArray& associatedData)
{
    if (key.size() != keyLength) {
        return {};
    }
    const QByteArray nonce = randomBytes(nonceLength);
    if (nonce.size() != nonceLength) {
        return {};
    }
    const std::unique_ptr<EVP_CIPHER_CTX, CipherContextDeleter> context(EVP_CIPHER_CTX_new());
    if (!context || EVP_EncryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, nonceLength, nullptr) != 1 ||
        EVP_EncryptInit_ex(context.get(), nullptr, nullptr, bytesOf(key), bytesOf(nonce)) != 1) {
        return {};
    }

    int length = 0;
    if (!associatedData.isEmpty() &&
        EVP_EncryptUpdate(context.get(), nullptr, &length, bytesOf(associatedData),
                          static_cast<int>(associatedData.size())) != 1) {
        return {};
    }

    QByteArray sealed = nonce;
    sealed.resize(nonceLength + plaintext.size() + tagLength);
    unsigned char* out = bytesOf(sealed) + nonceLength;
    int written = 0;
    if (!plaintext.isEmpty()) {
        if (EVP_EncryptUpdate(context.get(), out, &length, bytesOf(plaintext),
                              static_cast<int>(plaintext.size())) != 1) {
            return {};
        }
        written = length;
    }
    if (EVP_EncryptFinal_ex(context.get(), out + written, &length) != 1) {
        return {};
    }
    written += length;
    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_GET_TAG, tagLength, out + written) != 1) {
        return {};
    }
    sealed.resize(nonceLength + written + tagLength);
    return sealed;
}

std::optional<QByteArray> open(const QByteArray& key, const QByteArray& sealed, const QByteArray& associatedData)
{
    if (key.size() != keyLength || sealed.size() < nonceLength + tagLength) {
        return std::nullopt;
    }
    const QByteArray nonce = sealed.left(nonceLength);
    const QByteArray ciphertext = sealed.mid(nonceLength, sealed.size() - nonceLength - tagLength);
    QByteArray tag = sealed.right(tagLength);

    const std::unique_ptr<EVP_CIPHER_CTX, CipherContextDeleter> context(EVP_CIPHER_CTX_new());
    if (!context || EVP_DecryptInit_ex(context.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1 ||
        EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_IVLEN, nonceLength, nullptr) != 1 ||
        EVP_DecryptInit_ex(context.get(), nullptr, nullptr, bytesOf(key), bytesOf(nonce)) != 1) {
        return std::nullopt;
    }

    int length = 0;
    if (!associatedData.isEmpty() &&
        EVP_DecryptUpdate(context.get(), nullptr, &length, bytesOf(associatedData),
                          static_cast<int>(associatedData.size())) != 1) {
        return std::nullopt;
    }

    QByteArray plaintext(ciphertext.size(), Qt::Uninitialized);
    int written = 0;
    if (!ciphertext.isEmpty()) {
        if (EVP_DecryptUpdate(context.get(), bytesOf(plaintext), &length, bytesOf(ciphertext),
                              static_cast<int>(ciphertext.size())) != 1) {
            wipe(plaintext);
            return std::nullopt;
        }
        written = length;
    }
    if (EVP_CIPHER_CTX_ctrl(context.get(), EVP_CTRL_GCM_SET_TAG, tagLength, bytesOf(tag)) != 1 ||
        EVP_DecryptFinal_ex(context.get(), bytesOf(plaintext) + written, &length) != 1) {
        wipe(plaintext);
        return std::nullopt;
    }
    plaintext.resize(written + length);
    return plaintext;
}

void wipe(QByteArray& secret)
{
    if (!secret.isEmpty()) {
        OPENSSL_cleanse(secret.data(), static_cast<size_t>(secret.size()));
    }
    secret.clear();
}

} // namespace chrono_vault::crypto
