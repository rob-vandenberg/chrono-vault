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
#include "item_order.h"
#include "program_scale.h"

#include <QDesktopServices>
#include <QScopeGuard>

// --- Version ------------------------------------------------------------
// test_intuitive.cpp 0.8.121

// --- Version history ----------------------------------------------------
// v0.8.121: Yes without &, its letter Y; letter keys quickly one after another, no typed word (D-079, D-081)
// v0.8.119: No minimize button, the title, the top bar, General's order (test_intuitive_bars); Sort By and the letter keys (test_intuitive_sorting) (D-079, D-074, D-017, D-081)
// v0.7.111: A question fits its text; Enter, Esc, Y and N as the owner set them (D-079)
// v0.6.88: The settings' topics in their new order (D-066)
// v0.5.83: The fixture, and the tests of several items, in files of their own (D-063)
// v0.5.82: Initial release (D-062)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

#if defined(CHRONO_VAULT_INTUITIVE_SORTING)
// Catches a website being opened, so no browser starts.
class Opener : public QObject
{
    Q_OBJECT

public:
    explicit Opener(QStringList* into) : seen(into) {}
    Q_INVOKABLE void open(const QUrl& address) { seen->append(address.toString()); }

private:
    QStringList* seen;
};
#endif

// What a person guesses is what happens (D-062): mouse and keys only.
class TestIntuitive : public IntuitiveFixture
{
    Q_OBJECT

private slots:
#if !defined(CHRONO_VAULT_INTUITIVE_BARS) && !defined(CHRONO_VAULT_INTUITIVE_SORTING)
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
#elif defined(CHRONO_VAULT_INTUITIVE_BARS)
    // The questions' keys, the title bars, the top bar and Settings' order,
    // apart so each binary stays under 20 seconds (OR-11).

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
        // The words without "&", the translator's letter still pressing it (0.8.121).
        QCOMPARE(yesButton->property("text").toString(), QStringLiteral("Yes"));
        QCOMPARE(question->property("yesLetter").toString(), QStringLiteral("Y"));
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
    // In Settings > General "Check for updates" stands above "Clear the
    // clipboard after", on the screen and for Tab (the owner, 0.8.119).
    void check_for_updates_comes_before_the_clipboard()
    {
        auto* settings = dialog("settingsWindow");
        QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
        QVERIFY(comesUp(settings));
        const auto here = [settings](const char* name) { return seen(settings->contentItem(), QString::fromLatin1(name)); };
        QQuickItem* updates = here("checkFrequencyBox");
        QQuickItem* clipboard = here("clipboardBox");
        QVERIFY(updates && clipboard);
        QVERIFY(updates->mapToScene(QPointF()).y() + updates->height() <= clipboard->mapToScene(QPointF()).y());
        updates->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(updates->hasActiveFocus());
        QTest::keyClick(settings, Qt::Key_Tab);
        QTRY_VERIFY(clipboard->hasActiveFocus() || clipboard->isAncestorOf(settings->activeFocusItem()));
        QTest::keyClick(settings, Qt::Key_Backtab);
        QTRY_VERIFY(updates->hasActiveFocus());
        settings->close();
    }

    // No window that asks an answer can be minimized or maximized: its title
    // bar holds its title and × only (the owner, D-079).
    void no_question_window_has_a_minimize_button()
    {
        int questions = 0;
        for (QQuickWindow* one : window->findChildren<QQuickWindow*>()) {
            if (one->objectName() == QLatin1String("settingsWindow")) {
                continue; // the main window and Settings keep theirs
            }
            const QString name = one->objectName() + QLatin1Char(' ') + one->title();
            const Qt::WindowFlags flags = one->flags();
            QVERIFY2(flags.testFlag(Qt::CustomizeWindowHint), qPrintable(name));
            QVERIFY2(flags.testFlag(Qt::WindowCloseButtonHint), qPrintable(name));
            QVERIFY2(!flags.testFlag(Qt::WindowMinimizeButtonHint), qPrintable(name));
            QVERIFY2(!flags.testFlag(Qt::WindowMaximizeButtonHint), qPrintable(name));
#if defined(Q_OS_LINUX)
            QVERIFY2(one->type() == Qt::Tool, qPrintable(name)); // KDE minimizes no utility window
#else
            QVERIFY2(one->type() == Qt::Dialog, qPrintable(name));
#endif
            ++questions;
        }
        QVERIFY(dialog("keyCatcher") && dialog("keyTaken") && dialog("disconnectWindow"));
        QVERIFY(questions > 20);
    }

