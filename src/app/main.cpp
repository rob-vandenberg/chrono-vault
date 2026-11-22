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

#include "app_setup.h"
#include "quick_unlock.h"
#include "appearance.h"
#include "settings_files.h"
#include "single_instance.h"
#include "system_tray.h"
#include "updater.h"
#include "vault_controller.h"
#include "version.h"

#include <QApplication>

#include <cstdio>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTranslator>
#include <QtQml/QQmlExtensionPlugin>

// --- Version ------------------------------------------------------------
// main.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: --reset deletes chrono-vault.ini; no carry-over (D-026)
// v0.0.30: The identifier chrono-vault for the settings, carried over; --debug writes the fingerprint log (D-044)
// v0.0.12: --reset: the settings not read at the start (D-026)
// v0.0.7: The kept theme and UI scaling put in use at the start (D-023)
// v0.0.5: The automatic update checks; --check-for-update for the release checks (D-021)
// v0.0.4: QApplication and the tray, Chrono Vault's own style, fonts and icon, start minimized
// v0.0.3: The organisation name, so the settings have a place of their own
// v0.0.2: The translation of the system's language is loaded at start (D-008)
// v0.0.1: Initial release

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    // The window decides when the program ends (Main.qml): closing it may keep
    // Chrono Vault running in the tray (D-019).
    QApplication::setQuitOnLastWindowClosed(false);
    // The identifier, not the name people read (D-044).
    QGuiApplication::setOrganizationName(QStringLiteral("chrono-vault"));
    QGuiApplication::setApplicationName(QStringLiteral("chrono-vault"));
    // --reset: chrono-vault.ini is deleted before anything reads it; the
    // vaults in chrono-vault.conf stay (D-026).
    if (QCoreApplication::arguments().contains(QStringLiteral("--reset"))) {
        chrono_vault::deleteSettingsFile();
    }
    // --debug: the fingerprint's steps are written to a log (the owner).
    chrono_vault::setFingerLogOn(QCoreApplication::arguments().contains(QStringLiteral("--debug")));
    QGuiApplication::setApplicationVersion(chrono_vault::programVersion());
    QGuiApplication::setWindowIcon(chrono_vault::programIcon());
    chrono_vault::applyStyle();
    // The kept theme and UI scaling, before any window shows (D-023).
    chrono_vault::Appearance::applyAtStart();

    // The packages' own check: one look on GitHub, its answer printed, then
    // the program ends (build-release.sh tries HTTPS with it, D-021).
    if (QCoreApplication::arguments().contains(QStringLiteral("--check-for-update"))) {
        chrono_vault::Updater updater;
        QObject::connect(&updater, &chrono_vault::Updater::stateChanged, &application, [&updater]() {
            using State = chrono_vault::Updater::State;
            if (updater.state() != State::Checking) {
                std::printf("%s\n", qPrintable(updater.message()));
                QCoreApplication::exit(updater.state() == State::Unreachable ? 1 : 0);
            }
        });
        updater.checkNow();
        return updater.state() == chrono_vault::Updater::State::Checking ? QApplication::exec() : 0;
    }

    // One Chrono Vault per person: a later start brings the running one to
    // the front and ends (D-020).
    chrono_vault::SingleInstance instance;
    if (!instance.becomeTheOnlyOne()) {
        return 0;
    }

    // US English is written in the code; another language is used when a
    // translation of the system's language is carried inside the program.
    QTranslator translator;
    if (translator.load(QLocale(), QStringLiteral("chrono-vault"), QStringLiteral("_"),
                        QStringLiteral(":/i18n"))) {
        QCoreApplication::installTranslator(&translator);
    }

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("ChronoVault", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    auto* controller = engine.singletonInstance<chrono_vault::VaultController*>("ChronoVault", "VaultController");
    const bool trayAvailable = chrono_vault::SystemTray::isAvailable();
    controller->setTrayAvailable(trayAvailable);
    chrono_vault::SystemTray tray(controller, window);
    QObject::connect(&instance, &chrono_vault::SingleInstance::showRequested, &tray,
                     &chrono_vault::SystemTray::showWindow);
    chrono_vault::showMainWindow(window, controller->startMinimized(), trayAvailable);
    engine.singletonInstance<chrono_vault::Updater*>("ChronoVault", "Updater")->startAutomaticChecks();

    return QGuiApplication::exec();
}
