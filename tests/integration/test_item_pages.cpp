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
#include "vault_file.h"

#include <QQmlApplicationEngine>
#include <QQmlError>
#include "settings_files.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>
#include <QQuickItem>
#include <QFile>
#include <QDir>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QApplication>
#include <QFileDialog>
#include <QLineEdit>
#include <QTimer>
#include <functional>

// --- Version ------------------------------------------------------------
// test_item_pages.cpp 0.5.78.3

// --- Version history ----------------------------------------------------
// v0.5.78.3: The export's chooser over the open import window
// v0.5.77: Import… asks first where from (D-060)
// v0.5.76: The item's conflict settled in the Sync conflicts dialog (D-060)
// v0.2.51: The file dialogs answered as a person would (D-055)
// v0.2.45: A merge note on the item and its mark in the list (D-054)
// v0.1.41: Tests of the Notes box, the arrow keys, the cards, the switch and the field types (D-051)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.22: Initial release: the item's view, the editor, the import and the export windows, moved here from
//          test_pages.cpp so that neither runs past 20 seconds (OR-11)

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
// The windows of an item, of importing and of exporting, driven as a person
// would, without a single QML error.
// Answers the next file dialog from a timer, as a person would: `answer`
// looks at it, then accepts or cancels it (D-055). A file dialog waits for
// the person, so the answer has to be on its way before it opens.
void answerNextFileDialog(std::function<void(QFileDialog*)> answer)
{
    auto* poll = new QTimer;
    poll->setInterval(20);
    QObject::connect(poll, &QTimer::timeout, poll, [poll, answer]() {
        if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
            poll->stop();
            poll->deleteLater();
            answer(dialog);
        }
    });
    poll->start();
}

class TestItemPages : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void an_item_shows_a_card_per_section_and_hidden_values_as_dots();
    void the_editor_changes_fields_through_field_options();
    void notes_arrow_keys_cards_and_the_switch();
    void the_import_window_offers_import_and_closes_at_a_lock();
    void the_export_window_asks_the_master_password_first();
    void a_merge_note_is_shown_on_the_item_and_in_the_list();
};

void TestItemPages::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_item_pages"));
    applyStyle();
    SettingsFile().clear();
    VaultsFile().clear();
}

void TestItemPages::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestItemPages::an_item_shows_a_card_per_section_and_hidden_values_as_dots()
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

void TestItemPages::the_editor_changes_fields_through_field_options()
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

    // A field the person names "Notes" is an ordinary field (D-051).
    QVariant index;
    QVERIFY(QMetaObject::invokeMethod(editor, "addField", Q_RETURN_ARG(QVariant, index), Q_ARG(QVariant, QStringLiteral("multiline"))));
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, index), Q_ARG(QVariant, true)));
    QTRY_VERIFY(options->property("opened").toBool());
    options->findChild<QObject*>(QStringLiteral("fieldNameField"))->setProperty("text", QStringLiteral("Notes"));
    QVERIFY(QMetaObject::invokeMethod(options, "accept"));

    // A field added and given a name, a type and the hide mark.
    QVERIFY(QMetaObject::invokeMethod(editor, "addField", Q_RETURN_ARG(QVariant, index), Q_ARG(QVariant, QStringLiteral("text"))));
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, index), Q_ARG(QVariant, true)));
    QTRY_VERIFY(options->property("opened").toBool());
    options->findChild<QObject*>(QStringLiteral("fieldNameField"))->setProperty("text", QStringLiteral("PIN code"));
    options->findChild<QObject*>(QStringLiteral("fieldTypeBox"))->setProperty("currentIndex", 3); // Numeric
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

void TestItemPages::the_import_window_offers_import_and_closes_at_a_lock()
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
    // The chooser opens over the import window, which stays open, as Browse…
    // does (0.5.78.3).
    bool asked = false;
    bool belongs = false;
    bool windowOpen = false;
    answerNextFileDialog([&](QFileDialog* dialog) {
        asked = true;
        belongs = dialog->windowHandle() != nullptr &&
                  dialog->windowHandle()->transientParent() == qobject_cast<QWindow*>(importWindow);
        windowOpen = importWindow->property("visible").toBool();
        dialog->reject();
    });
    // Import… asks first where from (D-060); an export opens the chooser.
    QVERIFY(QMetaObject::invokeMethod(importWindow, "start"));
    QTRY_VERIFY(importWindow->property("opened").toBool());
    importWindow->setProperty("source", QStringLiteral("export"));
    // Next… is a button of its own while choosing; OK (Import) is not shown.
    const QList<QQuickItem*> nextButtons =
        visualItems(qobject_cast<QQuickWindow*>(importWindow)->contentItem(), QStringLiteral("importNextButton"));
    QCOMPARE(nextButtons.size(), 1);
    QQuickItem* nextButton = nextButtons.constFirst();
    QTRY_VERIFY(nextButton->isVisible() && nextButton->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(nextButton, "clicked"));
    QTRY_VERIFY(asked);
    QVERIFY(belongs);
    QVERIFY(windowOpen);
    QVERIFY(importWindow->property("visible").toBool()); // Cancel leaves the import window open
    importWindow->setProperty("reading", true);
    controller->readImport(QUrl::fromLocalFile(exportFile));
    QTRY_COMPARE(importWindow->property("step").toString(), QStringLiteral("summary"));
    QVERIFY(importWindow->property("opened").toBool());
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

