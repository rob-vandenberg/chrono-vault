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
#include "item_list_model.h"
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
#include <QQuickItem>
#include <QFile>
#include <QQuickWindow>
#include <QSignalSpy>

// --- Version ------------------------------------------------------------
// test_pages.cpp 0.0.19

// --- Version history ----------------------------------------------------
// v0.0.19: The import window: Import, and closed by a lock (D-035)
// v0.0.18: The editor: Field Options, Add > Section, a new field closed without Save, moving, Cancel (D-034)
// v0.0.17: The item view: a card per section, hidden values as dots, the Trash (D-033)
// v0.0.16: New Item shows its keyboard focus (P12)
// v0.0.15: The vault chooser, the keyfile on the unlock page, the master password window (D-030, D-031)
// v0.0.14: New Item's categories, a tag typed in the editor, the left column, the star, Settings > Customize and the
//          template editor (D-027, D-028)
// v0.0.9: One bar with the menus and the buttons; three columns; the item read-only, the pencil opening the editor
// v0.0.5: A quit for an update waits for the editor and goes on, or is abandoned by Cancel
// v0.0.4: Chrono Vault's own style; the window shown as the program shows it
// v0.0.3: Initial release, with unsaved changes asked about and kept through a lock

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

// An item a Repeater made, which is in the tree of items but not of objects.
QQuickItem* visualItem(QQuickItem* root, const QString& name)
{
    if (!root) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = visualItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

// Every item a Repeater made with this name, in the tree of items.
QList<QQuickItem*> visualItems(QQuickItem* root, const QString& name)
{
    QList<QQuickItem*> found;
    if (!root) {
        return found;
    }
    if (root->objectName() == name) {
        found << root;
    }
    for (QQuickItem* child : root->childItems()) {
        found << visualItems(child, name);
    }
    return found;
}

} // namespace

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
    void the_main_window_has_one_bar_and_three_columns();
    void a_quit_for_an_update_waits_for_the_editor_and_goes_on();
    void unsaved_changes_are_asked_about_and_kept_through_a_lock();
    void categories_tags_favourites_and_templates_work_from_the_windows();
    void a_file_still_being_read_is_never_lost_by_leaving_the_editor();
    void vaults_are_chosen_and_the_master_password_changed_from_the_windows();
    void new_item_shows_when_it_has_the_keyboard_focus();
    void an_item_shows_a_card_per_section_and_hidden_values_as_dots();
    void the_editor_changes_fields_through_field_options();
    void the_import_window_offers_import_and_closes_at_a_lock();
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

