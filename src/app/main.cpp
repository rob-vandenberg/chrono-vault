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

#include "version.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QTranslator>
#include <QtQml/QQmlExtensionPlugin>

// --- Version ------------------------------------------------------------
// main.cpp 0.0.3

// --- Version history ----------------------------------------------------
// v0.0.3: The organisation name, so the settings have a place of their own
// v0.0.2: The translation of the system's language is loaded at start (D-008)
// v0.0.1: Initial release

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("Chrono Vault"));
    QGuiApplication::setApplicationName(QStringLiteral("Chrono Vault"));
    QGuiApplication::setApplicationVersion(chrono_vault::programVersion());

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

    return QGuiApplication::exec();
}
