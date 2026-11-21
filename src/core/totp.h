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
#include <QString>

#include <optional>

// --- Version ------------------------------------------------------------
// totp.h 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

// One-time codes (TOTP, RFC 6238), as login pages ask for after the password.
namespace chrono_vault {

enum class TotpAlgorithm { Sha1, Sha256, Sha512 };

struct TotpSettings
{
    QByteArray secret;
    int digits = 6;
    int period = 30;
    TotpAlgorithm algorithm = TotpAlgorithm::Sha1;
};

// Reads what a site gives: either the secret in base32 (spaces and case do
// not matter), or an otpauth://totp/... address. Nothing when it is neither.
[[nodiscard]] std::optional<TotpSettings> parseTotp(const QString& text);

// The code at the given moment, in seconds since 1970 (UTC).
[[nodiscard]] QString totpCode(const TotpSettings& settings, qint64 unixSeconds);

// How many seconds the code of that moment stays valid.
[[nodiscard]] int totpSecondsLeft(const TotpSettings& settings, qint64 unixSeconds);

} // namespace chrono_vault