void TestPages::the_main_window_has_one_bar_and_three_columns()
{
    QTemporaryDir folder;
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});

    // The bar: the five menus on the left, the buttons on the right, on one line.
    QObject* menuBar = window->findChild<QObject*>(QStringLiteral("menuBar"));
    auto* newItem = qobject_cast<QQuickItem*>(window->findChild<QObject*>(QStringLiteral("newItemButton")));
    auto* preferences = qobject_cast<QQuickItem*>(window->findChild<QObject*>(QStringLiteral("preferencesButton")));
    QVERIFY(menuBar && newItem && preferences);
    QCOMPARE(menuBar->property("count").toInt(), 5);
    auto* bar = qobject_cast<QQuickItem*>(menuBar);
    QVERIFY(bar);
    QTRY_VERIFY(newItem->width() > 0);
    QVERIFY(bar->mapToScene(QPointF(0, 0)).x() < newItem->mapToScene(QPointF(0, 0)).x());
    QVERIFY(qAbs(bar->mapToScene(QPointF(0, bar->height() / 2)).y()
                 - newItem->mapToScene(QPointF(0, newItem->height() / 2)).y()) <= 4);
    // Without an unlocked vault, New Item is greyed (never hidden).
    QVERIFY(newItem->isVisible() && !newItem->isEnabled());

    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("columns.chronovault"))),
                            QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QVERIFY(newItem->isEnabled());
    QVariantMap item = controller->newLogin();
    item[QStringLiteral("title")] = QStringLiteral("Bakery orders");
    QVERIFY(controller->saveItem(item));
    QTRY_VERIFY(controller->isIdle());
    const QString id = item[QStringLiteral("id")].toString();

    // Three columns: the vault, the items under their letter, the chosen item.
    QObject* vaultColumn = window->findChild<QObject*>(QStringLiteral("vaultColumn"));
    QObject* list = window->findChild<QObject*>(QStringLiteral("itemList"));
    QObject* page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* details = window->findChild<QObject*>(QStringLiteral("itemDetails"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QVERIFY(vaultColumn && list && page && details && editor);
    QCOMPARE(list->property("section").value<QObject*>()->property("property").toString(), QStringLiteral("initial"));
    QCOMPARE(controller->items()->data(controller->items()->index(0), ItemListModel::InitialRole).toString(),
             QStringLiteral("B"));

    // Chosen: shown read-only; the pencil opens the editor in the same column.
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id)));
    QTRY_VERIFY(details->property("visible").toBool());
    QObject* title = details->findChild<QObject*>(QStringLiteral("itemTitle"));
    QVERIFY(title);
    QCOMPARE(title->property("text").toString(), QStringLiteral("Bakery orders"));
    QVERIFY(!editor->property("visible").toBool());
    QObject* pencil = details->findChild<QObject*>(QStringLiteral("editButton"));
    QVERIFY(pencil && QMetaObject::invokeMethod(pencil, "clicked"));
    QTRY_VERIFY(editor->property("visible").toBool());
    QCOMPARE(editor->property("itemId").toString(), id);
    QVERIFY(QMetaObject::invokeMethod(editor, "close"));
    QTRY_VERIFY(details->property("visible").toBool());

    // Item > Move to Trash: once in the Trash, the item is no longer shown
    // here, and the Trash holds it (D-033).
    QVERIFY(QMetaObject::invokeMethod(page, "deleteChosen"));
    QVERIFY(!editor->property("visible").toBool()); // no editor, straight into the Trash
    QTRY_VERIFY(controller->isIdle());
    QTRY_COMPARE(page->property("chosenItemId").toString(), QString());
    QVERIFY(!details->property("visible").toBool());
    QCOMPARE(controller->items()->rowCount(), 0);
    controller->setFilter(QStringLiteral("trash"));
    QCOMPARE(controller->items()->rowCount(), 1);
    controller->setFilter(QString());
}

