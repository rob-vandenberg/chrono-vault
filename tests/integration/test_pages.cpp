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
#include "updater.h"
#include "vault_controller.h"

#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_pages.cpp 0.0.5

// --- Version history ----------------------------------------------------
// v0.0.5: A quit for an update waits for the editor and goes on, or is abandoned by Cancel
// v0.0.4: Chrono Vault's own style; the window shown as the program shows it
// v0.0.3: Initial release, with unsaved changes asked about and kept through a lock

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

// The pages themselves, driven as a person would: a new login is made in
// the editor, given a field of its own, saved and closed, and the vault is
// locked, all without a single QML error.
class TestPages : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void a_login_made_in_the_editor_is_saved_without_qml_errors();
    void a_quit_for_an_update_waits_for_the_editor_and_goes_on();
    void unsaved_changes_are_asked_about_and_kept_through_a_lock();
};

void TestPages::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_pages"));
    applyStyle();
    QSettings().clear();
}

void TestPages::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestPages::a_login_made_in_the_editor_is_saved_without_qml_errors()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);

    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    QVERIFY(controller != nullptr);
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("pages.chronovault"))),
                            QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    QObject* vaultPage = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QVERIFY(vaultPage != nullptr);
    QVERIFY(editor != nullptr);

    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newLogin"));
    QVERIFY(!editor->property("itemId").toString().isEmpty());
    QVERIFY(editor->property("isNew").toBool());

    QVERIFY(QMetaObject::invokeMethod(editor, "addField", Q_ARG(QVariant, QStringLiteral("password"))));
    QVERIFY(QMetaObject::invokeMethod(editor, "store"));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->items()->rowCount(), 1);
    QVERIFY(!editor->property("isNew").toBool());

    const QVariantMap stored = controller->item(editor->property("itemId").toString());
    QCOMPARE(stored.value(QStringLiteral("fields")).toList().size(), 6);

    QVERIFY(QMetaObject::invokeMethod(editor, "close"));
    controller->lock();
    QCOMPARE(controller->state(), State::Locked);

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestPages::unsaved_changes_are_asked_about_and_kept_through_a_lock()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());

    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("asking.chronovault"))),
                            QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    QObject* vaultPage = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QObject* unsavedDialog = window->findChild<QObject*>(QStringLiteral("unsavedDialog"));
    QVERIFY(vaultPage && editor && unsavedDialog);

    // A new login nobody typed in is not a change: nothing is asked.
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newLogin"));
    QVERIFY(!editor->property("changed").toBool());
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newLogin"));
    QVERIFY(!unsavedDialog->property("opened").toBool());

    // Once typed in, making a note asks first.
    const QString loginId = editor->property("itemId").toString();
    editor->setProperty("changed", true);
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newSecureNote"));
    QTRY_VERIFY(unsavedDialog->property("opened").toBool());
    QCOMPARE(editor->property("itemId").toString(), loginId);

    // Save: the login is stored, then the note is opened.
    QVERIFY(QMetaObject::invokeMethod(unsavedDialog, "accept"));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->items()->rowCount(), 1);
    QVERIFY(editor->property("isNote").toBool());

    // Closing the window while the note is being typed asks first.
    editor->setProperty("changed", true);
    QVERIFY(QMetaObject::invokeMethod(window, "close"));
    QTRY_VERIFY(unsavedDialog->property("opened").toBool());
    QVERIFY(window->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(unsavedDialog, "reject"));
    QTRY_VERIFY(!unsavedDialog->property("opened").toBool());

    // Locking while the note is being typed keeps it.
    controller->lock();
    controller->unlock(QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->items()->rowCount(), 2);

    // An untouched new login is not kept by a lock: no blank items.
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newLogin"));
    controller->lock();
    controller->unlock(QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->items()->rowCount(), 2);

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestPages::a_quit_for_an_update_waits_for_the_editor_and_goes_on()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    auto* updater = engine.singletonInstance<Updater*>("ChronoVault", "Updater");
    QVERIFY(controller && updater);
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("update.chronovault"))),
                            QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    QObject* vaultPage = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QObject* unsavedDialog = window->findChild<QObject*>(QStringLiteral("unsavedDialog"));
    QVERIFY(vaultPage && editor && unsavedDialog);
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newLogin"));
    editor->setProperty("changed", true);

    // The update is installed and asks to quit while a login is being typed:
    // the quit asks about the login first, and the update stays armed.
    updater->armRestartForTests();
    QSignalSpy quit(&engine, &QQmlEngine::quit);
    emit updater->quitRequested();
    QTRY_VERIFY(unsavedDialog->property("opened").toBool());
    QVERIFY(updater->armedForTests());

    // Saved: the quit goes on, with the update still armed.
    QVERIFY(QMetaObject::invokeMethod(unsavedDialog, "accept"));
    QTRY_COMPARE(quit.count(), 1);
    QVERIFY(updater->armedForTests());
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->items()->rowCount(), 1);
    updater->quitHeldUp();

    // Cancel abandons the quit: nothing quits, and the update is not set off
    // at a later, ordinary quit.
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newLogin"));
    editor->setProperty("changed", true);
    updater->armRestartForTests();
    emit updater->quitRequested();
    QTRY_VERIFY(unsavedDialog->property("opened").toBool());
    QVERIFY(QMetaObject::invokeMethod(unsavedDialog, "reject"));
    QTRY_VERIFY(!unsavedDialog->property("opened").toBool());
    QVERIFY(!updater->armedForTests());
    QCOMPARE(quit.count(), 1);
    QVERIFY(window->property("visible").toBool());
}

QTEST_MAIN(TestPages)
#include "test_pages.moc"