void TestItemPages::the_export_window_asks_the_master_password_first()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("export.chronovault"))),
                            QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QObject* exportWindow = window->findChild<QObject*>(QStringLiteral("exportWindow"));
    QVERIFY(exportWindow && QMetaObject::invokeMethod(exportWindow, "ask"));
    QTRY_VERIFY(exportWindow->property("opened").toBool());
    QVariant buttonValue;
    QVERIFY(QMetaObject::invokeMethod(exportWindow, "standardButton", Q_RETURN_ARG(QVariant, buttonValue),
                                      Q_ARG(QVariant, 0x00000400))); // Dialog.Ok
    auto* exportButton = buttonValue.value<QObject*>();
    QVERIFY(exportButton);
    QCOMPARE(exportButton->property("text").toString(), QStringLiteral("Export…"));
    QVERIFY(!exportButton->property("enabled").toBool());
    exportWindow->findChild<QObject*>(QStringLiteral("exportPasswordField"))->setProperty("text", QStringLiteral("a"));
    QVERIFY(exportButton->property("enabled").toBool());
    // Export… chooses where, the window staying open.
    bool chosen = false;
    bool stillOpen = false;
    answerNextFileDialog([&](QFileDialog* dialog) {
        chosen = true;
        stillOpen = exportWindow->property("opened").toBool();
        dialog->reject();
    });
    QVERIFY(QMetaObject::invokeMethod(exportWindow, "accept"));
    QTRY_VERIFY(chosen);
    QVERIFY(stillOpen);

    // Saved in a folder of the person's choice: the dialog starts there the
    // next time, as Qt's own did (D-055).
    QDir(folder.path()).mkdir(QStringLiteral("exports"));
    const QString target = folder.filePath(QStringLiteral("exports/mine.json"));
    answerNextFileDialog([&](QFileDialog* dialog) {
        // As a person: into the folder, then the name typed.
        dialog->setDirectory(QFileInfo(target).absolutePath());
        auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
        QVERIFY(name);
        name->setText(QFileInfo(target).fileName());
        static_cast<QDialog*>(dialog)->accept();
    });
    QVERIFY(QMetaObject::invokeMethod(exportWindow, "accept"));
    QTRY_VERIFY(QFile::exists(target));
    QObject* saveDialog = exportWindow->findChild<QObject*>(QStringLiteral("exportSaveDialog"));
    QVERIFY(saveDialog);
    QCOMPARE(saveDialog->property("currentFolder").toUrl(), QUrl::fromLocalFile(folder.filePath(QStringLiteral("exports"))));

    // A lock while the dialog is open closes it as Cancel: nothing is written.
    QTRY_VERIFY(!exportWindow->property("busy").toBool());
    exportWindow->findChild<QObject*>(QStringLiteral("exportPasswordField"))->setProperty("text", QStringLiteral("a"));
    const QString second = folder.filePath(QStringLiteral("exports/second.json"));
    bool closedByLock = false;
    answerNextFileDialog([&](QFileDialog* dialog) {
        dialog->setDirectory(QFileInfo(second).absolutePath());
        if (auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"))) {
            name->setText(QFileInfo(second).fileName());
        }
        controller->lock();
        closedByLock = !dialog->isVisible();
    });
    QVERIFY(QMetaObject::invokeMethod(exportWindow, "accept"));
    QTRY_VERIFY(closedByLock);
    QVERIFY(QApplication::activeModalWidget() == nullptr);
    QVERIFY(!QFile::exists(second));
    QTRY_VERIFY(!exportWindow->property("opened").toBool());
}

