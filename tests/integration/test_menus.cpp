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
#include <QCursor>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include "settings_files.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_menus.cpp 0.8.119

// --- Version history ----------------------------------------------------
// v0.8.119: Four tests in test_menus_second, so each binary stays under 20 seconds (OR-11)
// v0.7.111: The arrows move the selection in the key-in-use window (D-079)
// v0.6.95: UI scaling to 200%, Options in Tools, Lock vault in File, first sizes and headings (the owner)
// v0.6.88: The settings' topics in their new order (D-066)
// v0.6.86: View > Audit, Exclude from Audit (D-065)
// v0.5.83: The Item menu on the selected items (D-063)
// v0.5.82: Backspace asks first (D-062)
// v0.5.76: Tools > Sync Conflicts… (D-060)
// v0.3.60: Tools > Sync Now, greyed for a vault on this computer (D-057)
// v0.2.56: Edit… and Reset or Delete on every category; Edit… on a right-click in the sidebar (D-056)
// v0.2.50: Keys a text field never uses are commands while typing
// v0.2.49: The sidebar snap after 80 pixels each way
// v0.2.48: The sidebar snap after 100 pixels each way
// v0.2.47: Counts above 0 only; the sidebar snaps shut and open when dragged (the owner)
// v0.2.46: The main window's shortcuts do nothing with the settings window in front (P33)
// v0.1.42: Tests of Esc through the menu bar and the About box (D-052)
// v0.0.39: Tests of the settings tidied, the scroll bars and Esc in the About box (D-049)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.27: the sidebar collapses to its icons, its groups fold, the splitters (D-042)
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
#if !defined(CHRONO_VAULT_MENUS_SECOND)
    void the_menus_hold_their_entries_in_order_with_their_shortcuts();
    void the_item_menu_copies_the_chosen_items_fields_and_greys_what_it_lacks();
    void shortcuts_stand_aside_while_a_text_field_is_typed_in();
    void the_view_menu_hides_the_sidebar_and_its_groups_and_shows_counts();
    void clear_clipboard_empties_it_and_the_code_has_a_copy_button();
    void the_sidebar_collapses_to_icons_and_its_groups_fold();
#else
    void the_sidebar_snaps_shut_and_open_when_its_splitter_is_dragged();
    void every_category_has_edit_and_reset_or_delete_and_a_right_click_menu();
    void after_a_menu_the_content_holds_the_keyboard();
    void keyboard_shortcuts_are_set_on_their_page_and_kept();
#endif

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
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestMenus::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
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

