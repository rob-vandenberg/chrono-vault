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
#include "vault_controller.h"

#include <QClipboard>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_menus.cpp 0.0.24

// --- Version history ----------------------------------------------------
// v0.0.24: Initial release (D-039)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

// An item in the tree of items (a Repeater's or a delegate's).
QQuickItem* visualItem(QQuickItem* root, const QString& name)
{
    if (!root) {
        return nullptr;
    }
    if (root->objectName() == name && root->isVisible()) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = visualItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

// A secret of a one-time code (RFC 6238's example key, in base 32).
const QString codeSecret = QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");

} // namespace

// The menus after the reference program's (D-039), driven as a person would,
// without a single QML error.
class TestMenus : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void the_menus_hold_their_entries_in_order_with_their_shortcuts();
    void the_item_menu_copies_the_chosen_items_fields_and_greys_what_it_lacks();
    void shortcuts_stand_aside_while_a_text_field_is_typed_in();
    void the_view_menu_hides_the_sidebar_and_its_groups_and_shows_counts();
    void clear_clipboard_empties_it_and_the_code_has_a_copy_button();

private:
    // The main window with a vault unlocked, and one item shown.
    bool open(const QString& name);
    QString addItem(const QVariantList& fields, const QString& title);
    QObject* action(const QString& name) const { return window->findChild<QObject*>(name); }
    QStringList menuEntries(int menu) const;

    QTemporaryDir* folder = nullptr;
    QQmlApplicationEngine* engine = nullptr;
    QList<QQmlError> qmlWarnings;
    QObject* window = nullptr;
    VaultController* controller = nullptr;
    QObject* page = nullptr;
};

void TestMenus::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_menus"));
    applyStyle();
}

void TestMenus::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestMenus::init()
{
    QSettings().clear();
    qmlWarnings.clear();
    folder = new QTemporaryDir;
}

void TestMenus::cleanup()
{
    if (controller) {
        controller->lock();
    }
    delete engine;
    engine = nullptr;
    window = nullptr;
    controller = nullptr;
    page = nullptr;
    delete folder;
    folder = nullptr;
    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

bool TestMenus::open(const QString& name)
{
    engine = new QQmlApplicationEngine;
    QObject::connect(engine, &QQmlEngine::warnings, engine,
                     [this](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine->loadFromModule("ChronoVault", "Main");
    if (engine->rootObjects().isEmpty()) {
        return false;
    }
    window = engine->rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    controller = engine->singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder->filePath(name + QStringLiteral(".chronovault"))),
                            QStringLiteral("a"), QStringLiteral("a"));
    page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
    return QTest::qWaitFor([this]() { return controller->state() == State::Unlocked; }, 10000) && page;
}

QString TestMenus::addItem(const QVariantList& fields, const QString& title)
{
    QVariantMap item = controller->newLogin();
    item.insert(QStringLiteral("fields"), fields);
    item.insert(QStringLiteral("title"), title);
    const QString id = item.value(QStringLiteral("id")).toString();
    if (!controller->saveItem(item) || !QTest::qWaitFor([this]() { return controller->isIdle(); }, 10000)) {
        return {};
    }
    QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id));
    return id;
}

QStringList TestMenus::menuEntries(int menu) const
{
    static const QStringList names{QStringLiteral("fileMenu"), QStringLiteral("viewMenu"), QStringLiteral("itemMenu"),
                                   QStringLiteral("toolsMenu"), QStringLiteral("helpMenu")};
    QObject* chosen = window->findChild<QObject*>(names.at(menu));
    if (!chosen) {
        return {};
    }
    QStringList entries{chosen->property("title").toString()};
    const int count = chosen->property("count").toInt();
    for (int index = 0; index < count; ++index) {
        QQuickItem* entry = nullptr;
        QMetaObject::invokeMethod(chosen, "itemAt", Q_RETURN_ARG(QQuickItem*, entry), Q_ARG(int, index));
        if (QString(entry->metaObject()->className()).contains(QLatin1String("Separator"))) {
            entries << QStringLiteral("---");
            continue;
        }
        QString text = entry->property("text").toString();
        if (const QString keys = entry->property("shortcutText").toString(); !keys.isEmpty()) {
            text += QStringLiteral(" [") + keys + QLatin1Char(']');
        }
        entries << text;
    }
    return entries;
}

