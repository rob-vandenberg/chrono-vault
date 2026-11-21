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

#include "totp.h"

#include <QMessageAuthenticationCode>
#include <QUrl>
#include <QUrlQuery>
#include <QtEndian>

// --- Version ------------------------------------------------------------
// totp.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

namespace chrono_vault {

namespace {

constexpr int longestSecretCharacters = 1024;

std::optional<QByteArray> decodeBase32(const QString& text)
{
    QByteArray bytes;
    quint32 buffer = 0;
    int bitsInBuffer = 0;
    int characters = 0;
    for (const QChar character : text) {
        if (character.isSpace() || character == QLatin1Char('-') || character == QLatin1Char('=')) {
            continue;
        }
        if (++characters > longestSecretCharacters) {
            return std::nullopt;
        }
        const char letter = character.toUpper().toLatin1();
        int value = -1;
        if (letter >= 'A' && letter <= 'Z') {
            value = letter - 'A';
        } else if (letter >= '2' && letter <= '7') {
            value = letter - '2' + 26;
        }
        if (value < 0) {
            return std::nullopt;
        }
        buffer = (buffer << 5) | static_cast<quint32>(value);
        bitsInBuffer += 5;
        if (bitsInBuffer >= 8) {
            bitsInBuffer -= 8;
            bytes.append(static_cast<char>((buffer >> bitsInBuffer) & 0xFF));
        }
    }
    if (bytes.isEmpty()) {
        return std::nullopt;
    }
    return bytes;
}

QCryptographicHash::Algorithm hashOf(TotpAlgorithm algorithm)
{
    switch (algorithm) {
    case TotpAlgorithm::Sha256: return QCryptographicHash::Sha256;
    case TotpAlgorithm::Sha512: return QCryptographicHash::Sha512;
    case TotpAlgorithm::Sha1: break;
    }
    return QCryptographicHash::Sha1;
}

} // namespace

std::optional<TotpSettings> parseTotp(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return std::nullopt;
    }
    if (!trimmed.startsWith(QLatin1String("otpauth://"), Qt::CaseInsensitive)) {
        const auto secret = decodeBase32(trimmed);
        if (!secret) {
            return std::nullopt;
        }
        return TotpSettings{*secret, 6, 30, TotpAlgorithm::Sha1};
    }

    const QUrl address(trimmed);
    if (!address.isValid() || address.host().compare(QLatin1String("totp"), Qt::CaseInsensitive) != 0) {
        return std::nullopt;
    }
    const QUrlQuery query(address);
    const auto secret = decodeBase32(query.queryItemValue(QStringLiteral("secret")));
    if (!secret) {
        return std::nullopt;
    }
    TotpSettings settings{*secret, 6, 30, TotpAlgorithm::Sha1};
    if (query.hasQueryItem(QStringLiteral("digits"))) {
        bool ok = false;
        const int digits = query.queryItemValue(QStringLiteral("digits")).toInt(&ok);
        if (!ok || digits < 6 || digits > 8) {
            return std::nullopt;
        }
        settings.digits = digits;
    }
    if (query.hasQueryItem(QStringLiteral("period"))) {
        bool ok = false;
        const int period = query.queryItemValue(QStringLiteral("period")).toInt(&ok);
        if (!ok || period < 1 || period > 300) {
            return std::nullopt;
        }
        settings.period = period;
    }
    if (query.hasQueryItem(QStringLiteral("algorithm"))) {
        const QString algorithm = query.queryItemValue(QStringLiteral("algorithm")).toUpper();
        if (algorithm == QLatin1String("SHA1")) {
            settings.algorithm = TotpAlgorithm::Sha1;
        } else if (algorithm == QLatin1String("SHA256")) {
            settings.algorithm = TotpAlgorithm::Sha256;
        } else if (algorithm == QLatin1String("SHA512")) {
            settings.algorithm = TotpAlgorithm::Sha512;
        } else {
            return std::nullopt;
        }
    }
    return settings;
}

QString totpCode(const TotpSettings& settings, qint64 unixSeconds)
{
    if (settings.secret.isEmpty() || settings.period <= 0 || unixSeconds < 0) {
        return {};
    }
    const quint64 counter = static_cast<quint64>(unixSeconds / settings.period);
    QByteArray message(8, '\0');
    qToBigEndian<quint64>(counter, message.data());
    const QByteArray hash = QMessageAuthenticationCode::hash(message, settings.secret, hashOf(settings.algorithm));
    const int offset = hash.at(hash.size() - 1) & 0x0F;
    const quint32 binary = ((static_cast<quint32>(static_cast<uchar>(hash.at(offset))) & 0x7F) << 24) |
                           (static_cast<quint32>(static_cast<uchar>(hash.at(offset + 1))) << 16) |
                           (static_cast<quint32>(static_cast<uchar>(hash.at(offset + 2))) << 8) |
                           static_cast<quint32>(static_cast<uchar>(hash.at(offset + 3)));
    quint32 modulus = 1;
    for (int digit = 0; digit < settings.digits; ++digit) {
        modulus *= 10;
    }
    return QStringLiteral("%1").arg(binary % modulus, settings.digits, 10, QLatin1Char('0'));
}

int totpSecondsLeft(const TotpSettings& settings, qint64 unixSeconds)
{
    if (settings.period <= 0 || unixSeconds < 0) {
        return 0;
    }
    return settings.period - static_cast<int>(unixSeconds % settings.period);
}

} // namespace chrono_vault