#if !defined(CHRONO_VAULT_MENUS_SECOND)
void TestMenus::the_menus_hold_their_entries_in_order_with_their_shortcuts()
{
    QVERIFY(open(QStringLiteral("order")));
    // The keyboard is in the list after an unlock, not in a hidden page.
    QCOMPARE(qobject_cast<QQuickWindow*>(window)->activeFocusItem(),
             window->findChild<QQuickItem*>(QStringLiteral("itemList")));
    QCOMPARE(menuEntries(0), QStringList({"&File", "New vault", "Open vault", "Lock vault [Ctrl+L]", "Change Master Password", "---", "Backup",
                                          "Restore", "---", "Import", "Export", "---",
                                          "Quit [Ctrl+Q]"}));
    QCOMPARE(menuEntries(1), QStringList({"&View", "Show Item Counts", "Hide Sidebar", "Hide Sensitive", "---",
                                          "Categories", "Tags", "Audit", "Others"}));
    QCOMPARE(menuEntries(2),
             QStringList({"&Item", "Add to Favorites", "Archive [Shift+Backspace]", "Change Category",
                          "Exclude from Audit",
                          "Move to Trash [Backspace]", "Restore", "Delete Permanently… [Backspace]", "---",
                          "Copy Username [Shift+Ctrl+U]", "Copy E-mail [Shift+Ctrl+E]", "Copy Password [Shift+Ctrl+P]",
                          "Show Passwords [Shift+Ctrl+H]", "Copy URL [Shift+Ctrl+R]", "Copy OTP [Shift+Ctrl+T]",
                          "Launch URL [Shift+Ctrl+L]"}));
    QCOMPARE(menuEntries(3), QStringList({"&Tools", "Sync Now [Ctrl+J]", "Sync Conflicts…",
                                          "Clear Clipboard [Shift+Ctrl+B]", "Generate Password [Ctrl+G]", "---", "Options"}));
    // Sync Now only for a vault kept on a WebDAV server (D-057).
    QObject* syncNow = window->findChild<QObject*>(QStringLiteral("syncNowAction"));
    QVERIFY(syncNow);
    QVERIFY(!syncNow->property("enabled").toBool());
    QCOMPARE(menuEntries(4), QStringList({"&Help", "Check for updates", "Support website", "Keyboard shortcuts…",
                                          "About [F1]"}));
    // Backup and Restore are not built yet: greyed.
    QObject* file = window->findChild<QObject*>(QStringLiteral("fileMenu"));
    QVERIFY(file);
    for (int index : {5, 6}) {
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

    // Archive moves it into the Archive; then it offers Unarchive (D-063).
    QTRY_VERIFY(action(QStringLiteral("archiveAction"))->property("enabled").toBool());
    QMetaObject::invokeMethod(action(QStringLiteral("archiveAction")), "trigger");
    QTRY_VERIFY(controller->item(id).value(QStringLiteral("archived")).toBool());
    controller->setFilter(QStringLiteral("archive"));
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id)));
    QTRY_COMPARE(action(QStringLiteral("archiveAction"))->property("text").toString(), QStringLiteral("Unarchive"));
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

    // Keys a text field never uses are commands while typing (the owner):
    // Shift+Ctrl+U copies, F1 opens About; Ctrl+A stays the field's own.
    QGuiApplication::clipboard()->clear();
    QTest::keyClick(quickWindow, Qt::Key_U, Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("rob"));
    QTest::keyClick(quickWindow, Qt::Key_X);
    QTest::keyClick(quickWindow, Qt::Key_Y);
    QTest::keyClick(quickWindow, Qt::Key_A, Qt::ControlModifier);
    QCOMPARE(search->property("selectedText").toString(), QStringLiteral("xy"));
    QTest::keyClick(quickWindow, Qt::Key_Backspace);
    QCOMPARE(search->property("text").toString(), QString());
    QObject* about = window->findChild<QObject*>(QStringLiteral("aboutWindow"));
    QVERIFY(about);
    QTest::keyClick(quickWindow, Qt::Key_F1);
    QTRY_VERIFY(about->property("visible").toBool());
    QMetaObject::invokeMethod(about, "close");
    QTRY_VERIFY(!about->property("visible").toBool());
    quickWindow->requestActivate();
    QTRY_COMPARE(QGuiApplication::focusWindow(), quickWindow);
    search->forceActiveFocus();

    // Outside a text field, Shift+Ctrl+U copies and Backspace moves to the Trash.
    auto* list = window->findChild<QQuickItem*>(QStringLiteral("itemList"));
    list->forceActiveFocus();
    QTRY_VERIFY(!window->property("typing").toBool());
    QTest::keyClick(quickWindow, Qt::Key_U, Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("rob"));

    // With the settings window in front, the main window's shortcuts do
    // nothing (P33); with the main window in front again, they answer.
    QGuiApplication::clipboard()->clear();
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    auto* settingsQuick = qobject_cast<QQuickWindow*>(settings);
    settingsQuick->requestActivate();
    QTRY_COMPARE(QGuiApplication::focusWindow(), settingsQuick);
    QTRY_VERIFY(!window->property("holdsKeyboard").toBool());
    QTest::keyClick(settingsQuick, Qt::Key_U, Qt::ShiftModifier | Qt::ControlModifier);
    QVERIFY(QGuiApplication::clipboard()->text().isEmpty());
    settingsQuick->close();
    quickWindow->requestActivate();
    QTRY_COMPARE(QGuiApplication::focusWindow(), quickWindow);
    list->forceActiveFocus();
    QTRY_VERIFY(window->property("holdsKeyboard").toBool());
    QTest::keyClick(quickWindow, Qt::Key_U, Qt::ShiftModifier | Qt::ControlModifier);
    QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("rob"));

    // Asked first (D-062): No leaves it, Yes moves it.
    QTest::keyClick(quickWindow, Qt::Key_Backspace);
    QObject* question = window->findChild<QObject*>(QStringLiteral("placeQuestion"));
    QVERIFY(question);
    QTRY_VERIFY(question->property("visible").toBool());
    QVERIFY(!controller->item(id).value(QStringLiteral("trashed")).toBool());
    QVERIFY(QMetaObject::invokeMethod(question, "accept"));
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
    // A count of 0 is not shown, and its entry's name keeps that room; an
    // entry with a count leaves room for it (the owner, 23 November 2026).
    QVERIFY(!visualItem(root, QStringLiteral("count_trash")));
    QQuickItem* trashEntry = visualItem(root, QStringLiteral("trashEntry"));
    QQuickItem* homeEntry = visualItem(root, QStringLiteral("homeEntry"));
    QCOMPARE(trashEntry->property("rightPadding").toReal(), trashEntry->property("padding").toReal());
    QVERIFY(homeEntry->property("rightPadding").toReal() >
            homeEntry->property("padding").toReal() + visualItem(root, QStringLiteral("count_"))->width());
    QCOMPARE(visualItem(root, QStringLiteral("count_category:login"))->property("text").toString(),
             QStringLiteral("1"));
    QVERIFY(SettingsFile().value(QStringLiteral("view/showItemCounts")).toBool());

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
    QVERIFY(SettingsFile().value(QStringLiteral("view/sidebarHidden")).toBool());

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

