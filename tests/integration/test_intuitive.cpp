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
#include "settings_files.h"
#include "vault_controller.h"
#include "vault_file.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_intuitive.cpp 0.5.82

// --- Version history ----------------------------------------------------
// v0.5.82: Initial release (D-062)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

QQuickItem* seen(QQuickItem* root, const QString& name)
{
    if (!root) {
        return nullptr;
    }
    if (root->objectName() == name && root->isVisible()) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = seen(child, name)) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

// What a person guesses is what happens (D-062): mouse and keys only.
class TestIntuitive : public QObject
{
    Q_OBJECT

    QTemporaryDir folder;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow* window = nullptr;
    VaultController* controller = nullptr;
    QObject* page = nullptr;
    QQuickItem* list = nullptr;
    QList<QQmlError> problems;

    QQuickItem* row(int index)
    {
        QQuickItem* found = nullptr;
        QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, found), Q_ARG(int, index));
        return found;
    }
    void click(QQuickItem* item, Qt::KeyboardModifiers held = {}, Qt::MouseButton with = Qt::LeftButton)
    {
        QVERIFY(item);
        QTest::mouseClick(item->window(), with, held, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    }
    void key(int code, Qt::KeyboardModifiers held = {}) { QTest::keyClick(window, Qt::Key(code), held); }
    void type(const QString& text)
    {
        for (const QChar letter : text) {
            QTest::keyClick(window, letter.toLatin1());
        }
    }
    int current() const { return list->property("currentIndex").toInt(); }
    QStringList chosen() const { return page->property("selectedIds").toStringList(); }
    QQuickItem* part(const char* name) { return seen(window->contentItem(), QString::fromLatin1(name)); }
    QQuickWindow* dialog(const char* name) const
    {
        return qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QString::fromLatin1(name)));
    }
    bool comesUp(QQuickWindow* one) const
    {
        return one && QTest::qWaitFor([one]() { return one->isVisible() && QGuiApplication::focusWindow() == one; }, 5000);
    }
    bool inView(int index)
    {
        QQuickItem* one = row(index);
        if (!one) {
            return false;
        }
        const double y = one->mapToItem(list, QPointF(0, 0)).y();
        return y >= -1 && y + one->height() <= list->height() + 1;
    }
    QString idAt(int index) const { return controller->items()->idAt(index); }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_intuitive"));
        applyStyle();
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        engine = std::make_unique<QQmlApplicationEngine>();
        problems.clear();
        connect(engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError>& some) { problems += some; });
        engine->loadFromModule("ChronoVault", "Main");
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst());
        showMainWindow(window, false, false);
        controller = engine->singletonInstance<VaultController*>("ChronoVault", "VaultController");
        controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        const QString file = folder.filePath(QString::fromLatin1(QTest::currentTestFunction()) + QStringLiteral(".chronovault"));
        controller->createVault(QUrl::fromLocalFile(file), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QTRY_VERIFY(controller->isIdle());
        const OpenResult opened = openVaultFile(file, "pass");
        VaultContents contents = contentsFromJson(opened.contents).contents;
        for (int number = 0; number <= 30; ++number) {
            Item item;
            item.id = QUuid::createUuid();
            item.title = number == 30 ? QStringLiteral("Mill") : QStringLiteral("Item %1").arg(number, 2, 10, QLatin1Char('0'));
            item.created = item.modified = QDateTime::currentDateTimeUtc();
            contents.items.append(item);
        }
        QCOMPARE(saveVaultFile(file, contentsToJson(contents), opened.key), SaveStatus::Saved);
        controller->lock();
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
        list = window->findChild<QQuickItem*>(QStringLiteral("itemList"));
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QTRY_VERIFY(row(3));
    }
    void cleanup()
    {
        QString said;
        for (const QQmlError& problem : std::as_const(problems)) {
            said += problem.toString() + QLatin1Char('\n');
        }
        engine.reset();
        SettingsFile().clear();
        VaultsFile().clear();
        QVERIFY2(said.isEmpty(), qPrintable(said));
    }

    void a_click_gives_the_list_the_keyboard_and_every_key_scrolls_to_its_item()
    {
        click(part("searchField")); // the keyboard elsewhere first
        click(row(2));
        QTRY_COMPARE(current(), 2);
        key(Qt::Key_Down);
        QCOMPARE(current(), 3);
        QCOMPARE(page->property("chosenItemId").toString(), idAt(3));
        key(Qt::Key_End);
        QCOMPARE(current(), 30);
        QTRY_VERIFY(inView(30));
        key(Qt::Key_Home);
        QCOMPARE(current(), 0);
        QTRY_VERIFY(inView(0));
        key(Qt::Key_PageDown, Qt::KeypadModifier);
        QVERIFY(current() > 3);
        QTRY_VERIFY(inView(current()));
        key(Qt::Key_PageUp);
        QCOMPARE(current(), 0);
        key(Qt::Key_M); // a letter jumps
        QCOMPARE(current(), 30);
        QTRY_VERIFY(inView(30));
    }

    void delete_asks_and_chooses_the_next_item()
    {
        click(row(2));
        QTRY_COMPARE(current(), 2);
        const QString gone = idAt(2);
        const QString next = idAt(3);
        QQuickWindow* question = dialog("placeQuestion");
        key(Qt::Key_Delete);
        QVERIFY(comesUp(question));
        QTest::keyClick(question, Qt::Key_Escape); // Esc is No
        QTRY_VERIFY(!question->isVisible());
        QVERIFY(!controller->item(gone).value(QStringLiteral("trashed")).toBool());
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        key(Qt::Key_Backspace);
        QVERIFY(comesUp(question));
        QTest::keyClick(question, Qt::Key_Return); // Enter is Yes
        QTRY_VERIFY(controller->item(gone).value(QStringLiteral("trashed")).toBool());
        QTRY_COMPARE(page->property("chosenItemId").toString(), next);
        QVERIFY(controller->message().contains(QStringLiteral("Trash")));

        // In the Trash, Enter never deletes for good; Yes does.
        controller->setFilter(QStringLiteral("trash"));
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QTRY_VERIFY(row(0));
        click(row(0));
        key(Qt::Key_Delete);
        QQuickWindow* lasting = dialog("foreverQuestion");
        QVERIFY(comesUp(lasting));
        QTest::keyClick(lasting, Qt::Key_Return);
        QTRY_VERIFY(!lasting->isVisible());
        QVERIFY(!controller->item(gone).isEmpty() && controller->item(gone).contains(QStringLiteral("id")));
        QVERIFY(QMetaObject::invokeMethod(lasting, "ask", Q_ARG(QVariant, QStringList{gone}), Q_ARG(QVariant, QString())));
        QVERIFY(QMetaObject::invokeMethod(lasting, "accept"));
        QTRY_VERIFY(!controller->item(gone).contains(QStringLiteral("id")));
    }

    void several_items_are_chosen_and_acted_on_together()
    {
        click(row(1));
        click(row(3), Qt::ControlModifier);
        QTRY_COMPARE(chosen(), (QStringList{idAt(1), idAt(3)}));
        QVERIFY(part("severalLabel")->property("text").toString().contains(QLatin1Char('2')));
        click(row(3), Qt::ControlModifier); // taken away: the mark on the one left
        QTRY_COMPARE(chosen(), QStringList{idAt(1)});
        QCOMPARE(current(), 1);
        QCOMPARE(page->property("chosenItemId").toString(), idAt(1));
        click(row(1), Qt::ControlModifier);
        QTRY_COMPARE(current(), -1);
        click(row(1));
        click(row(3), Qt::ControlModifier);
        QTRY_COMPARE(chosen().size(), 2);
        click(row(5), Qt::ShiftModifier); // from the last one
        QTRY_COMPARE(chosen().size(), 3);
        key(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(chosen().size(), 4);
        key(Qt::Key_Down); // one again
        QCOMPARE(chosen().size(), 1);
        key(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(chosen().size(), 31);
        click(row(0));
        click(row(2), Qt::ShiftModifier);
        QTRY_COMPARE(chosen().size(), 3);
        QQuickWindow* question = dialog("placeQuestion");
        key(Qt::Key_Delete);
        QVERIFY(comesUp(question));
        QTest::keyClick(question, Qt::Key_Return);
        QTRY_COMPARE(controller->trashedItems().size(), 3);
        QTRY_COMPARE(controller->items()->rowCount(), 28);

        // The Trash: all restored at once, after a question; and emptied.
        controller->setFilter(QStringLiteral("trash"));
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QTRY_VERIFY(part("restoreAllButton"));
        click(part("restoreAllButton"));
        QVERIFY(comesUp(question));
        QTest::keyClick(question, Qt::Key_Return);
        QTRY_COMPARE(controller->trashedItems().size(), 0);
        QVERIFY(!part("emptyTrashButton")->isEnabled());
        controller->placeItems({idAt(0)}, QStringLiteral("trashed"), true);
        controller->setFilter(QString());
        controller->placeItems({idAt(0), idAt(1)}, QStringLiteral("trashed"), true);
        controller->setFilter(QStringLiteral("trash"));
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QTRY_VERIFY(part("emptyTrashButton") && part("emptyTrashButton")->isEnabled());
        click(part("emptyTrashButton"));
        QQuickWindow* lasting = dialog("foreverQuestion");
        QVERIFY(comesUp(lasting));
        QVERIFY(QMetaObject::invokeMethod(lasting, "accept"));
        QTRY_COMPARE(controller->trashedItems().size(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }

    void search_and_editor_by_keys()
    {
        click(row(4));
        QTRY_COMPARE(current(), 4);
        key(Qt::Key_F, Qt::ControlModifier);
        QQuickItem* search = part("searchField");
        QTRY_VERIFY(search->hasActiveFocus());
        type(QStringLiteral("mill"));
        QTRY_COMPARE(controller->items()->rowCount(), 1);
        QCOMPARE(page->property("chosenItemId").toString(), QString()); // not in the results
        key(Qt::Key_Down);
        QTRY_VERIFY(list->hasActiveFocus());
        QCOMPARE(page->property("chosenItemId").toString(), idAt(0));
        key(Qt::Key_F, Qt::ControlModifier);
        key(Qt::Key_Escape);
        QTRY_COMPARE(controller->items()->rowCount(), 31);
        QTRY_VERIFY(list->hasActiveFocus());

        // Enter opens the editor on its title; Esc cancels, Ctrl+S saves.
        key(Qt::Key_Return);
        QTRY_VERIFY(page->property("editing").toBool());
        QTRY_VERIFY(part("titleField")->hasActiveFocus());
        key(Qt::Key_Escape);
        QTRY_VERIFY(!page->property("editing").toBool());
        QTRY_VERIFY(list->hasActiveFocus());
        QTest::mouseDClick(window, Qt::LeftButton, {}, row(current())->mapToScene(QPointF(20, 10)).toPoint());
        QTRY_VERIFY(page->property("editing").toBool());
        key(Qt::Key_End);
        type(QStringLiteral("er"));
        key(Qt::Key_S, Qt::ControlModifier);
        QTRY_VERIFY(!page->property("editing").toBool());
        QCOMPARE(controller->item(page->property("chosenItemId").toString()).value(QStringLiteral("title")).toString(), QStringLiteral("Miller"));
        // Changed: Esc asks.
        key(Qt::Key_Return);
        QTRY_VERIFY(page->property("editing").toBool());
        type(QStringLiteral("x"));
        key(Qt::Key_Escape);
        QVERIFY(comesUp(dialog("unsavedDialog")));
        QVERIFY(QMetaObject::invokeMethod(dialog("unsavedDialog"), "discard"));
        QTRY_VERIFY(!page->property("editing").toBool());
    }

    void the_sidebar_by_keys_and_a_value_copied_by_double_click()
    {
        click(part("homeEntry"));
        key(Qt::Key_Down);
        QTRY_COMPARE(controller->filter(), QStringLiteral("favourites"));
        key(Qt::Key_End);
        QTRY_COMPARE(controller->filter(), QStringLiteral("trash"));
        key(Qt::Key_Up);
        QTRY_COMPARE(controller->filter(), QStringLiteral("archive"));
        key(Qt::Key_Home);
        QTRY_COMPARE(controller->filter(), QString());

        QVariantMap item = controller->newLogin();
        item.insert(QStringLiteral("title"), QStringLiteral("AAA"));
        QVariantList fields = item.value(QStringLiteral("fields")).toList();
        QVariantMap user = fields[0].toMap();
        user.insert(QStringLiteral("value"), QStringLiteral("rob"));
        fields[0] = user;
        item.insert(QStringLiteral("fields"), fields);
        QVERIFY(controller->saveItem(item));
        QTRY_COMPARE(controller->items()->rowCount(), 32);
        QTRY_VERIFY(row(0));
        click(row(0));
        QTRY_VERIFY(part("fieldValue"));
        QQuickItem* value = part("fieldValue");
        QTest::mouseDClick(window, Qt::LeftButton, {}, value->mapToScene(QPointF(10, value->height() / 2)).toPoint());
        QTRY_COMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("rob"));
        // A right click on an item: its menu.
        click(row(1), {}, Qt::RightButton);
        QObject* menu = window->findChild<QObject*>(QStringLiteral("itemListMenu"));
        QTRY_VERIFY(menu && menu->property("visible").toBool());
        QCOMPARE(page->property("chosenItemId").toString(), idAt(1));
        QMetaObject::invokeMethod(menu, "close");
    }

    void settings_vaults_has_its_own_buttons_and_the_crown()
    {
        auto* settings = dialog("settingsWindow");
        QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
        QVERIFY(comesUp(settings));
        const auto here = [settings](const char* name) { return seen(settings->contentItem(), QString::fromLatin1(name)); };
        QVERIFY(here("restoreDefaultsButton") && !here("createVaultButton") && !here("openVaultButton"));
        QObject* topics = settings->findChild<QObject*>(QStringLiteral("topics"));
        QVERIFY(topics);
        topics->setProperty("currentIndex", 5);
        QTRY_VERIFY(here("createVaultButton") && here("openVaultButton") && !here("restoreDefaultsButton"));
        QTRY_VERIFY(here("primaryCrown_settings_vaults_has_its_own_buttons_and_the_crown"));
        QTRY_COMPARE(here("createVaultButton")->mapToScene(QPointF()).x(), here("vaultsPage")->mapToScene(QPointF()).x());
        click(here("createVaultButton"));
        auto* storage = dialog("storageWindow");
        QVERIFY(comesUp(storage));
        QCOMPARE(storage->transientParent(), settings); // in front of it
        QVERIFY(settings->isVisible());
        QTest::keyClick(storage, Qt::Key_Escape);
        QTRY_VERIFY(!storage->isVisible());
        settings->close();
    }

    // Enter never throws away or deletes for good, whichever question asks.
    void enter_never_loses_anything()
    {
        for (const char* name : {"quitDialog", "leaveVaultDialog", "foreverQuestion", "deleteIconQuestion", "forgetWindow",
                                 "disconnectWindow"}) {
            QQuickWindow* one = dialog(name);
            QVERIFY2(one, name);
            auto* target = one->property("enterTarget").value<QQuickItem*>();
            QVERIFY2(target, name);
            const QString text = target->property("text").toString();
            QVERIFY2(text == QLatin1String("Cancel") || text == QLatin1String("No"), qPrintable(QLatin1String(name) + text));
        }
    }

    // Esc in a dialog made inside another closes only that one.
    void esc_closes_the_active_dialog_only()
    {
        auto* outer = dialog("syncConflictsWindow");
        auto* inner = dialog("retryPasswordWindow");
        QVERIFY(QMetaObject::invokeMethod(outer, "open"));
        QVERIFY(comesUp(outer));
        QVERIFY(QMetaObject::invokeMethod(inner, "open"));
        QVERIFY(comesUp(inner));
        QTest::keyClick(inner, Qt::Key_Escape);
        QTRY_VERIFY(!inner->isVisible());
        QVERIFY(outer->isVisible());
        outer->requestActivate();
        QVERIFY(comesUp(outer));
        QTest::keyClick(outer, Qt::Key_Escape);
        QTRY_VERIFY(!outer->isVisible());
    }
};

QTEST_MAIN(TestIntuitive)
#include "test_intuitive.moc"
