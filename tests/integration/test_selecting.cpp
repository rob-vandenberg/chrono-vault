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

#include "intuitive_fixture.h"

// --- Version ------------------------------------------------------------
// test_selecting.cpp 0.5.83

// --- Version history ----------------------------------------------------
// v0.5.83: Initial release (D-063)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// The focused item and the selected ones, the menus, empty lists and counts (D-063).
class TestSelecting : public IntuitiveFixture
{
    Q_OBJECT

private slots:
    void several_items_are_chosen_and_acted_on_together()
    {
        const auto focused = [this]() { return page->property("chosenItemId").toString(); };
        const auto bars = [this]() {
            int found = 0;
            for (int at = 0; at < 8; ++at) {
                found += seen(row(at), QStringLiteral("chosenBar")) ? 1 : 0;
            }
            return found;
        };
        click(row(2));
        QTRY_COMPARE(focused(), idAt(2));
        // Ctrl+click: the selection grows, the focus and the right side stay.
        click(row(4), Qt::ControlModifier);
        QTRY_COMPARE(chosen(), (QStringList{idAt(2), idAt(4)}));
        QCOMPARE(focused(), idAt(2));
        QCOMPARE(current(), 2);
        QCOMPARE(bars(), 1);
        QVERIFY(seen(row(2), QStringLiteral("chosenBar")));
        click(row(2), Qt::ControlModifier); // the focused one no longer selected, still focused
        QTRY_COMPARE(chosen(), QStringList{idAt(4)});
        QCOMPARE(focused(), idAt(2));
        QCOMPARE(bars(), 1);
        // Shift+click: from the focused one, which stays, also back again.
        click(row(5), Qt::ShiftModifier);
        QTRY_COMPARE(chosen(), (QStringList{idAt(2), idAt(3), idAt(4), idAt(5)}));
        click(row(0), Qt::ShiftModifier);
        QTRY_COMPARE(chosen(), (QStringList{idAt(0), idAt(1), idAt(2)}));
        QCOMPARE(focused(), idAt(2));
        // Shift with keys: grows, shrinks and crosses over.
        click(row(2));
        QTRY_COMPARE(chosen(), QStringList{idAt(2)});
        key(Qt::Key_Down, Qt::ShiftModifier);
        key(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(chosen(), (QStringList{idAt(2), idAt(3), idAt(4)}));
        key(Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(chosen(), (QStringList{idAt(2), idAt(3)}));
        key(Qt::Key_Up, Qt::ShiftModifier);
        key(Qt::Key_Up, Qt::ShiftModifier);
        QCOMPARE(chosen(), (QStringList{idAt(1), idAt(2)}));
        key(Qt::Key_End, Qt::ShiftModifier);
        QCOMPARE(chosen().size(), 29);
        QTRY_VERIFY(inView(30));
        key(Qt::Key_Home, Qt::ShiftModifier);
        QCOMPARE(chosen().size(), 3);
        QCOMPARE(focused(), idAt(2));
        key(Qt::Key_Down); // one step from the focused one, that one alone
        QCOMPARE(chosen(), QStringList{idAt(3)});
        key(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(chosen().size(), 31);
        QCOMPARE(focused(), idAt(3));
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

    void the_item_menu_acts_on_the_selected_items()
    {
        const auto action = [this](const char* name) { return window->findChild<QObject*>(QString::fromLatin1(name)); };
        const auto enabled = [&action](const char* name) { return action(name)->property("enabled").toBool(); };
        QVERIFY(!enabled("favoriteAction") && !enabled("moveToTrashAction") && !enabled("categoryAction"));
        click(row(1));
        click(row(3), Qt::ControlModifier);
        QTRY_COMPARE(chosen().size(), 2);
        const QStringList two = chosen();
        // Favorites and Change Category: at once, on both, nothing else moved.
        QMetaObject::invokeMethod(action("favoriteAction"), "trigger");
        for (const QString& id : two) {
            QVERIFY(controller->item(id).value(QStringLiteral("favourite")).toBool());
        }
        QCOMPARE(action("favoriteAction")->property("text").toString(), QStringLiteral("Remove from Favorites"));
        QCOMPARE(chosen(), two);
        QVERIFY(QMetaObject::invokeMethod(action("categoryAction"), "chosen", Q_ARG(QString, QStringLiteral("finance"))));
        for (const QString& id : two) {
            QCOMPARE(controller->item(id).value(QStringLiteral("categoryId")).toString(), QStringLiteral("finance"));
            QCOMPARE(controller->item(id).value(QStringLiteral("templateId")).toString(), QStringLiteral("login"));
        }
        QCOMPARE(chosen(), two);
        controller->setCategoryOf(two, QStringLiteral("no such category"));
        QCOMPARE(controller->item(two.at(0)).value(QStringLiteral("categoryId")).toString(), QStringLiteral("finance"));
        // Archive asks for two; Unarchive and the Trash from the menu.
        QQuickWindow* question = dialog("placeQuestion");
        QMetaObject::invokeMethod(action("archiveAction"), "trigger");
        QVERIFY(comesUp(question));
        QTest::keyClick(question, Qt::Key_Return);
        QTRY_COMPARE(controller->items()->rowCount(), 29);
        controller->setFilter(QStringLiteral("archive"));
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QTRY_VERIFY(row(1));
        click(row(0));
        QTRY_COMPARE(action("archiveAction")->property("text").toString(), QStringLiteral("Unarchive"));
        QMetaObject::invokeMethod(action("archiveAction"), "trigger"); // one: not asked
        QTRY_COMPARE(controller->items()->rowCount(), 1);
        controller->setFilter(QString());
        controller->placeItems({two.at(0)}, QStringLiteral("trashed"), true);
        controller->setFilter(QStringLiteral("trash"));
        QTRY_VERIFY(row(0));
        click(row(0));
        QTRY_VERIFY(enabled("restoreAction") && enabled("deletePermanentlyAction"));
        QVERIFY(!enabled("moveToTrashAction") && !enabled("favoriteAction") && !enabled("categoryAction"));
        QMetaObject::invokeMethod(action("restoreAction"), "trigger");
        QTRY_COMPARE(controller->trashedItems().size(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }

    void a_right_click_offers_only_what_is_possible()
    {
        QObject* menu = window->findChild<QObject*>(QStringLiteral("itemListMenu"));
        const auto offered = [menu]() {
            QStringList texts;
            for (int at = 0; at < menu->property("count").toInt(); ++at) {
                QQuickItem* entry = nullptr;
                QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem*, entry), Q_ARG(int, at));
                if (entry->isVisible() && entry->height() > 0) {
                    texts.append(entry->property("text").toString());
                }
            }
            return texts;
        };
        click(row(1));
        click(row(3), Qt::ControlModifier);
        QTRY_COMPARE(chosen().size(), 2);
        click(row(3), {}, Qt::RightButton);
        QTRY_VERIFY(menu->property("opened").toBool());
        QCOMPARE(chosen().size(), 2);
        QCOMPARE(offered(), (QStringList{QStringLiteral("Add to Favorites"), QStringLiteral("Archive"),
                                         QStringLiteral("Change Category"), QStringLiteral("Move to Trash")}));
        QMetaObject::invokeMethod(menu, "close");
        QTRY_VERIFY(!menu->property("visible").toBool());
        controller->placeItems({idAt(0)}, QStringLiteral("trashed"), true);
        controller->setFilter(QStringLiteral("trash"));
        QTRY_COMPARE(controller->items()->rowCount(), 1);
        QTRY_VERIFY(row(0));
        click(row(0), {}, Qt::RightButton);
        QTRY_VERIFY(menu->property("opened").toBool());
        QCOMPARE(offered(), (QStringList{QStringLiteral("Restore"), QStringLiteral("Delete Permanently…")}));
        QMetaObject::invokeMethod(menu, "close");
        QTRY_VERIFY(!menu->property("visible").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }

    void an_item_not_in_the_list_is_not_selected_and_the_counts_show()
    {
        QVERIFY(part("vaultOverview"));
        click(row(1));
        QTRY_VERIFY(!part("vaultOverview"));
        click(part("categoryEntry_finance"));
        QTRY_VERIFY(part("emptyList"));
        QCOMPARE(chosen(), QStringList{});
        QCOMPARE(page->property("chosenItemId").toString(), QString());
        QCOMPARE(part("emptyHeadline")->property("text").toString(), QStringLiteral("No items in Finance"));
        QVERIFY(part("emptyVault") && part("emptyAdvice"));
        QVERIFY(part("vaultOverview"));
        QCOMPARE(part("overviewName")->property("text").toString(), controller->vaultName());
        QCOMPARE(part("countItems")->property("count").toInt(), 31);
        QCOMPARE(part("countTrash")->property("count").toInt(), 0);
        click(part("countTrash"));
        QTRY_COMPARE(controller->filter(), QStringLiteral("trash"));
        QCOMPARE(part("emptyHeadline")->property("text").toString(), QStringLiteral("The Trash is empty"));
        QVERIFY(!part("emptyAdvice"));
        click(part("countItems"));
        QTRY_COMPARE(controller->filter(), QString());
        controller->setSearchText(QStringLiteral("xyz"));
        QTRY_COMPARE(part("emptyHeadline")->property("text").toString(), QStringLiteral("Nothing found for “xyz”"));
        QVERIFY(!part("emptyVault"));
        controller->setSearchText(QString());
        // Still in the list: still selected.
        click(row(1));
        QTRY_COMPARE(chosen().size(), 1);
        click(part("categoryEntry_login"));
        QTRY_COMPARE(controller->filter(), QStringLiteral("category:login"));
        QCOMPARE(chosen(), QStringList{idAt(1)});
        controller->setFilter(QStringLiteral("trash"));
        QTRY_VERIFY(part("countCategories"));
        QQuickWindow* settings = dialog("settingsWindow");
        QVERIFY(settings);
        click(part("countCategories"));
        QVERIFY(comesUp(settings));
        QTRY_VERIFY(seen(settings->contentItem(), QStringLiteral("customizePage")));
        settings->close();
    }

    void tags_are_renamed_and_removed_in_customize()
    {
        QVariantMap one = controller->item(idAt(0));
        one.insert(QStringLiteral("tags"), QStringList{QStringLiteral("Work")});
        QVERIFY(controller->saveItem(one));
        click(part("countTags"));
        QQuickWindow* settings = dialog("settingsWindow");
        QVERIFY(comesUp(settings));
        const auto there = [settings](const char* name) { return seen(settings->contentItem(), QString::fromLatin1(name)); };
        QTRY_VERIFY(there("tagRename_Work"));
        QMetaObject::invokeMethod(there("tagRename_Work"), "clicked");
        QQuickWindow* naming = qobject_cast<QQuickWindow*>(settings->findChild<QObject*>(QStringLiteral("customizeTagDialog")));
        QVERIFY(comesUp(naming));
        seen(naming->contentItem(), QStringLiteral("tagNameField"))->setProperty("text", QStringLiteral("Home"));
        QTest::keyClick(naming, Qt::Key_Return);
        QTRY_VERIFY(there("tagRemove_Home"));
        QMetaObject::invokeMethod(there("tagRemove_Home"), "clicked");
        QTRY_COMPARE(controller->tags().size(), 0);
        settings->close();
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }
};

QTEST_MAIN(TestSelecting)
#include "test_selecting.moc"