void TestPages::categories_tags_favourites_and_templates_work_from_the_windows()
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
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("customize.chronovault"))),
                            QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    // New Item offers every category with templates: eight.
    QObject* newItemMenu = window->findChild<QObject*>(QStringLiteral("newItemMenu"));
    QVERIFY(newItemMenu);
    QTRY_COMPARE(newItemMenu->property("count").toInt(), 8);

    // A credit card with a tag typed in the editor, not yet added with Enter.
    QObject* vaultPage = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QObject* tagField = window->findChild<QObject*>(QStringLiteral("tagField"));
    QVERIFY(vaultPage && editor && tagField);
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newItem", Q_ARG(QVariant, QStringLiteral("card"))));
    QCOMPARE(editor->property("categoryId").toString(), QStringLiteral("card"));
    tagField->setProperty("text", QStringLiteral(" Money / Cards "));
    QVERIFY(QMetaObject::invokeMethod(editor, "store"));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->tags().size(), 2);
    const QString itemId = editor->property("itemId").toString();
    QCOMPARE(controller->item(itemId).value(QStringLiteral("tags")).toStringList(),
             QStringList{QStringLiteral("Money/Cards")});
    QVERIFY(QMetaObject::invokeMethod(editor, "close"));

    // The left column narrows the list; the tag tree shows the nested tag.
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    QVERIFY(quickWindow);
    QTRY_VERIFY(visualItem(quickWindow->contentItem(), QStringLiteral("tagEntry_Money/Cards")) != nullptr);
    QVERIFY(visualItem(quickWindow->contentItem(), QStringLiteral("categoryEntry_card")) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "choose", Q_ARG(QVariant, QStringLiteral("category:login"))));
    QCOMPARE(controller->filter(), QStringLiteral("category:login"));
    QCOMPARE(controller->items()->rowCount(), 0);
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "choose", Q_ARG(QVariant, QStringLiteral("tag:Money"))));
    QCOMPARE(controller->items()->rowCount(), 1);

    // The star marks it a favourite.
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "showItem", Q_ARG(QVariant, itemId)));
    QObject* star = window->findChild<QObject*>(QStringLiteral("favouriteButton"));
    QVERIFY(star);
    QVERIFY(QMetaObject::invokeMethod(star, "clicked"));
    QVERIFY(controller->item(itemId).value(QStringLiteral("favourite")).toBool());

    // Settings > Customize: a new template, named, kept in the vault.
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QObject* topics = settings->findChild<QObject*>(QStringLiteral("topics"));
    QVERIFY(topics);
    topics->setProperty("currentIndex", 3);
    QObject* newTemplate = settings->findChild<QObject*>(QStringLiteral("newTemplateButton"));
    QObject* templateEditor = settings->findChild<QObject*>(QStringLiteral("templateEditor"));
    QVERIFY(newTemplate && templateEditor);
    QVERIFY(QMetaObject::invokeMethod(newTemplate, "clicked"));
    QTRY_VERIFY(templateEditor->property("opened").toBool());
    QObject* nameField = templateEditor->findChild<QObject*>(QStringLiteral("templateNameField"));
    QVERIFY(nameField);
    // Without a name it stays open, nothing typed lost.
    QVERIFY(QMetaObject::invokeMethod(templateEditor, "accept"));
    QTRY_VERIFY(templateEditor->property("opened").toBool());
    nameField->setProperty("text", QStringLiteral("Car"));
    QVERIFY(QMetaObject::invokeMethod(templateEditor, "accept"));
    QTRY_VERIFY(!templateEditor->property("opened").toBool());
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->templates().size(), 14);
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));

    controller->lock();
    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestPages::a_file_still_being_read_is_never_lost_by_leaving_the_editor()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    const QString source = folder.filePath(QStringLiteral("scan.txt"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("the scan");
    }

    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("reading.chronovault"))),
                            QStringLiteral("master"), QStringLiteral("master"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    QObject* vaultPage = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QObject* readingDialog = window->findChild<QObject*>(QStringLiteral("readingDialog"));
    QVERIFY(vaultPage && editor && readingDialog);
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newItem", Q_ARG(QVariant, QStringLiteral("passport"))));
    const QString itemId = editor->property("itemId").toString();

    // Leaving while the file is read is held up and said; the item stays open.
    QVERIFY(QMetaObject::invokeMethod(editor, "addAttachment", Q_ARG(QVariant, QUrl::fromLocalFile(source))));
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "newLogin"));
    QTRY_VERIFY(readingDialog->property("opened").toBool());
    QCOMPARE(editor->property("itemId").toString(), itemId);
    QVERIFY(QMetaObject::invokeMethod(readingDialog, "accept"));

    // Once read, it is there, and saved with the item.
    QTRY_COMPARE(editor->property("waitingFor").toList().size(), 0);
    QVERIFY(QMetaObject::invokeMethod(editor, "store"));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->item(itemId).value(QStringLiteral("attachments")).toList().size(), 1);

    controller->lock();
    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestPages::vaults_are_chosen_and_the_master_password_changed_from_the_windows()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    QSettings().clear();

    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    const QString home = folder.filePath(QStringLiteral("home.chronovault"));
    const QString work = folder.filePath(QStringLiteral("work.chronovault"));
    controller->createVault(QUrl::fromLocalFile(home), QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    controller->createVault(QUrl::fromLocalFile(work), QStringLiteral("b"), QStringLiteral("b"), QUrl(), true);
    QTRY_COMPARE(controller->vaultPath(), work);
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QTRY_VERIFY(controller->isIdle());

    // The vault chooser lists both vaults, then New vault and Open vault.
    QObject* vaultMenu = window->findChild<QObject*>(QStringLiteral("vaultMenu"));
    QVERIFY(vaultMenu);
    QTRY_COMPARE(vaultMenu->property("count").toInt(), 5);
    QObject* vaultPage = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QVERIFY(QMetaObject::invokeMethod(vaultPage, "switchVault", Q_ARG(QVariant, home)));
    QTRY_COMPARE(controller->vaultPath(), home);
    QTRY_COMPARE(controller->state(), State::Unlocked);

    // The master password window changes it.
    QObject* passwordWindow = window->findChild<QObject*>(QStringLiteral("masterPasswordWindow"));
    QVERIFY(passwordWindow);
    QVERIFY(QMetaObject::invokeMethod(passwordWindow, "ask"));
    QTRY_VERIFY(passwordWindow->property("opened").toBool());
    passwordWindow->findChild<QObject*>(QStringLiteral("currentPasswordField"))->setProperty("text", QStringLiteral("a"));
    passwordWindow->findChild<QObject*>(QStringLiteral("newPasswordField"))->setProperty("text", QStringLiteral("c"));
    passwordWindow->findChild<QObject*>(QStringLiteral("repeatedPasswordField"))->setProperty("text", QStringLiteral("c"));
    QVERIFY(passwordWindow->property("ready").toBool());
    QSignalSpy changed(controller, &VaultController::masterPasswordChanged);
    QVERIFY(QMetaObject::invokeMethod(passwordWindow, "accept"));
    QTRY_COMPARE(changed.size(), 1);
    QVERIFY(changed.first().at(0).toBool());

    // Locked: the unlock page has no keyfile row for a vault without one.
    controller->lock();
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    QQuickItem* keyfileLabel = visualItem(quickWindow->contentItem(), QStringLiteral("unlockKeyfileLabel"));
    QVERIFY(keyfileLabel);
    QVERIFY(!keyfileLabel->isVisible());
    controller->unlock(QStringLiteral("c"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    controller->lock();

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestPages::new_item_shows_when_it_has_the_keyboard_focus()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("focus.chronovault"))),
                            QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    auto* button = window->findChild<QQuickItem*>(QStringLiteral("newItemButton"));
    auto* fill = window->findChild<QQuickItem*>(QStringLiteral("newItemFill"));
    QVERIFY(button && fill);
    QCOMPARE(fill->property("border").value<QObject*>()->property("width").toInt(), 0);
    button->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(button->property("visualFocus").toBool());
    QVERIFY(fill->property("border").value<QObject*>()->property("width").toInt() >= 2);
    controller->lock();
}

