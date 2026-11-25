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

#include "vault_contents.h"

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QList>

#include <optional>

// --- Version ------------------------------------------------------------
// audit.h 0.6.86

// --- Version history ----------------------------------------------------
// v0.6.86: Initial release (D-065)

namespace chrono_vault {

// A website that lost its users' passwords, from Have I Been Pwned's list.
struct Breach
{
    QString title;
    QString domain;
    QDate date;
};

// One password as the audit finds it (D-065).
struct AuditedPassword
{
    QUuid itemId;
    qint64 fieldUid = 0;
    QByteArray sha1; // 40 capital hexadecimal digits
    double bits = 0;
    bool weak = false;
    bool old = false;
    QDateTime changed;     // when it last changed, if known
    QList<QUuid> sameIn;   // the other items with the same password
    int group = 0;         // 1, 2, 3, … for the passwords in more than one item
    int leaked = -1;       // how often it leaked; -1 when not checked
    QList<Breach> breaches; // of its website, since it last changed
};

// How long a password may stay as it is before it is old.
inline constexpr int oldAfterYears = 2;

// Every password of the vault but those in the Trash or the Archive, empty,
// deleted, or of an item left out of the audit.
[[nodiscard]] QList<AuditedPassword> auditPasswords(const VaultContents& contents, const QHash<QByteArray, int>& leaks,
                                                    const QList<Breach>& breaches, const QDateTime& now);

// What Have I Been Pwned answers about a hash's first five digits: the other
// 35 and how often, for those with a number above 0 (padding has 0).
[[nodiscard]] QHash<QByteArray, int> readLeakAnswer(const QByteArray& answer);
// Its list of breached websites: only the verified ones that lost passwords.
// Nothing when the answer is not that list.
[[nodiscard]] std::optional<QList<Breach>> readBreaches(const QByteArray& answer);

} // namespace chrono_vault