void TestMenus::the_sidebar_collapses_to_icons_and_its_groups_fold()
{
    QVERIFY(open(QStringLiteral("sidebar")));
    auto* root = qobject_cast<QQuickWindow*>(window)->contentItem();
    auto* column = window->findChild<QQuickItem*>(QStringLiteral("vaultColumn"));
    QQuickItem* toggle = visualItem(root, QStringLiteral("sidebarToggle"));
    QVERIFY(column && toggle);
    // Archive and Trash have icons of their own.
    for (const char* name : {"archiveEntry", "trashEntry"}) {
        QQuickItem* entry = visualItem(root, QString::fromLatin1(name));
        QVERIFY2(entry, name);
        const QString source = QQmlProperty::read(entry, QStringLiteral("icon.source")).toUrl().toString();
        QVERIFY2(source.endsWith(QString::fromLatin1(name).chopped(5) + QStringLiteral(".svg")), qPrintable(source));
    }
    const qreal expandedWidth = column->width();
    QQuickItem* homeEntry = visualItem(root, QStringLiteral("homeEntry"));
    QVERIFY(homeEntry);
    const qreal expandedHeight = homeEntry->height();
    const qreal expandedIcon = QQmlProperty::read(homeEntry, QStringLiteral("icon.height")).toReal();

    // Collapsed: a narrow column, icons only, remembered.
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_VERIFY(column->width() < expandedWidth / 2);
    QVERIFY(controller->sidebarCollapsed());
    QQuickItem* home = visualItem(root, QStringLiteral("homeEntry"));
    QCOMPARE(home->property("display").toInt(), 0); // AbstractButton.IconOnly
    // Exactly as tall as expanded, with a larger icon (D-049).
    QCOMPARE(home->height(), expandedHeight);
    QVERIFY(QQmlProperty::read(home, QStringLiteral("icon.height")).toReal() > expandedIcon);
    QVERIFY(SettingsFile().value(QStringLiteral("view/sidebarCollapsed")).toBool());
    // Hide Sidebar still hides it whole, apart from collapsing.
    controller->setSidebarHidden(true);
    QTRY_VERIFY(!column->isVisible());
    controller->setSidebarHidden(false);
    QTRY_VERIFY(column->isVisible());
    QVERIFY(controller->sidebarCollapsed());

    // The triangle folds a group, and unfolds it; remembered.
    QQuickItem* foldCategories = visualItem(root, QStringLiteral("foldButton_categories"));
    QVERIFY(foldCategories);
    QVERIFY(visualItem(root, QStringLiteral("categoryEntry_login")));
    QVERIFY(QMetaObject::invokeMethod(foldCategories, "clicked"));
    QTRY_VERIFY(!visualItem(root, QStringLiteral("categoryEntry_login")));
    QVERIFY(SettingsFile().value(QStringLiteral("view/categoriesFolded")).toBool());
    QQuickItem* foldOthers = visualItem(root, QStringLiteral("foldButton_others"));
    QVERIFY(QMetaObject::invokeMethod(foldOthers, "clicked"));
    QTRY_VERIFY(!visualItem(root, QStringLiteral("trashEntry")));
    QVERIFY(QMetaObject::invokeMethod(foldOthers, "clicked"));
    QTRY_VERIFY(visualItem(root, QStringLiteral("trashEntry")));

    // Expanded again: its earlier width; the folding stays.
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_COMPARE(column->width(), expandedWidth);
    QCOMPARE(home->property("display").toInt(), 2); // AbstractButton.TextBesideIcon
    QVERIFY(!visualItem(root, QStringLiteral("categoryEntry_login")));
    QVERIFY(QMetaObject::invokeMethod(foldCategories, "clicked"));
    QTRY_VERIFY(visualItem(root, QStringLiteral("categoryEntry_login")));

    // The splitters catch the mouse over 8 pixels, not Fusion's 2.
    int handles = 0;
    std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
        if (QString(item->metaObject()->className()).startsWith(QLatin1String("QQuickItem")) &&
            qFuzzyCompare(item->implicitWidth(), 8.0) && item->childItems().size() == 1) {
            ++handles;
        }
        for (QQuickItem* child : item->childItems()) {
            walk(child);
        }
    };
    walk(root);
    QCOMPARE(handles, 2);
}