void TestPages::an_item_shows_a_card_per_section_and_hidden_values_as_dots()
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
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("cards.chronovault"))),
                            QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    QVariantMap item = controller->newLogin();
    QVariantList fields{
        QVariantMap{{"role", ""}, {"label", "Website"}, {"kind", "url"}, {"value", "https://example.org"}},
        QVariantMap{{"role", ""}, {"label", "Password"}, {"kind", "text"}, {"value", "secret"}, {"sensitive", true}},
        QVariantMap{{"role", ""}, {"label", "2FA Authentication"}, {"kind", "section"}, {"value", ""}},
        QVariantMap{{"role", ""}, {"label", "Repository"}, {"kind", "url"}, {"value", "https://example.org/r"}},
        QVariantMap{{"role", ""}, {"label", "Empty part"}, {"kind", "section"}, {"value", ""}},
        QVariantMap{{"role", ""}, {"label", "Phone"}, {"kind", "phone"}, {"value", ""}},
        QVariantMap{{"role", ""}, {"label", "2FA"}, {"kind", "hidden"}, {"value", "x"}, {"hiddenType", ".twoFA"}},
    };
    item.insert(QStringLiteral("fields"), fields);
    item.insert(QStringLiteral("title"), QStringLiteral("Example"));
    const QString id = item.value(QStringLiteral("id")).toString();
    QVERIFY(controller->saveItem(item));
    QTRY_VERIFY(controller->isIdle());
    QObject* page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id)));

    auto* root = qobject_cast<QQuickWindow*>(window)->contentItem();
    // Two cards: the first without a heading, then the one section with a
    // field to show; the empty section, the empty phone and the hidden
    // field are not shown.
    QTRY_COMPARE(visualItems(root, QStringLiteral("fieldCard")).size(), 2);
    QStringList headings;
    for (QQuickItem* heading : visualItems(root, QStringLiteral("sectionHeading"))) {
        if (heading->isVisible()) {
            headings << heading->property("text").toString();
        }
    }
    QCOMPARE(headings, QStringList{QStringLiteral("2FA Authentication")});
    QStringList values;
    for (QQuickItem* value : visualItems(root, QStringLiteral("fieldValue"))) {
        if (value->isVisible()) {
            values << value->property("text").toString();
        }
    }
    QVERIFY(values.contains(QStringLiteral("••••••••")));
    QVERIFY(!values.contains(QStringLiteral("secret")));
    QCOMPARE(values.size(), 3);

    // Moved to the Trash and shown there: Restore brings it back.
    controller->moveToTrash(id, true);
    controller->setFilter(QStringLiteral("trash"));
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id)));
    QObject* details = window->findChild<QObject*>(QStringLiteral("itemDetails"));
    QObject* restore = details->findChild<QObject*>(QStringLiteral("restoreButton"));
    QVERIFY(restore);
    QTRY_VERIFY(restore->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(restore, "clicked"));
    QVERIFY(!controller->item(id).value(QStringLiteral("trashed")).toBool());
    controller->setFilter(QString());
    controller->lock();

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestPages::the_editor_changes_fields_through_field_options()
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
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("editor.chronovault"))),
                            QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);

    QObject* page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QObject* options = window->findChild<QObject*>(QStringLiteral("fieldOptions"));
    QVERIFY(page && editor && options);
    QVERIFY(QMetaObject::invokeMethod(page, "newItem", Q_ARG(QVariant, QStringLiteral("note"))));
    const QString id = editor->property("itemId").toString();

    // A field added and given a name, a type and the hide mark.
    QVariant index;
    QVERIFY(QMetaObject::invokeMethod(editor, "addField", Q_RETURN_ARG(QVariant, index), Q_ARG(QVariant, QStringLiteral("text"))));
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, index), Q_ARG(QVariant, true)));
    QTRY_VERIFY(options->property("opened").toBool());
    options->findChild<QObject*>(QStringLiteral("fieldNameField"))->setProperty("text", QStringLiteral("PIN code"));
    options->findChild<QObject*>(QStringLiteral("fieldTypeBox"))->setProperty("currentIndex", 7); // Number
    options->findChild<QObject*>(QStringLiteral("sensitiveSwitch"))->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(options, "accept"));

    // A section, and a field closed without Save, which is not added.
    QVERIFY(QMetaObject::invokeMethod(editor, "addField", Q_RETURN_ARG(QVariant, index), Q_ARG(QVariant, QStringLiteral("section"))));
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, index), Q_ARG(QVariant, true)));
    options->findChild<QObject*>(QStringLiteral("fieldNameField"))->setProperty("text", QStringLiteral("Bank"));
    QVERIFY(QMetaObject::invokeMethod(options, "accept"));
    QVERIFY(QMetaObject::invokeMethod(editor, "addField", Q_RETURN_ARG(QVariant, index), Q_ARG(QVariant, QStringLiteral("text"))));
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, index), Q_ARG(QVariant, true)));
    QVERIFY(QMetaObject::invokeMethod(options, "reject"));

    // The section moved to the top, then saved.
    QVERIFY(QMetaObject::invokeMethod(editor, "moveField", Q_ARG(QVariant, 2), Q_ARG(QVariant, -2)));
    QVERIFY(QMetaObject::invokeMethod(editor, "store"));
    QTRY_VERIFY(controller->isIdle());
    QStringList shape;
    for (const QVariant& field : controller->item(id).value(QStringLiteral("fields")).toList()) {
        const QVariantMap map = field.toMap();
        shape << map.value(QStringLiteral("kind")).toString() + QLatin1Char(':') + map.value(QStringLiteral("name")).toString() +
                     (map.value(QStringLiteral("sensitive")).toBool() ? QStringLiteral("*") : QString());
    }
    QCOMPARE(shape, QStringList({"section:Bank", "multiline:Notes", "numeric:PIN code*"}));

    // A field added and closed without Save leaves the saved item unchanged.
    QVERIFY(!editor->property("changed").toBool());
    QVERIFY(QMetaObject::invokeMethod(options, "showForNew", Q_ARG(QVariant, QStringLiteral("text"))));
    QVERIFY(QMetaObject::invokeMethod(options, "reject"));
    QVERIFY(!editor->property("changed").toBool());

    // Delete Field on the middle field takes only that one away, its
    // neighbour unchanged; Cancel then leaves without keeping it.
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, 1), Q_ARG(QVariant, false)));
    QObject* deleteField = options->findChild<QObject*>(QStringLiteral("deleteFieldButton"));
    QVERIFY(deleteField && QMetaObject::invokeMethod(deleteField, "clicked"));
    QVERIFY(editor->property("changed").toBool());
    QVERIFY(QMetaObject::invokeMethod(editor, "store"));
    QTRY_VERIFY(controller->isIdle());
    shape.clear();
    for (const QVariant& field : controller->item(id).value(QStringLiteral("fields")).toList()) {
        const QVariantMap map = field.toMap();
        shape << map.value(QStringLiteral("kind")).toString() + QLatin1Char(':') + map.value(QStringLiteral("name")).toString() +
                     (map.value(QStringLiteral("sensitive")).toBool() ? QStringLiteral("*") : QString());
    }
    QCOMPARE(shape, QStringList({"section:Bank", "numeric:PIN code*"}));
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, 1), Q_ARG(QVariant, false)));
    QVERIFY(QMetaObject::invokeMethod(deleteField, "clicked"));
    // Cancel with a change not saved asks first; Discard then leaves.
    QObject* cancel = editor->findChild<QObject*>(QStringLiteral("cancelButton"));
    QObject* unsaved = window->findChild<QObject*>(QStringLiteral("unsavedDialog"));
    QVERIFY(cancel && unsaved && QMetaObject::invokeMethod(cancel, "clicked"));
    QTRY_VERIFY(unsaved->property("opened").toBool());
    QCOMPARE(editor->property("itemId").toString(), id);
    QVERIFY(QMetaObject::invokeMethod(unsaved, "discard"));
    QCOMPARE(editor->property("itemId").toString(), QString());
    QCOMPARE(controller->item(id).value(QStringLiteral("fields")).toList().size(), 2);
    controller->lock();

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestPages::the_import_window_offers_import_and_closes_at_a_lock()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("import.chronovault"))),
                            QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    const QString exportFile = folder.filePath(QStringLiteral("export.json"));
    {
        QFile file(exportFile);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"items":[{"title":"Mail","category":"login","template_type":"login.default","fields":[]}]})");
    }
    QObject* importWindow = window->findChild<QObject*>(QStringLiteral("importWindow"));
    QVERIFY(importWindow);
    importWindow->setProperty("reading", true);
    controller->readImport(QUrl::fromLocalFile(exportFile));
    QTRY_VERIFY(importWindow->property("opened").toBool());
    QVariant buttonValue;
    QVERIFY(QMetaObject::invokeMethod(importWindow, "standardButton", Q_RETURN_ARG(QVariant, buttonValue),
                                      Q_ARG(QVariant, 0x00000400))); // Dialog.Ok
    auto* importButton = buttonValue.value<QObject*>();
    QVERIFY(importButton);
    QCOMPARE(importButton->property("text").toString(), QStringLiteral("Import"));
    QVERIFY(importButton->property("enabled").toBool());

    controller->lock();
    QTRY_VERIFY(!importWindow->property("opened").toBool());
    controller->unlock(QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QCOMPARE(controller->items()->rowCount(), 0); // nothing imported
    controller->lock();
}

QTEST_MAIN(TestPages)
#include "test_pages.moc"
