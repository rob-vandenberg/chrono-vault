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

#include <QLocalServer>
#include <QObject>
#include <QString>

// --- Version ------------------------------------------------------------
// single_instance.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

namespace chrono_vault {

// One Chrono Vault per person (D-020): the first start listens on a local
// connection only that person can use; a later start finds it, asks it to
// come to the front, and ends itself.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString& name = defaultName(), QObject* parent = nullptr);

    // True when this is the only Chrono Vault running: it then listens for
    // later starts. False when another one runs: it has been asked to come
    // to the front, and this start should end.
    [[nodiscard]] bool becomeTheOnlyOne();

    [[nodiscard]] static QString defaultName();

signals:
    // A later start asked this one to come to the front.
    void showRequested();

private:
    QString name;
    QLocalServer* server = nullptr; // a child, so it moves with this object to another thread
};

} // namespace chrono_vault
