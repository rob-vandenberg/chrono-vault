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

#include "password_generator.h"

#include "crypto.h"

#include <QList>

#include <algorithm>
#include <optional>

// --- Version ------------------------------------------------------------
// password_generator.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

namespace chrono_vault {

namespace {

const QString uppercaseLetters = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
const QString lowercaseLetters = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
const QString digitCharacters = QStringLiteral("0123456789");
const QString symbolCharacters = QStringLiteral("!#$%&()*+,-./:;<=>?@[]^_{|}~");

// A number below the bound, every value equally likely: random bytes that
// would favour low numbers are thrown away and drawn again.
std::optional<int> uniformBelow(int bound)
{
    const quint32 limit = (0x100000000ULL / static_cast<quint32>(bound)) * static_cast<quint32>(bound);
    for (int attempt = 0; attempt < 1000; ++attempt) {
        const QByteArray bytes = crypto::randomBytes(4);
        if (bytes.size() != 4) {
            return std::nullopt;
        }
        const quint32 value = (static_cast<quint32>(static_cast<uchar>(bytes[0])) << 24) |
                              (static_cast<quint32>(static_cast<uchar>(bytes[1])) << 16) |
                              (static_cast<quint32>(static_cast<uchar>(bytes[2])) << 8) |
                              static_cast<quint32>(static_cast<uchar>(bytes[3]));
        if (limit == 0 || value < limit) {
            return static_cast<int>(value % static_cast<quint32>(bound));
        }
    }
    return std::nullopt;
}

} // namespace

QString generatePassword(const GeneratorOptions& options)
{
    QList<QString> kinds;
    if (options.uppercase) {
        kinds << uppercaseLetters;
    }
    if (options.lowercase) {
        kinds << lowercaseLetters;
    }
    if (options.digits) {
        kinds << digitCharacters;
    }
    if (options.symbols) {
        kinds << symbolCharacters;
    }
    if (kinds.isEmpty()) {
        kinds << lowercaseLetters;
    }
    const int length = std::clamp(options.length, shortestGeneratedPassword, longestGeneratedPassword);
    const QString all = kinds.join(QString());

    // Drawn again until every chosen kind is in it; with 4 or more characters
    // and at most 4 kinds that takes a few tries at most.
    for (int attempt = 0; attempt < 1000; ++attempt) {
        QString password;
        password.reserve(length);
        for (int index = 0; index < length; ++index) {
            const auto pick = uniformBelow(static_cast<int>(all.size()));
            if (!pick) {
                return {};
            }
            password.append(all.at(*pick));
        }
        const bool hasEveryKind = std::all_of(kinds.cbegin(), kinds.cend(), [&password](const QString& kind) {
            return std::any_of(password.cbegin(), password.cend(),
                               [&kind](QChar character) { return kind.contains(character); });
        });
        if (hasEveryKind) {
            return password;
        }
    }
    return {};
}

} // namespace chrono_vault
