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

#include "system_tray.h"

#include "vault_controller.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWindow>

// --- Version ------------------------------------------------------------
// system_tray.cpp 0.8.124

// --- Version history ----------------------------------------------------
// v0.8.124: The icon only while Keep Chrono Vault running in the system tray is on (D-019)
// v0.6.106: Lock while the key store is open (D-075)
// v0.6.96: One tray icon, the owner's, on every tray (D-064)
// v0.5.85: The vault as its icon, for a dark or a light tray (D-064)
// v0.0.4: Initial release

namespace chrono_vault {

SystemTray::SystemTray(VaultController* vaultController, QWindow* mainWindow, QObject* parent)
    : QObject(parent), controller(vaultController), window(mainWindow)
{
    menu = new QMenu;
    show = menu->addAction(tr("Show Chrono Vault"));
    lock = menu->addAction(tr("Lock"));
    menu->addSeparator();
    leave = menu->addAction(tr("Quit"));
    connect(show, &QAction::triggered, this, &SystemTray::showWindow);
    connect(lock, &QAction::triggered, this, &SystemTray::lockVault);
    connect(leave, &QAction::triggered, this, &SystemTray::quit);
    if (controller) {
        connect(controller, &VaultController::stateChanged, this, &SystemTray::updateLockAction);
        connect(controller, &VaultController::keptChanged, this, &SystemTray::updateLockAction);
    }
    updateLockAction();

    if (isAvailable()) {
        icon = new QSystemTrayIcon(this);
        icon->setToolTip(tr("Chrono Vault %1").arg(controller ? controller->version() : QString()));
        icon->setContextMenu(menu);
        connect(icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                showWindow();
            }
        });
    }
    if (icon) {
        icon->setIcon(QIcon(iconFile()));
    }
    // Only while "Keep Chrono Vault running in the system tray" is on (the
    // owner, D-019), following it at once.
    if (icon) {
        icon->setVisible(!controller || controller->keepInTray());
        if (controller) {
            connect(controller, &VaultController::keepInTrayChanged, icon,
                    [this]() { icon->setVisible(controller && controller->keepInTray()); });
        }
    }
}

bool SystemTray::iconShown() const
{
    return icon && icon->isVisible();
}

SystemTray::~SystemTray()
{
    delete menu;
}

bool SystemTray::isAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void SystemTray::showWindow()
{
    if (!window) {
        return;
    }
    window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
    window->show();
    window->raise();
    window->requestActivate();
}

void SystemTray::lockVault()
{
    if (controller) {
        controller->lock();
    }
}

void SystemTray::quit()
{
    if (window) {
        // The window asks about unsaved work first, as for every quit (D-016).
        QMetaObject::invokeMethod(window, "requestQuit");
    }
}

void SystemTray::updateLockAction()
{
    lock->setEnabled(controller && (controller->state() == VaultController::State::Unlocked || controller->keyStoreOpen()));
}

} // namespace chrono_vault