    // The main window's title is the program and its version, also with a
    // vault open (the owner, D-017).
    void the_title_is_the_program_and_its_version()
    {
        QCOMPARE(controller->state(), State::Unlocked);
        QCOMPARE(window->title(), QStringLiteral("Chrono Vault ") + controller->version());
    }

    // In the top bar, the space from Generate Password to the padlock equals
    // the space from the padlock to the gear, both as seen on the screen (the
    // owner, D-074).
    void the_top_bar_spaces_are_equal()
    {
        const auto back = qScopeGuard([]() { applyProgramScale(100); });
        spacesAreEqualAt(100);
        spacesAreEqualAt(200);
    }

private:
    void spacesAreEqualAt(int scale)
    {
        applyProgramScale(scale);
        window->resize(16 * scale, 9 * scale);
        QCoreApplication::processEvents();
        (void)window->grabWindow();
        QCoreApplication::processEvents();
        QQuickItem* generate = part("generateButton");
        QQuickItem* lock = part("lockButton");
        QQuickItem* gear = part("preferencesButton");
        QVERIFY(generate && lock && gear);
        const QImage shot = window->grabWindow();
        const qreal ratio = shot.devicePixelRatio();
        const QPointF lockAt = lock->mapToScene(QPointF(0, 0));
        const int lockX = qCeil(lockAt.x() * ratio);
        const int gearX = qCeil(gear->mapToScene(QPointF(0, 0)).x() * ratio);
        const int fromX = qFloor(generate->mapToScene(QPointF(generate->width() / 2, 0)).x() * ratio);
        const int top = qRound((lockAt.y() + lock->height() / 4) * ratio);
        const int bottom = qRound((lockAt.y() + lock->height() * 3 / 4) * ratio);
        const int empty = lockX + 1; // inside the padlock's circle, before the picture
        // A column holds something seen where a pixel differs from the bar's
        // own colour in its row.
        const auto seenAt = [&](int x) {
            for (int y = top; y <= bottom; ++y) {
                const QColor one = shot.pixelColor(x, y);
                const QColor bar = shot.pixelColor(empty, y);
                if (qAbs(one.red() - bar.red()) + qAbs(one.green() - bar.green()) + qAbs(one.blue() - bar.blue()) > 60) {
                    return true;
                }
            }
            return false;
        };
        const auto firstSeen = [&](int from, int to, int step) {
            for (int x = from; x != to; x += step) {
                if (seenAt(x)) {
                    return x;
                }
            }
            return -1;
        };
        const int generateRight = firstSeen(lockX - 1, fromX, -1);
        const int lockLeft = firstSeen(lockX, gearX, 1);
        const int lockRight = firstSeen(gearX - 1, lockX, -1);
        const int gearLeft = firstSeen(gearX, gearX + qRound(gear->width() * ratio), 1);
        QVERIFY(generateRight > 0 && lockLeft > 0 && lockRight > lockLeft && gearLeft > lockRight);
        const int before = lockLeft - generateRight - 1;
        const int after = gearLeft - lockRight - 1;
        QVERIFY2(qAbs(before - after) <= 1, qPrintable(QStringLiteral("%1%: %2 and %3").arg(scale).arg(before).arg(after)));
    }
#else
    // Sort By, its headings and the letter keys, apart so each binary stays
    // under 20 seconds (OR-11; the owner, D-081).
    void each_order_and_direction()
    {
        rewrite([](VaultContents& contents) {
            const QDateTime base = QDateTime::currentDateTimeUtc().addDays(-1);
            for (Item& item : contents.items) {
                const int number = item.title == QLatin1String("Mill") ? 30 : item.title.mid(5).toInt();
                item.fields.append(webAddress(QStringLiteral("site%1.example").arg((number * 7) % 31, 2, 10, QLatin1Char('0'))));
                item.created = base.addSecs(60 * ((number * 11) % 31));
                item.modified = base.addSecs(60 * ((number * 13) % 31));
            }
        });
        // By use: Item 05 three times, Item 03 twice, Item 07 once; the last
        // use of each in the order 07, 03, 05.
        for (const char* title : {"Item 05", "Item 05", "Item 07", "Item 03", "Item 03", "Item 05"}) {
            controller->usedItem(idOf(QString::fromLatin1(title)));
            QTest::qWait(3);
        }
        const auto titles = [this]() {
            QStringList all;
            for (int row = 0; row < controller->items()->rowCount(); ++row) {
                all << controller->items()->index(row).data(ItemListModel::TitleRole).toString();
            }
            return all;
        };
        const auto valuesBy = [this](const char* role) {
            QStringList all;
            const VaultContents& contents = kept;
            for (int row = 0; row < controller->items()->rowCount(); ++row) {
                const QString id = controller->items()->idAt(row);
                for (const Item& item : contents.items) {
                    if (item.id.toString(QUuid::WithoutBraces) != id) {
                        continue;
                    }
                    all << (qstrcmp(role, "url") == 0       ? item.fields.constFirst().value
                            : qstrcmp(role, "created") == 0 ? item.created.toString(Qt::ISODate)
                                                            : item.modified.toString(Qt::ISODate));
                }
            }
            return all;
        };
        const auto sorted = [](QStringList values, bool descending) {
            std::sort(values.begin(), values.end());
            if (descending) {
                std::reverse(values.begin(), values.end());
            }
            return values;
        };
        for (const bool descending : {false, true}) {
            controller->setSortDescending(descending);
            controller->setSortBy(0);
            QCOMPARE(titles().constFirst(), descending ? QStringLiteral("Mill") : QStringLiteral("Item 00"));
            QCOMPARE(titles().constLast(), descending ? QStringLiteral("Item 00") : QStringLiteral("Mill"));
            int by = 1;
            for (const char* role : {"url", "created", "modified"}) {
                controller->setSortBy(by++);
                const QStringList seen = valuesBy(role);
                QCOMPARE(seen.size(), 31);
                QCOMPARE(seen, sorted(seen, descending));
            }
            // Never used first going up, by title A to Z among themselves.
            controller->setSortBy(4);
            QStringList used = titles();
            QCOMPARE(descending ? used.mid(0, 3) : used.mid(28),
                     descending ? QStringList({"Item 05", "Item 03", "Item 07"}) : QStringList({"Item 07", "Item 03", "Item 05"}));
            QCOMPARE(descending ? used.at(3) : used.at(0), QStringLiteral("Item 00"));
            controller->setSortBy(5);
            used = titles();
            QCOMPARE(descending ? used.mid(0, 3) : used.mid(28),
                     descending ? QStringList({"Item 05", "Item 03", "Item 07"}) : QStringList({"Item 07", "Item 03", "Item 05"}));
        }
    }