void TestMenus::the_menus_hold_their_entries_in_order_with_their_shortcuts()
{
    QVERIFY(open(QStringLiteral("order")));
    // The keyboard is in the list after an unlock, not in a hidden page.
    QCOMPARE(qobject_cast<QQuickWindow*>(window)->activeFocusItem(),
             window->findChild<QQuickItem*>(QStringLiteral("itemList")));
    QCOMPARE(menuEntries(0), QStringList({"&File", "New vault", "Open vault", "Change Master Password", "---", "Backup",
                                          "Restore", "---", "Import", "Export", "---", "Preferences", "---",
                                          "Quit [Ctrl+Q]"}));
    QCOMPARE(menuEntries(1), QStringList({"&View", "Show Item Counts", "Hide Sidebar", "Hide Sensitive", "---",
                                          "Categories", "Tags", "Others"}));
    QCOMPARE(menuEntries(2),
             QStringList({"&Item", "Archive [Shift+Backspace]", "Move to Trash [Backspace]", "---",
                          "Copy Username [Shift+Ctrl+U]", "Copy E-mail [Shift+Ctrl+E]", "Copy Password [Shift+Ctrl+P]",
                          "Show Passwords [Shift+Ctrl+H]", "Copy URL [Shift+Ctrl+R]", "Copy OTP [Shift+Ctrl+T]",
                          "Launch URL [Shift+Ctrl+L]"}));
    QCOMPARE(menuEntries(3), QStringList({"&Tools", "Lock vault [Ctrl+L]", "Clear Clipboard [Ctrl+B]",
                                          "Generate Password [Ctrl+G]"}));
    QCOMPARE(menuEntries(4), QStringList({"&Help", "Check for updates", "Support website", "About [F1]"}));
    // Backup and Restore are not built yet: greyed.
    QObject* file = window->findChild<QObject*>(QStringLiteral("fileMenu"));
    QVERIFY(file);
    for (int index : {4, 5}) {
        QQuickItem* entry = nullptr;
        QMetaObject::invokeMethod(file, "itemAt", Q_RETURN_ARG(QQuickItem*, entry), Q_ARG(int, index));
        QVERIFY(!entry->isEnabled());
    }
}