// The one Notes of every item, the arrow keys in the list, the cards' colour
// and Chrono Vault's own switch (D-051).
void TestItemPages::notes_arrow_keys_cards_and_the_switch()
{
    QTemporaryDir folder;
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    showMainWindow(window, false, false);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("notes.chronovault"))),
                            QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QObject* page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QVERIFY(page && editor);

    // A new item: the Notes box is there, its text kept with Save.
    QVERIFY(QMetaObject::invokeMethod(page, "newItem", Q_ARG(QVariant, QStringLiteral("login"))));
    const QString first = editor->property("itemId").toString();
    QObject* notes = editor->findChild<QObject*>(QStringLiteral("noteArea"));
    QVERIFY(notes);
    QCOMPARE(notes->property("placeholderText").toString(), QStringLiteral("Add Note"));
    notes->setProperty("text", QStringLiteral("Bought in 2024"));
    editor->setProperty("changed", true);
    editor->findChild<QObject*>(QStringLiteral("titleField"))->setProperty("text", QStringLiteral("Alpha"));
    QVERIFY(QMetaObject::invokeMethod(editor, "store"));
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->item(first).value(QStringLiteral("note")).toString(), QStringLiteral("Bought in 2024"));
    QVERIFY(QMetaObject::invokeMethod(editor, "close"));

    // A second item, then the arrow keys move the item shown.
    QVERIFY(QMetaObject::invokeMethod(page, "newItem", Q_ARG(QVariant, QStringLiteral("login"))));
    const QString second = editor->property("itemId").toString();
    editor->findChild<QObject*>(QStringLiteral("titleField"))->setProperty("text", QStringLiteral("Beta"));
    QVERIFY(QMetaObject::invokeMethod(editor, "store"));
    QTRY_VERIFY(controller->isIdle());
    QVERIFY(QMetaObject::invokeMethod(editor, "close"));
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, first)));
    auto* list = window->findChild<QQuickItem*>(QStringLiteral("itemList"));
    QVERIFY(list);
    list->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_Down);
    QTRY_COMPARE(page->property("chosenItemId").toString(), second);
    QTest::keyClick(window, Qt::Key_Up);
    QTRY_COMPARE(page->property("chosenItemId").toString(), first);
    QTest::keyClick(window, Qt::Key_End);
    QTRY_COMPARE(page->property("chosenItemId").toString(), second);

    // The cards stand on the theme's Base colour.
    bool baseCard = false;
    for (QQuickItem* item : window->contentItem()->findChildren<QQuickItem*>()) {
        if (QString::fromLatin1(item->metaObject()->className()).startsWith(QStringLiteral("QQuickRectangle")) &&
            item->isVisible() && item->property("color").value<QColor>() == QGuiApplication::palette().color(QPalette::Base)) {
            baseCard = true;
        }
    }
    QVERIFY(baseCard);

    // Chrono Vault's own switch: blue with the knob at the right when on.
    QVERIFY(QMetaObject::invokeMethod(page, "editChosen"));
    QVariant index;
    QVERIFY(QMetaObject::invokeMethod(editor, "addField", Q_RETURN_ARG(QVariant, index), Q_ARG(QVariant, QStringLiteral("text"))));
    QObject* options = window->findChild<QObject*>(QStringLiteral("fieldOptions"));
    QVERIFY(QMetaObject::invokeMethod(options, "showFor", Q_ARG(QVariant, index), Q_ARG(QVariant, true)));
    QTRY_VERIFY(options->property("opened").toBool());
    QObject* toggle = options->findChild<QObject*>(QStringLiteral("sensitiveSwitch"));
    QVERIFY(toggle);
    auto* track = toggle->findChild<QQuickItem*>(QStringLiteral("switchTrack"));
    auto* knob = toggle->findChild<QQuickItem*>(QStringLiteral("switchKnob"));
    QVERIFY(track && knob);
    toggle->setProperty("checked", false);
    QTRY_VERIFY(knob->x() < track->width() / 2);
    // Choosing PIN as the type switches Sensitive on (D-051).
    QObject* typeBox = options->findChild<QObject*>(QStringLiteral("fieldTypeBox"));
    QVERIFY(QMetaObject::invokeMethod(typeBox, "activated", Q_ARG(int, 2)));
    QVERIFY(toggle->property("checked").toBool());
    toggle->setProperty("checked", false);
    toggle->setProperty("checked", true);
    QTRY_VERIFY(knob->x() > track->width() / 2);
    QCOMPARE(track->property("color").value<QColor>(), QGuiApplication::palette().color(QPalette::Highlight));
    QVERIFY(QMetaObject::invokeMethod(options, "close"));
    editor->setProperty("changed", false);
    QVERIFY(QMetaObject::invokeMethod(editor, "close"));

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
    controller->lock();
}

