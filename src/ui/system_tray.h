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
#include <QPointer>

class QAction;
class QMenu;
class QSystemTrayIcon;
class QWindow;

// --- Version ------------------------------------------------------------
// system_tray.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

namespace chrono_vault {

class VaultController;

// Chrono Vault in the system tray (D-019): its icon, a click that shows the
// window, and a menu with Show, Lock and Quit.
class SystemTray : public QObject
{
    Q_OBJECT

public:
    SystemTray(VaultController* controller, QWindow* window, QObject* parent = nullptr);
    ~SystemTray() override;

    [[nodiscard]] static bool isAvailable();

    // What the menu's entries do; tests call them directly.
    void showWindow();
    void lockVault();
    void quit();

    [[nodiscard]] QAction* lockAction() const { return lock; }

private:
    void updateLockAction();

    QPointer<VaultController> controller;
    QPointer<QWindow> window;
    QSystemTrayIcon* icon = nullptr;
    QMenu* menu = nullptr;
    QAction* show = nullptr;
    QAction* lock = nullptr;
    QAction* leave = nullptr;
};

} // namespace chrono_vault