void TestMenus::the_item_menu_copies_the_chosen_items_fields_and_greys_what_it_lacks()
{
    QVERIFY(open(QStringLiteral("item")));
    QClipboard* clipboard = QGuiApplication::clipboard();
    const QString id = addItem(
        {QVariantMap{{"role", "username"}, {"label", ""}, {"kind", "username"}, {"value", "rob"}},
         QVariantMap{{"role", ""}, {"label", "Old"}, {"kind", "email"}, {"value", "gone@example.org"}, {"deleted", true}},
         QVariantMap{{"role", ""}, {"label", "Password"}, {"kind", "text"}, {"value", "secret"}, {"sensitive", true}},
         QVariantMap{{"role", ""}, {"label", "Website"}, {"kind", "url"}, {"value", "example.org"}},
         QVariantMap{{"role", ""}, {"label", "Code"}, {"kind", "totp"}, {"value", codeSecret}}},
        QStringLiteral("Example"));
    QVERIFY(!id.isEmpty());
    QTRY_VERIFY(action(QStringLiteral("copyUsernameAction"))->property("enabled").toBool());

    const auto copied = [&](const char* name) {
        QMetaObject::invokeMethod(action(QString::fromLatin1(name)), "trigger");
        return clipboard->text();
    };
    QCOMPARE(copied("copyUsernameAction"), QStringLiteral("rob"));
    QCOMPARE(copied("copyPasswordAction"), QStringLiteral("secret"));
    QCOMPARE(copied("copyUrlAction"), QStringLiteral("example.org"));
    QCOMPARE(copied("copyOtpAction"), controller->oneTimeCode(codeSecret));
    // Launch URL: an address without a scheme opens as https, a host with a
    // port included.
    for (const auto& [given, opened] : {std::pair{"example.org", "https://example.org"},
                                        std::pair{"nas.local:5000/admin", "https://nas.local:5000/admin"},
                                        std::pair{" example.org ", "https://example.org"},
                                        std::pair{"http://router.lan", "http://router.lan"},
                                        std::pair{"mailto:me@example.org", "mailto:me@example.org"}}) {
        QVariant result;
        QMetaObject::invokeMethod(window, "addressToOpen", Q_RETURN_ARG(QVariant, result),
                                  Q_ARG(QVariant, QString::fromLatin1(given)));
        QCOMPARE(result.toString(), QString::fromLatin1(opened));
    }
    // A deleted field does not count: no e-mail to copy.
    QVERIFY(!action(QStringLiteral("copyEmailAction"))->property("enabled").toBool());

    // Show Passwords shows the password as text, until another item is chosen.
    auto* root = qobject_cast<QQuickWindow*>(window)->contentItem();
    const auto shownValues = [root]() {
        QStringList values;
        std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
            if (item->objectName() == QLatin1String("fieldValue") && item->isVisible()) {
                values << item->property("text").toString();
            }
            for (QQuickItem* child : item->childItems()) {
                walk(child);
            }
        };
        walk(root);
        return values;
    };
    QTRY_VERIFY(shownValues().contains(QStringLiteral("••••••••")));
    QMetaObject::invokeMethod(action(QStringLiteral("showPasswordsAction")), "trigger");
    QTRY_VERIFY(shownValues().contains(QStringLiteral("secret")));
    QVERIFY(page->property("revealAll").toBool());
    // Hide Sensitive switched off and on: hidden again.
    controller->setHideSensitive(false);
    controller->setHideSensitive(true);
    QVERIFY(!page->property("revealAll").toBool());
    QTRY_VERIFY(!shownValues().contains(QStringLiteral("secret")));
    QMetaObject::invokeMethod(action(QStringLiteral("showPasswordsAction")), "trigger");
    QVERIFY(page->property("revealAll").toBool());

    // A note without fields: every copy greyed, Show Passwords off again.
    const QString note = addItem({}, QStringLiteral("Note"));
    QTRY_VERIFY(!action(QStringLiteral("copyUsernameAction"))->property("enabled").toBool());
    QVERIFY(!page->property("revealAll").toBool());
    for (const char* name : {"copyPasswordAction", "copyUrlAction", "copyOtpAction", "launchUrlAction"}) {
        QVERIFY2(!action(QString::fromLatin1(name))->property("enabled").toBool(), name);
    }

    // A one-time code whose secret makes no code: Copy OTP greyed.
    QVERIFY(!addItem({QVariantMap{{"role", ""}, {"label", "Code"}, {"kind", "totp"}, {"value", "not a secret!"}}},
                     QStringLiteral("Broken"))
                 .isEmpty());
    QTRY_VERIFY(!action(QStringLiteral("copyOtpAction"))->property("enabled").toBool());

    // While the editor is open, the Item menu waits.
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id)));
    QTRY_VERIFY(action(QStringLiteral("archiveAction"))->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(page, "editChosen"));
    QTRY_VERIFY(!action(QStringLiteral("archiveAction"))->property("enabled").toBool());
    QVERIFY(!action(QStringLiteral("copyUsernameAction"))->property("enabled").toBool());
    QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
    QVERIFY(QMetaObject::invokeMethod(editor, "close"));

    // Archive moves it into the Archive; then Archive is greyed for it.
    QTRY_VERIFY(action(QStringLiteral("archiveAction"))->property("enabled").toBool());
    QMetaObject::invokeMethod(action(QStringLiteral("archiveAction")), "trigger");
    QTRY_VERIFY(controller->item(id).value(QStringLiteral("archived")).toBool());
    controller->setFilter(QStringLiteral("archive"));
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id)));
    QTRY_VERIFY(!action(QStringLiteral("archiveAction"))->property("enabled").toBool());
    QVERIFY(action(QStringLiteral("moveToTrashAction"))->property("enabled").toBool());
    QVERIFY(!note.isEmpty());
}

void TestMenus::shortcuts_stand_aside_while_a_text_field_is_typed_in()
{
    QVERIFY(open(QStringLiteral("keys")));
    const QString id = addItem({QVariantMap{{"role", "username"}, {"label", ""}, {"kind", "username"}, {"value", "rob"}}},
                               QStringLiteral("Example"));
    QVERIFY(!id.isEmpty());
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    quickWindow->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(quickWindow));

    // Backspace in the search field deletes a letter, never the item.
    auto* search = window->findChild<QQuickItem*>(QStringLiteral("searchField"));
    QVERIFY(search);
    search->forceActiveFocus();
    QTRY_VERIFY(window->property("typing").toBool());
    // The menus still show their shortcuts meanwhile.
    QVERIFY(menuEntries(2).contains(QStringLiteral("Copy Username [Shift+Ctrl+U]")));
    QTest::keyClick(quickWindow, Qt::Key_X);
    QTRY_COMPARE(search->property("text").toString(), QStringLiteral("x"));
    QTest::keyClick(quickWindow, Qt::Key_Backspace);
    QCOMPARE(search->property("text").toString(), QString());
    QVERIFY(!controller->item(id).value(QStringLiteral("trashed")).toBool());

    // Outside a text field, Shift+Ctrl+U copies and Backspace moves to the Trash.
    auto* list = window->findChild<QQuickItem*>(QStringLiteral("itemList"));
    list->forceActiveFocus();
    QTRY_VERIFY(!window->property("typing").toBool());
    QTest::keyClick(quickWindow, Qt::Key_U, Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("rob"));
    QTest::keyClick(quickWindow, Qt::Key_Backspace);
    QTRY_VERIFY(controller->item(id).value(QStringLiteral("trashed")).toBool());
}