// A title changed here and, a minute later, elsewhere: the item shows the
// note with "Use the other value", and the list its mark (D-054).
void TestItemPages::a_merge_note_is_shown_on_the_item_and_in_the_list()
{
    QTemporaryDir folder;
    QVERIFY(folder.isValid());
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    const QString vaultFile = folder.filePath(QStringLiteral("merge.chronovault"));
    controller->createVault(QUrl::fromLocalFile(vaultFile), QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QVariantMap item = controller->newLogin();
    item.insert(QStringLiteral("title"), QStringLiteral("Bank"));
    const QString id = item.value(QStringLiteral("id")).toString();
    QVERIFY(controller->saveItem(item));
    QTRY_VERIFY(controller->isIdle());

    {
        const OpenResult opened = openVaultFile(vaultFile, "a");
        ContentsResult read = contentsFromJson(opened.contents);
        read.contents.items[0].title = QStringLiteral("The bank");
        read.contents.items[0].modified = QDateTime::currentDateTimeUtc().addSecs(60);
        QCOMPARE(saveVaultFile(vaultFile, contentsToJson(read.contents), opened.key), SaveStatus::Saved);
    }
    QVariantMap mine = controller->item(id);
    mine.insert(QStringLiteral("title"), QStringLiteral("My bank"));
    QVERIFY(controller->saveItem(mine));
    QTRY_VERIFY(controller->isIdle());
    QTRY_VERIFY(!controller->mergeNotes(id).isEmpty());

    QObject* page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id)));
    auto* root = qobject_cast<QQuickWindow*>(window)->contentItem();
    QTRY_VERIFY(visualItems(root, QStringLiteral("mergeNote")).constFirst()->isVisible());
    QTRY_VERIFY(visualItems(root, QStringLiteral("mergedMark")).constFirst()->isVisible());
    // Settle… opens the item's conflict in the Sync conflicts dialog (D-060):
    // nothing chosen beforehand, Apply once every row is chosen.
    QQuickItem* settle = visualItems(root, QStringLiteral("settleConflictButton")).constFirst();
    QVERIFY(settle->isVisible());
    QVERIFY(QMetaObject::invokeMethod(settle, "clicked"));
    auto* conflict = qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QStringLiteral("conflictWindow")));
    QVERIFY(conflict);
    QTRY_VERIFY(conflict->isVisible());
    QQuickItem* apply = visualItems(conflict->contentItem(), QStringLiteral("conflictApplyButton")).constFirst();
    QVERIFY(!apply->isEnabled());
    // As a person clicks: the button toggles, then says it was clicked. The
    // circles show the last choice only.
    // This device's on the left, the other device's on the right (D-061).
    QQuickItem* nowChoice = visualItems(conflict->contentItem(), QStringLiteral("conflictLeft_0")).constFirst();
    QQuickItem* otherChoice = visualItems(conflict->contentItem(), QStringLiteral("conflictRight_0")).constFirst();
    QCOMPARE(nowChoice->property("text").toString(), QStringLiteral("My bank"));
    QCOMPARE(otherChoice->property("text").toString(), QStringLiteral("The bank"));
    // The header's choices stand in line with the rows' (P53).
    QQuickItem* allLeft = visualItems(conflict->contentItem(), QStringLiteral("conflictAllLeft")).constFirst();
    QCOMPARE(allLeft->property("text").toString(), QStringLiteral("This device"));
    QTRY_COMPARE(allLeft->mapToScene(QPointF(0, 0)).x(), nowChoice->mapToScene(QPointF(0, 0)).x());
    for (QQuickItem* choice : {nowChoice, otherChoice}) {
        QVERIFY(QMetaObject::invokeMethod(choice, "toggle"));
        QVERIFY(QMetaObject::invokeMethod(choice, "clicked"));
    }
    QTRY_VERIFY(otherChoice->property("checked").toBool());
    QVERIFY(!nowChoice->property("checked").toBool());
    QTRY_VERIFY(apply->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(apply, "clicked"));
    QTRY_VERIFY(!conflict->isVisible());
    QTRY_VERIFY(!visualItems(root, QStringLiteral("mergeNote")).constFirst()->isVisible());
    QTRY_VERIFY(controller->isIdle());
    QCOMPARE(controller->item(id).value(QStringLiteral("title")).toString(), QStringLiteral("The bank"));
}

QTEST_MAIN(TestItemPages)
#include "test_item_pages.moc"
