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

#include "app_setup.h"
#include "vault_controller.h"

#include <QAction>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QWindow>

// --- Version ------------------------------------------------------------
// system_tray.cpp 0.0.4

// --- Version history ----------------------------------------------------
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
    }
    updateLockAction();

    if (isAvailable()) {
        icon = new QSystemTrayIcon(programIcon(), this);
        icon->setToolTip(tr("Chrono Vault %1").arg(controller ? controller->version() : QString()));
        icon->setContextMenu(menu);
        connect(icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
                showWindow();
            }
        });
        icon->show();
    }
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
    lock->setEnabled(controller && controller->state() == VaultController::State::Unlocked);
}

} // namespace chrono_vault