void TestMenus::the_view_menu_hides_the_sidebar_and_its_groups_and_shows_counts()
{
    QVERIFY(open(QStringLiteral("view")));
    QVERIFY(!addItem({}, QStringLiteral("One")).isEmpty());
    const QString archived = addItem({}, QStringLiteral("Two"));
    controller->archiveItem(archived, true);
    QTRY_VERIFY(controller->isIdle());
    auto* root = qobject_cast<QQuickWindow*>(window)->contentItem();

    // Counts: off at first, then beside Home, Archive and the rest.
    QVERIFY(!visualItem(root, QStringLiteral("count_")));
    QMetaObject::invokeMethod(action(QStringLiteral("showItemCountsAction")), "toggle");
    QTRY_VERIFY(visualItem(root, QStringLiteral("count_")));
    QCOMPARE(visualItem(root, QStringLiteral("count_"))->property("text").toString(), QStringLiteral("1"));
    QCOMPARE(visualItem(root, QStringLiteral("count_archive"))->property("text").toString(), QStringLiteral("1"));
    QCOMPARE(visualItem(root, QStringLiteral("count_trash"))->property("text").toString(), QStringLiteral("0"));
    QCOMPARE(visualItem(root, QStringLiteral("count_category:login"))->property("text").toString(),
             QStringLiteral("1"));
    QVERIFY(QSettings().value(QStringLiteral("view/showItemCounts")).toBool());

    // Others, Categories, Tags hidden and shown again; a group hidden while
    // one of its entries is chosen leaves the list at Home.
    QVERIFY(visualItem(root, QStringLiteral("trashEntry")));
    controller->setFilter(QStringLiteral("archive"));
    QMetaObject::invokeMethod(action(QStringLiteral("showOthersAction")), "toggle");
    QTRY_VERIFY(!visualItem(root, QStringLiteral("trashEntry")));
    QVERIFY(!action(QStringLiteral("showOthersAction"))->property("checked").toBool());
    QCOMPARE(controller->filter(), QString());
    controller->setFilter(QStringLiteral("category:login"));
    QMetaObject::invokeMethod(action(QStringLiteral("showCategoriesAction")), "toggle");
    QTRY_VERIFY(!visualItem(root, QStringLiteral("categoryEntry_login")));
    QCOMPARE(controller->filter(), QString());
    QMetaObject::invokeMethod(action(QStringLiteral("showCategoriesAction")), "toggle");
    QTRY_VERIFY(visualItem(root, QStringLiteral("categoryEntry_login")));

    // Hide Sidebar, kept for the next start.
    QMetaObject::invokeMethod(action(QStringLiteral("hideSidebarAction")), "toggle");
    QTRY_VERIFY(!window->findChild<QQuickItem*>(QStringLiteral("vaultColumn"))->isVisible());
    QVERIFY(QSettings().value(QStringLiteral("view/sidebarHidden")).toBool());

    // Hide Sensitive in the menu is the Security setting; a change made
    // elsewhere shows in the menu.
    QVERIFY(action(QStringLiteral("hideSensitiveAction"))->property("checked").toBool());
    QMetaObject::invokeMethod(action(QStringLiteral("hideSensitiveAction")), "toggle");
    QVERIFY(!controller->hideSensitive());
    controller->setHideSensitive(true);
    QVERIFY(action(QStringLiteral("hideSensitiveAction"))->property("checked").toBool());
}

void TestMenus::clear_clipboard_empties_it_and_the_code_has_a_copy_button()
{
    QVERIFY(open(QStringLiteral("clipboard")));
    QClipboard* clipboard = QGuiApplication::clipboard();
    QVERIFY(!addItem({QVariantMap{{"role", ""}, {"label", "Code"}, {"kind", "totp"}, {"value", codeSecret}}},
                     QStringLiteral("Code"))
                 .isEmpty());
    auto* root = qobject_cast<QQuickWindow*>(window)->contentItem();
    QQuickItem* copyCode = nullptr;
    QTRY_VERIFY((copyCode = visualItem(root, QStringLiteral("copyCodeButton"))) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(copyCode, "clicked"));
    QCOMPARE(clipboard->text(), controller->oneTimeCode(codeSecret));

    // Clear Clipboard empties it at once, whatever is on it.
    clipboard->setText(QStringLiteral("someone else's"));
    QMetaObject::invokeMethod(action(QStringLiteral("clearClipboardAction")), "trigger");
    QVERIFY(clipboard->text().isEmpty());
    QCOMPARE(controller->message(), QStringLiteral("The clipboard is cleared."));
}

QTEST_MAIN(TestMenus)
#include "test_menus.moc"