    // Headings by title only; the menu shows one tick in each part.
    void headings_only_by_title()
    {
        auto* section = list->property("section").value<QObject*>();
        QVERIFY(section);
        QCOMPARE(section->property("property").toString(), QStringLiteral("initial"));
        QTRY_VERIFY(headingSeen(QStringLiteral("I")));
        auto* button = part("sortButton");
        QVERIFY(button);
        QCOMPARE(button->property("text").toString(), QStringLiteral("Title ▾"));
        click(button);
        auto* menu = window->findChild<QObject*>(QStringLiteral("sortMenu"));
        QTRY_VERIFY(menu && menu->property("opened").toBool());
        QCOMPARE(menu->property("count").toInt(), 9); // six, a line, two
        auto* url = window->findChild<QObject*>(QStringLiteral("sortBy_1"));
        QVERIFY(url && QMetaObject::invokeMethod(url, "triggered"));
        QTRY_COMPARE(section->property("property").toString(), QString());
        QTRY_VERIFY(!headingSeen(QStringLiteral("I")));
        QCOMPARE(button->property("text").toString(), QStringLiteral("URL ▾"));
        int ticked = 0;
        for (int by = 0; by < 6; ++by) {
            ticked += window->findChild<QObject*>(QStringLiteral("sortBy_%1").arg(by))->property("checked").toBool() ? 1 : 0;
        }
        QCOMPARE(ticked, 1);
        auto* down = window->findChild<QObject*>(QStringLiteral("sortDescending"));
        auto* up = window->findChild<QObject*>(QStringLiteral("sortAscending"));
        QVERIFY(up->property("checked").toBool() && !down->property("checked").toBool());
        QVERIFY(QMetaObject::invokeMethod(down, "triggered"));
        QTRY_VERIFY(controller->sortDescending());
        QVERIFY(!up->property("checked").toBool() && down->property("checked").toBool());
        QVERIFY(QMetaObject::invokeMethod(window->findChild<QObject*>(QStringLiteral("sortBy_0")), "triggered"));
        QTRY_COMPARE(section->property("property").toString(), QStringLiteral("initial"));
        QVERIFY(QMetaObject::invokeMethod(menu, "close"));
    }

