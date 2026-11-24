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

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>

#include <optional>

// --- Version ------------------------------------------------------------
// tray_icon.h 0.5.85

// --- Version history ----------------------------------------------------
// v0.5.85: Initial release (D-064)

namespace chrono_vault {

// Whether the system's tray is dark: on Windows the taskbar's own setting,
// elsewhere the desktop's light or dark; never Chrono Vault's own theme.
enum class TrayColour { Unknown, Light, Dark };
[[nodiscard]] TrayColour trayColour();
// The vault for that tray: the plain white one only where the tray is surely
// dark, the one with a dark edge, seen on both, everywhere else (D-064).
[[nodiscard]] QString trayIconFile(TrayColour colour);
// The colour a test says it is; nothing: the system's again.
void setTrayColourForTests(std::optional<TrayColour> colour);

// Says when the system may have changed its tray's colour.
class TrayColourWatch : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit TrayColourWatch(QObject* parent = nullptr);
    ~TrayColourWatch() override;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void changed();
};

} // namespace chrono_vault