#else
void TestMenus::keyboard_shortcuts_are_set_on_their_page_and_kept()
{
    QVERIFY(open(QStringLiteral("keys")));
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QObject* catcher = window->findChild<QObject*>(QStringLiteral("keyCatcher"));
    if (!catcher) {
        catcher = settings->findChild<QObject*>(QStringLiteral("keyCatcher"));
    }
    QObject* ok = settings->findChild<QObject*>(QStringLiteral("okButton"));
    QObject* cancel = settings->findChild<QObject*>(QStringLiteral("cancelButton"));
    QObject* restore = settings->findChild<QObject*>(QStringLiteral("restoreDefaultsButton"));
    QVERIFY(catcher && ok && cancel && restore);
    QCOMPARE(controller->keysOf(QStringLiteral("sidebar")), QStringList{QStringLiteral("Ctrl+B")});
    const auto keyMap = [settings](const char* action) {
        return settings->property("keyMap").toMap().value(QString::fromLatin1(action)).toStringList();
    };

    // Help > Keyboard shortcuts… opens the page.
    QMetaObject::invokeMethod(action(QStringLiteral("shortcutsAction")), "trigger");
    QTRY_VERIFY(settings->property("visible").toBool());

    // The page is as tall as the topic shown, not the tallest one (D-047).
    auto* shortcutsPage = settings->findChild<QQuickItem*>(QStringLiteral("shortcutsPage"));
    QQuickItem* flick = shortcutsPage->parentItem();
    while (flick && !flick->property("contentHeight").isValid()) {
        flick = flick->parentItem();
    }
    QVERIFY(flick);
    QTRY_VERIFY(qAbs(flick->property("contentHeight").toReal() - shortcutsPage->implicitHeight()) <= 12);
    QObject* topicList = settings->findChild<QObject*>(QStringLiteral("topics"));
    topicList->setProperty("currentIndex", 0);
    QTRY_VERIFY(flick->property("contentHeight").toReal() < shortcutsPage->implicitHeight() - 100);
    topicList->setProperty("currentIndex", 6);
    // "no key" stands in the keys' own column, at their left edge.
    {
        QQuickItem* noKey = nullptr;
        QQuickItem* aKey = nullptr;
        std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
            if (item->objectName() == QLatin1String("noKey") && item->isVisible() && !noKey) noKey = item;
            if (item->objectName() == QLatin1String("shortcutKey") && item->isVisible() && !aKey) aKey = item;
            for (QQuickItem* child : item->childItems()) walk(child);
        };
        walk(shortcutsPage);
        QVERIFY(noKey && aKey);
        QCOMPARE(noKey->mapToScene(QPointF(0, 0)).x(), aKey->mapToScene(QPointF(0, 0)).x());
    }

    // A new key added to Lock vault, kept with OK and shown in the menu.
    QMetaObject::invokeMethod(catcher, "askFor", Q_ARG(QVariant, QStringLiteral("lock")), Q_ARG(QVariant, QStringLiteral("Lock vault")));
    QMetaObject::invokeMethod(catcher, "take", Q_ARG(QVariant, QStringLiteral("Ctrl+K")));
    QCOMPARE(keyMap("lock"), QStringList({QStringLiteral("Ctrl+L"), QStringLiteral("Ctrl+K")}));

    // While the box waits, a key is the new key, never a command (P29):
    // Shift+Ctrl+P (Copy Password) asks about the key in use; Return is
    // taken as a key and leaves the settings window open.
    {
        auto* box = qobject_cast<QWindow*>(catcher);
        QVERIFY(box);
        QMetaObject::invokeMethod(catcher, "askFor", Q_ARG(QVariant, QStringLiteral("lock")), Q_ARG(QVariant, QStringLiteral("Lock vault")));
        box->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), box);
        QTest::keyClick(box, Qt::Key_P, Qt::ShiftModifier | Qt::ControlModifier);
        QTRY_COMPARE(catcher->property("pendingKey").toString(), QStringLiteral("Shift+Ctrl+P"));
        QVERIFY(QMetaObject::invokeMethod(settings->findChild<QObject*>(QStringLiteral("keyTakenCancel")), "clicked"));
        QCOMPARE(keyMap("copyPassword"), QStringList{QStringLiteral("Shift+Ctrl+P")});
        QMetaObject::invokeMethod(catcher, "askFor", Q_ARG(QVariant, QStringLiteral("lock")), Q_ARG(QVariant, QStringLiteral("Lock vault")));
        box->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), box);
        QTest::keyClick(box, Qt::Key_Return);
        QTRY_VERIFY(keyMap("lock").contains(QStringLiteral("Return")));
        QVERIFY(settings->property("visible").toBool());
        QMetaObject::invokeMethod(settings, "takeKey", Q_ARG(QVariant, QStringLiteral("lock")), Q_ARG(QVariant, QStringLiteral("Return")));
    }

    // A key already taken: Cancel changes nothing, OK moves that one key.
    QMetaObject::invokeMethod(catcher, "askFor", Q_ARG(QVariant, QStringLiteral("copyOtp")), Q_ARG(QVariant, QStringLiteral("Copy OTP")));
    QMetaObject::invokeMethod(catcher, "take", Q_ARG(QVariant, QStringLiteral("Ctrl+K")));
    QCOMPARE(catcher->property("pendingKey").toString(), QStringLiteral("Ctrl+K"));
    QObject* keyTaken = settings->findChild<QObject*>(QStringLiteral("keyTaken"));
    QVERIFY(keyTaken && keyTaken->property("visible").toBool());
    QVERIFY(!catcher->property("visible").toBool());
    QVERIFY(keyTaken->property("informative").toString().contains(QStringLiteral("Ctrl+K")));
    // The arrows move the selection, which shows its edge (D-079).
    {
        auto* takenWindow = qobject_cast<QQuickWindow*>(keyTaken);
        auto* okButton = settings->findChild<QQuickItem*>(QStringLiteral("keyTakenOk"));
        auto* cancelButton = settings->findChild<QQuickItem*>(QStringLiteral("keyTakenCancel"));
        QTRY_COMPARE(QGuiApplication::focusWindow(), takenWindow);
        QTRY_VERIFY(cancelButton->hasActiveFocus() && cancelButton->property("visualFocus").toBool());
        QTest::keyClick(takenWindow, Qt::Key_Right);
        QTRY_VERIFY(okButton->hasActiveFocus() && okButton->property("visualFocus").toBool());
        QTest::keyClick(takenWindow, Qt::Key_Left);
        QTRY_VERIFY(cancelButton->hasActiveFocus());
    }
    QVERIFY(QMetaObject::invokeMethod(settings->findChild<QObject*>(QStringLiteral("keyTakenCancel")), "clicked"));
    QCOMPARE(keyMap("lock"), QStringList({QStringLiteral("Ctrl+L"), QStringLiteral("Ctrl+K")}));
    QMetaObject::invokeMethod(catcher, "askFor", Q_ARG(QVariant, QStringLiteral("copyOtp")), Q_ARG(QVariant, QStringLiteral("Copy OTP")));
    QMetaObject::invokeMethod(catcher, "take", Q_ARG(QVariant, QStringLiteral("Ctrl+K")));
    QVERIFY(QMetaObject::invokeMethod(settings->findChild<QObject*>(QStringLiteral("keyTakenOk")), "clicked"));
    QCOMPARE(keyMap("lock"), QStringList{QStringLiteral("Ctrl+L")});
    QCOMPARE(keyMap("copyOtp"), QStringList({QStringLiteral("Shift+Ctrl+T"), QStringLiteral("Ctrl+K")}));

    // A key removed: no key at all is allowed.
    QMetaObject::invokeMethod(settings, "takeKey", Q_ARG(QVariant, QStringLiteral("generatePassword")), Q_ARG(QVariant, QStringLiteral("Ctrl+G")));
    QVERIFY(keyMap("generatePassword").isEmpty());
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QCOMPARE(controller->keysOf(QStringLiteral("copyOtp")), QStringList({QStringLiteral("Shift+Ctrl+T"), QStringLiteral("Ctrl+K")}));
    QVERIFY(controller->keysOf(QStringLiteral("generatePassword")).isEmpty());
    QCOMPARE(action(QStringLiteral("generatePasswordAction"))->property("shownKeys").toString(), QString());

    // Cancel throws changes away.
    QVERIFY(QMetaObject::invokeMethod(settings, "showShortcuts"));
    QTRY_VERIFY(settings->property("visible").toBool());
    QMetaObject::invokeMethod(settings, "takeKey", Q_ARG(QVariant, QStringLiteral("lock")), Q_ARG(QVariant, QStringLiteral("Ctrl+L")));
    QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QCOMPARE(controller->keysOf(QStringLiteral("lock")), QStringList{QStringLiteral("Ctrl+L")});

    // A key the person set works: Ctrl+K copies the code; Ctrl+B collapses
    // the sidebar.
    QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, QString())));
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    quickWindow->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(quickWindow));
    window->findChild<QQuickItem*>(QStringLiteral("itemList"))->forceActiveFocus();
    QTest::keyClick(quickWindow, Qt::Key_B, Qt::ControlModifier);
    QTRY_VERIFY(controller->sidebarCollapsed());
    QTest::keyClick(quickWindow, Qt::Key_B, Qt::ControlModifier);
    QTRY_VERIFY(!controller->sidebarCollapsed());

    // Restore defaults puts back the keys Chrono Vault starts with.
    QVERIFY(QMetaObject::invokeMethod(settings, "showShortcuts"));
    QTRY_VERIFY(settings->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(restore, "clicked"));
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QCOMPARE(controller->keysOf(QStringLiteral("copyOtp")), QStringList{QStringLiteral("Shift+Ctrl+T")});
    QCOMPARE(controller->keysOf(QStringLiteral("generatePassword")), QStringList{QStringLiteral("Ctrl+G")});
}

