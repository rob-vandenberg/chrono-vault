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

#include "audit.h"

#include "password_strength.h"
#include "templates.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

// --- Version ------------------------------------------------------------
// audit.cpp 0.6.86

// --- Version history ----------------------------------------------------
// v0.6.86: Initial release (D-065)

namespace chrono_vault {

QList<AuditedPassword> auditPasswords(const VaultContents& contents, const QHash<QByteArray, int>& leaks,
                                      const QList<Breach>& breaches, const QDateTime& now)
{
    QList<AuditedPassword> found;
    QList<QString> values; // beside found
    QHash<QString, QList<qsizetype>> byValue;
    for (const Item& item : contents.items) {
        if (item.trashed || item.archived || item.noAudit) {
            continue;
        }
        const QString host = websiteHostOf(item);
        QStringList personal{item.title, host};
        for (const Field& field : item.fields) {
            if (!field.deleted && (field.kind == FieldKind::Username || field.kind == FieldKind::Email
                                   || field.role == QLatin1String("username"))) {
                personal.append(field.value);
            }
        }
        for (const Field& field : item.fields) {
            if (field.kind != FieldKind::Password || field.deleted || field.value.isEmpty()) {
                continue;
            }
            AuditedPassword one;
            one.itemId = item.id;
            one.fieldUid = field.uid;
            one.sha1 = QCryptographicHash::hash(field.value.toUtf8(), QCryptographicHash::Sha1).toHex().toUpper();
            one.bits = passwordBits(field.value, personal);
            one.weak = one.bits < weakBelowBits;
            one.changed = field.changed;
            one.old = field.changed.isValid() && field.changed.addYears(oldAfterYears) <= now;
            one.leaked = leaks.value(one.sha1, -1);
            for (const Breach& breach : breaches) {
                if (host.isEmpty() || (host != breach.domain && !host.endsWith(QLatin1Char('.') + breach.domain))) {
                    continue;
                }
                // Unknown: not changed since, to be safe.
                if (!field.changed.isValid() || field.changed.date() <= breach.date) {
                    one.breaches.append(breach);
                }
            }
            byValue[field.value].append(found.size());
            values.append(field.value);
            found.append(one);
        }
    }
    int groups = 0;
    for (qsizetype index = 0; index < found.size(); ++index) {
        const QList<qsizetype> same = byValue.value(values.at(index));
        for (const qsizetype other : same) {
            const QUuid itsItem = found.at(other).itemId;
            if (itsItem != found.at(index).itemId && !found.at(index).sameIn.contains(itsItem)) {
                found[index].sameIn.append(itsItem);
            }
        }
        if (!found.at(index).sameIn.isEmpty() && found.at(index).group == 0) {
            ++groups;
            for (const qsizetype other : same) {
                found[other].group = groups;
            }
        }
    }
    return found;
}

QHash<QByteArray, int> readLeakAnswer(const QByteArray& answer)
{
    QHash<QByteArray, int> found;
    for (const QByteArray& line : answer.split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon != 35) {
            continue;
        }
        bool isNumber = false;
        const int count = line.mid(colon + 1).trimmed().toInt(&isNumber);
        if (isNumber && count > 0) {
            found.insert(line.left(colon).toUpper(), count);
        }
    }
    return found;
}

std::optional<QList<Breach>> readBreaches(const QByteArray& answer)
{
    const QJsonDocument document = QJsonDocument::fromJson(answer);
    if (!document.isArray()) {
        return std::nullopt;
    }
    QList<Breach> found;
    for (const QJsonValue& value : document.array()) {
        const QJsonObject object = value.toObject();
        Breach one;
        one.title = object.value(QStringLiteral("Title")).toString();
        one.domain = object.value(QStringLiteral("Domain")).toString().toLower();
        one.date = QDate::fromString(object.value(QStringLiteral("BreachDate")).toString(), Qt::ISODate);
        const bool lostPasswords = object.value(QStringLiteral("DataClasses")).toArray().contains(QStringLiteral("Passwords"));
        if (lostPasswords && object.value(QStringLiteral("IsVerified")).toBool()
            && !object.value(QStringLiteral("IsFabricated")).toBool() && !object.value(QStringLiteral("IsSpamList")).toBool()
            && one.domain.contains(QLatin1Char('.')) && one.date.isValid()) {
            found.append(one);
        }
    }
    return found;
}

} // namespace chrono_vault