    // A use is a field copied or the website opened; kept on this computer,
    // not in the vault (D-081).
    void a_use_is_counted()
    {
        rewrite([](VaultContents& contents) {
            Field name;
            name.role = QStringLiteral("username");
            name.kind = FieldKind::Username;
            name.value = QStringLiteral("rob");
            contents.items[4].fields << name << webAddress(QStringLiteral("example.com"));
        });
        QStringList opened;
        Opener opener(&opened);
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &opener, "open");
        const auto noHandler = qScopeGuard([]() { QDesktopServices::unsetUrlHandler(QStringLiteral("https")); });
        const QString id = idOf(QStringLiteral("Item 04"));
        const QString file = controller->vaultPath();
        QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, id));
        QTRY_COMPARE(page->property("chosenItemId").toString(), id);
        QObject* copyName = window->findChild<QObject*>(QStringLiteral("copyUsernameAction"));
        QTRY_VERIFY(copyName && copyName->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(copyName, "trigger"));
        QCOMPARE(keptUsesOf(file).value(QUuid::fromString(id)).count, 1);
        QObject* launch = window->findChild<QObject*>(QStringLiteral("launchUrlAction"));
        QVERIFY(launch && QMetaObject::invokeMethod(launch, "trigger"));
        QTRY_COMPARE(opened, QStringList{QStringLiteral("https://example.com")});
        QCOMPARE(keptUsesOf(file).value(QUuid::fromString(id)).count, 2);
        QVERIFY(keptUsesOf(file).value(QUuid::fromString(id)).last.isValid());
        controller->setSortBy(4);
        QCOMPARE(controller->items()->idAt(30), id); // the most recent last, going up
        forgetOrderOf(file);
    }

    // Sort By is kept per vault, on this computer.
    void kept_per_vault()
    {
        const QString first = controller->vaultPath();
        controller->setSortBy(2);
        controller->setSortDescending(true);
        QCOMPARE(keptOrderOf(first).by, SortBy::Created);
        controller->lock();
        QTRY_COMPARE(controller->state(), State::Locked);
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QCOMPARE(controller->sortBy(), 2);
        QVERIFY(controller->sortDescending());
        // Another vault starts on Title, Ascending.
        const QString second = folder.filePath(QStringLiteral("second.chronovault"));
        controller->createVault(QUrl::fromLocalFile(second), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_COMPARE(controller->vaultPath(), second);
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QCOMPARE(controller->sortBy(), 0);
        QVERIFY(!controller->sortDescending());
        QCOMPARE(keptOrderOf(first).by, SortBy::Created);
        forgetOrderOf(first);
        forgetOrderOf(second);
    }

    // By title, a key brings its heading to the top of the list (the owner,
    // D-081).
    void a_key_brings_its_heading_to_the_top()
    {
        rewrite([](VaultContents& contents) {
            for (const char* title : {"1Password", "Alpha 1", "Alpha 2", "Alpha 3", "Zeta"}) {
                Item item;
                item.id = QUuid::createUuid();
                item.title = QString::fromLatin1(title);
                item.created = item.modified = QDateTime::currentDateTimeUtc();
                contents.items.append(item);
            }
        });
        click(row(2));
        QTRY_VERIFY(list->hasActiveFocus());
        // Every key a first letter of its own: no word is typed, nothing
        // waits a second (the owner, D-081).
        QVERIFY(!list->property("typed").isValid());
        const auto press = [this](Qt::Key letter) {
            key(letter);
            QCoreApplication::processEvents();
        };
        press(Qt::Key_A);
        QTRY_VERIFY(headingAtTop(QStringLiteral("A")));
        press(Qt::Key_I);
        QTRY_VERIFY(headingAtTop(QStringLiteral("I")));
        press(Qt::Key_B); // no B: the next heading
        QTRY_VERIFY(headingAtTop(QStringLiteral("I")));
        press(Qt::Key_5); // a digit: "#"
        QTRY_VERIFY(headingAtTop(QStringLiteral("#")));
        press(Qt::Key_Z); // near the end: only to the bottom, no empty room
        QTRY_VERIFY(atTheBottom());
        QVERIFY(headingSeen(QStringLiteral("Z")));
        QVERIFY(!headingAtTop(QStringLiteral("Z")));
        press(Qt::Key_A);
        QTRY_VERIFY(headingAtTop(QStringLiteral("A")));
        press(Qt::Key_N); // after the last heading but Z: Z, at the bottom
        QTRY_VERIFY(atTheBottom());
        // Keys quickly one after another: each its own first letter at once.
        key(Qt::Key_Z);
        key(Qt::Key_A);
        QCoreApplication::processEvents();
        QTRY_VERIFY(headingAtTop(QStringLiteral("A")));
        key(Qt::Key_A);
        key(Qt::Key_I);
        key(Qt::Key_5);
        QCoreApplication::processEvents();
        QTRY_VERIFY(headingAtTop(QStringLiteral("#")));
    }

private:
    VaultContents kept; // the vault as rewrite last wrote it
    static Field webAddress(const QString& address)
    {
        Field field;
        field.role = QStringLiteral("url");
        field.kind = FieldKind::WebAddress;
        field.value = address;
        return field;
    }
    QString idOf(const QString& title) const
    {
        for (const Item& item : kept.items) {
            if (item.title == title) {
                return item.id.toString(QUuid::WithoutBraces);
            }
        }
        return {};
    }
    // The vault written again with a change, as the fixture writes it.
    void rewrite(const std::function<void(VaultContents&)>& change)
    {
        const QString file = controller->vaultPath();
        controller->lock();
        QTRY_COMPARE(controller->state(), State::Locked);
        const OpenResult opened = openVaultFile(file, "pass");
        VaultContents contents = contentsFromJson(opened.contents).contents;
        change(contents);
        kept = contents;
        QCOMPARE(saveVaultFile(file, contentsToJson(contents), opened.key), SaveStatus::Saved);
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QTRY_VERIFY(row(0));
    }
    QQuickItem* heading(const QString& text) const
    {
        for (QQuickItem* one : list->property("contentItem").value<QQuickItem*>()->childItems()) {
            if (one->isVisible() && one->property("section").isValid() && one->property("text").toString() == text) {
                return one;
            }
        }
        return nullptr;
    }
    bool headingSeen(const QString& text) const { return heading(text) != nullptr; }
    // The last row's bottom is the list's: nothing cut off, no room below.
    bool atTheBottom()
    {
        QQuickItem* last = row(list->property("count").toInt() - 1);
        return last && qAbs(last->mapToItem(list, QPointF(0, last->height())).y() - list->height()) <= 1;
    }
    bool headingAtTop(const QString& text) const
    {
        QQuickItem* one = heading(text);
        return one && qAbs(one->mapToItem(list, QPointF(0, 0)).y()) <= 1;
    }
#endif
};

QTEST_MAIN(TestIntuitive)
#include "test_intuitive.moc"
