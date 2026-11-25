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

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// --- Version ------------------------------------------------------------
// scaling.h 0.6.92

// --- Version history ----------------------------------------------------
// v0.6.92: Enlarged by exactly the UI scaling (D-069)
// v0.0.10: Initial release (D-023)

namespace chrono_vault {

// The UI scaling as one factor (1.0 is the desktop's own size), for the
// controls of Chrono Vault's style and for every size Chrono Vault's windows
// set themselves: text, icons, tick boxes, spacing and fixed sizes grow and
// shrink together, as in Chrono Mail (D-023).
class UiScale : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(double factor READ factor NOTIFY factorChanged)

public:
    // The one instance, shared by every QML engine and by the program.
    [[nodiscard]] static UiScale* instance();
    static UiScale* create(QQmlEngine* engine, QJSEngine* script);

    [[nodiscard]] double factor() const { return currentFactor; }
    void setFactor(double factor);

    // A size in pixels at the factor now; never below 1.
    [[nodiscard]] Q_INVOKABLE int px(double pixels) const;

    // A size rounded up to what it is in whole pixels at 100%, as Qt's layouts
    // do there, at the factor now: a layout is the one of 100%, enlarged (D-069).
    Q_INVOKABLE static double whole(double pixels);

signals:
    void factorChanged();

private:
    explicit UiScale(QObject* parent = nullptr) : QObject(parent) {}
    double currentFactor = 1.0;
};

} // namespace chrono_vault