// After a menu closes, the keyboard is back in the window's content, never
// kept by the menu bar, which would keep Esc for itself (D-052).
void TestMenus::after_a_menu_the_content_holds_the_keyboard()
{
    QVERIFY(open(QStringLiteral("focus")));
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    QVERIFY(quickWindow);
    QVERIFY(QTest::qWaitForWindowExposed(quickWindow));
    quickWindow->requestActivate();
    QTRY_VERIFY(quickWindow->isActive());
    QObject* viewMenu = window->findChild<QObject*>(QStringLiteral("viewMenu"));
    QVERIFY(viewMenu);
    QVERIFY(QMetaObject::invokeMethod(viewMenu, "open"));
    QTRY_VERIFY(viewMenu->property("opened").toBool());
    QVERIFY(QMetaObject::invokeMethod(viewMenu, "close"));
    QTRY_VERIFY(!viewMenu->property("visible").toBool());
    auto* list = window->findChild<QQuickItem*>(QStringLiteral("itemList"));
    QVERIFY(list);
    QTRY_VERIFY(list->hasActiveFocus());
    // Menus by keyboard still work: Alt+F opens File, and it stays open.
    QObject* fileMenu = window->findChild<QObject*>(QStringLiteral("fileMenu"));
    QVERIFY(fileMenu);
    QTest::keyClick(quickWindow, Qt::Key_F, Qt::AltModifier);
    QTRY_VERIFY(fileMenu->property("opened").toBool());
    QTest::qWait(200);
    QVERIFY(fileMenu->property("visible").toBool());
    QTest::keyClick(quickWindow, Qt::Key_Escape);
    QTRY_VERIFY(!fileMenu->property("visible").toBool());
    QTRY_VERIFY(list->hasActiveFocus());
    // Esc there, with "Use the Esc key to exit the program" ticked, closes
    // the window as its close button does.
    controller->setTrayAvailable(true);
    controller->setEscapeExits(true);
    QTest::keyClick(quickWindow, Qt::Key_Escape);
    QTRY_VERIFY(!quickWindow->isVisible());
    controller->setEscapeExits(false);
}

