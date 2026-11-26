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
// test_intuitive.cpp 0.7.111

// --- Version history ----------------------------------------------------
// v0.7.111: A question fits its text; Enter, Esc, Y and N as the owner set them (D-079)
// v0.6.88: The settings' topics in their new order (D-066)
// v0.5.83: The fixture, and the tests of several items, in files of their own (D-063)
// v0.5.82: Initial release (D-062)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// What a person guesses is what happens (D-062): mouse and keys only.
class TestIntuitive : public IntuitiveFixture
{
    Q_OBJECT

private slots:
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
        QTest::keyClick(question, Qt::Key_N); // N is No (Esc only presses Cancel, D-079)
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
        topics->setProperty("currentIndex", 2);
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
            const QString text = target->property("text").toString().remove(QLatin1Char('&'));
            QVERIFY2(text == QLatin1String("Cancel") || text == QLatin1String("No"), qPrintable(QLatin1String(name) + text));
        }
    }

    // A question is as tall as its text; its selected button shows its edge;
    // Enter presses only that one; Y and N answer; Esc presses Cancel, or
    // Close where there is no field, and nothing else (the owner, D-079).
    void a_question_fits_its_text_and_its_keys_are_plain()
    {
        QQuickWindow* question = dialog("placeQuestion");
        QSignalSpy yes(question, SIGNAL(accepted()));
        QSignalSpy no(question, SIGNAL(rejected()));
        const auto ask = [&]() {
            QVERIFY(QMetaObject::invokeMethod(question, "ask", Q_ARG(QVariant, QStringLiteral("Move 3 items to the Trash?")),
                                              Q_ARG(QVariant, QVariant())));
            QVERIFY(comesUp(question));
        };
        ask();
        QCOMPARE(question->minimumHeight(), question->maximumHeight());
        QCOMPARE(question->minimumWidth(), question->maximumWidth());
        QCOMPARE(question->height(), question->minimumHeight());
        auto* yesButton = question->property("enterTarget").value<QQuickItem*>();
        QVERIFY(yesButton);
        QCOMPARE(yesButton->property("text").toString(), QStringLiteral("&Yes"));
        QTRY_VERIFY(yesButton->property("visualFocus").toBool());
        QTest::keyClick(question, Qt::Key_Escape); // no Cancel: nothing
        QTest::qWait(50);
        QVERIFY(question->isVisible());
        QCOMPARE(yes.size() + no.size(), 0);
        QTest::keyClick(question, Qt::Key_Right); // the selection moves to No, with its edge
        auto* noButton = qobject_cast<QQuickItem*>(question->activeFocusItem());
        QVERIFY(noButton && noButton != yesButton);
        QVERIFY(noButton->property("visualFocus").toBool());
        QTest::keyClick(question, Qt::Key_Return); // the selected one
        QTRY_COMPARE(no.size(), 1);
        QCOMPARE(yes.size(), 0);
        ask();
        QTest::keyClick(question, Qt::Key_Y);
        QTRY_COMPARE(yes.size(), 1);
        ask();
        QTest::keyClick(question, Qt::Key_N);
        QTRY_COMPARE(no.size(), 2);

        // With Cancel, Esc presses it; with only Close and no field, Close.
        for (const char* name : {"certificateWindow", "updatedWindow"}) {
            QQuickWindow* one = dialog(name);
            QVERIFY2(one, name);
            QSignalSpy closed(one, SIGNAL(rejected()));
            QVERIFY(QMetaObject::invokeMethod(one, "open"));
            QVERIFY(comesUp(one));
            QTest::keyClick(one, Qt::Key_Escape);
            QTRY_VERIFY2(!one->isVisible(), name);
            QCOMPARE(closed.size(), 1);
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
