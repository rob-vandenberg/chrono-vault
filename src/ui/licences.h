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

#include <QList>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// --- Version ------------------------------------------------------------
// licences.h 0.0.8

// --- Version history ----------------------------------------------------
// v0.0.8: Initial release (D-021)

namespace chrono_vault {

// One licence the About box shows: what it covers, and its text (empty for a
// part shipped under terms that come without a text).
struct Licence
{
    QString covers;
    QString text;
};

// Every licence Chrono Vault ships, our own AGPL-3.0 first (D-021). The texts
// are carried inside the program.
[[nodiscard]] QList<Licence> shippedLicences();

// The licences as the About box shows them: each with a line naming what it
// covers, a horizontal bar between them.
class Licences : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString text READ text CONSTANT)

public:
    explicit Licences(QObject* parent = nullptr) : QObject(parent) {}
    [[nodiscard]] static QString text();
};

} // namespace chrono_vault