// Dragged narrower, the sidebar holds at 180 until the pointer passes 100,
// then collapses; dragged wider, it holds at its icons (64) until the pointer
// passes 144, then opens at 180: 80 pixels each way (the owner).
void TestMenus::the_sidebar_snaps_shut_and_open_when_its_splitter_is_dragged()
{
    QVERIFY(open(QStringLiteral("snap")));
    auto* quickWindow = qobject_cast<QQuickWindow*>(window);
    auto* column = window->findChild<QQuickItem*>(QStringLiteral("vaultColumn"));
    QVERIFY(column);
    if (controller->sidebarCollapsed()) {
        QVERIFY(QMetaObject::invokeMethod(visualItem(quickWindow->contentItem(), QStringLiteral("sidebarToggle")),
                                          "clicked"));
    }
    QTRY_VERIFY(column->width() > 180);
    const int y = 300;
    const auto moveTo = [&](int x) {
        QCursor::setPos(quickWindow->mapToGlobal(QPoint(x, y)));
        QTest::mouseMove(quickWindow, QPoint(x, y));
        QTest::qWait(60);
    };
    // Narrower: held at 180 at 120, shut past 100.
    int handle = qRound(column->width()) + 4;
    QCursor::setPos(quickWindow->mapToGlobal(QPoint(handle, y)));
    QTest::mousePress(quickWindow, Qt::LeftButton, Qt::NoModifier, QPoint(handle, y));
    moveTo(handle - 10);
    moveTo(120);
    QCOMPARE(qRound(column->width()), 180);
    QVERIFY(!controller->sidebarCollapsed());
    moveTo(90);
    QTRY_VERIFY(controller->sidebarCollapsed());
    QTest::mouseRelease(quickWindow, Qt::LeftButton, Qt::NoModifier, QPoint(90, y));
    QTRY_COMPARE(qRound(column->width()), 64);
    // Wider: held at its icons at 130, open at 180 past 144.
    handle = qRound(column->width()) + 4;
    QCursor::setPos(quickWindow->mapToGlobal(QPoint(handle, y)));
    QTest::mousePress(quickWindow, Qt::LeftButton, Qt::NoModifier, QPoint(handle, y));
    moveTo(handle + 10);
    moveTo(130);
    QVERIFY(controller->sidebarCollapsed());
    QCOMPARE(qRound(column->width()), 64);
    moveTo(155);
    QTRY_VERIFY(!controller->sidebarCollapsed());
    QTest::mouseRelease(quickWindow, Qt::LeftButton, Qt::NoModifier, QPoint(155, y));
    QTRY_COMPARE(qRound(column->width()), 180);
}

