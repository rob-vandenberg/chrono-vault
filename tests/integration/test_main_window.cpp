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
#include "version.h"

#include <QFont>
#include <QList>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_main_window.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Chrono Vault's own style and fonts; the version in the title
// v0.0.3: Settings of the test's own, never the person's
// v0.0.1: Initial release

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void the_main_window_loads_without_qml_errors_and_is_titled_chrono_vault();
};

void TestMainWindow::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_main_window"));
    chrono_vault::applyStyle();
}

void TestMainWindow::the_main_window_loads_without_qml_errors_and_is_titled_chrono_vault()
{
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });

    engine.loadFromModule("ChronoVault", "Main");

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QCOMPARE(window->title(), QStringLiteral("Chrono Vault ") + chrono_vault::programVersion());

    // The Noto fonts, carried inside the program (D-017).
    QCOMPARE(QGuiApplication::font().family(), QStringLiteral("Noto Sans"));
    auto* masterPassword = window->findChild<QObject*>(QStringLiteral("masterPasswordField"));
    QVERIFY(masterPassword != nullptr);
    QCOMPARE(masterPassword->property("font").value<QFont>().family(), QStringLiteral("Noto Sans Mono"));

    // Leave nothing behind (OR-11): the window goes with the engine.
    window->close();
}

QTEST_MAIN(TestMainWindow)
#include "test_main_window.moc"