// Settings > Categories: Edit… on every category, then Reset for a built-in one and
// Delete for the person's own; in the sidebar a right-click offers Edit…,
// which opens the same dialog (the owner, D-056).
void TestMenus::every_category_has_edit_and_reset_or_delete_and_a_right_click_menu()
{
    QVERIFY(open(QStringLiteral("categories")));
    const QString cars = controller->saveCategory(QString(), QStringLiteral("Cars"));
    QVERIFY(!cars.isEmpty());
    auto* settings = qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QStringLiteral("settingsWindow")));
    QVERIFY(settings && QMetaObject::invokeMethod(settings, "showWindow"));
    settings->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 3);
    QQuickItem* root = settings->contentItem();
    QQuickItem* edit = nullptr;
    QTRY_VERIFY((edit = visualItem(root, QStringLiteral("categoryEdit_login"))) != nullptr);
    QVERIFY(visualItem(root, QLatin1String("categoryEdit_") + cars));
    QCOMPARE(visualItem(root, QStringLiteral("categoryRemove_login"))->property("text").toString(), QStringLiteral("Reset"));
    QCOMPARE(visualItem(root, QLatin1String("categoryRemove_") + cars)->property("text").toString(),
             QStringLiteral("Delete"));
    // No dialog waits hidden until Edit… is used.
    QVERIFY(!settings->findChild<QObject*>(QStringLiteral("categoryEditor")));
    QVERIFY(QMetaObject::invokeMethod(edit, "clicked"));
    QObject* editor = nullptr;
    QTRY_VERIFY((editor = settings->findChild<QObject*>(QStringLiteral("categoryEditor"))) != nullptr);
    QTRY_VERIFY(editor->property("visible").toBool());
    QCOMPARE(editor->findChild<QObject*>(QStringLiteral("categoryNameField"))->property("text").toString(),
             QStringLiteral("Login"));
    QVERIFY(QMetaObject::invokeMethod(editor, "close"));
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));

    // The sidebar: a right-click menu with Edit… on each category.
    QTRY_VERIFY(visualItem(qobject_cast<QQuickWindow*>(window)->contentItem(), QStringLiteral("categoryEntry_login")));
    QVERIFY(window->findChild<QObject*>(QStringLiteral("categoryMenu_login")) ||
            visualItem(qobject_cast<QQuickWindow*>(window)->contentItem(), QStringLiteral("categoryEntry_login"))
                ->findChild<QObject*>(QStringLiteral("categoryMenu_login")));
}

#endif
QTEST_MAIN(TestMenus)
#include "test_menus.moc"
