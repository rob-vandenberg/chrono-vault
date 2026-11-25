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

#include "icon_library.h"

#include <QDir>
#include <QImage>
#include <QScopeGuard>
#include <QSignalSpy>

// --- Version ------------------------------------------------------------
// test_dialog_keys.cpp 0.6.97

// --- Version history ----------------------------------------------------
// v0.6.97: A long text breaks in a window of normal width (D-061)
// v0.6.93: A start on a locked vault (D-068)
// v0.6.92: A field has the keyboard, not its own item (D-069)
// v0.6.91: Initial release (D-068)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// The keyboard in the dialogs, and the text fields' menu (D-068).
class TestDialogKeys : public IntuitiveFixture
{
    Q_OBJECT

    // A window of this name, wherever it is made.
    QQuickWindow* named(const char* name) const
    {
        const QList<QQuickWindow*> all = window->findChildren<QQuickWindow*>(QString::fromLatin1(name));
        return all.isEmpty() ? nullptr : all.constFirst();
    }
    void typed(QQuickWindow* one, const char* field)
    {
        QVERIFY2(one, field);
        QVERIFY2(comesUp(one), field);
        QTest::keyClick(one, Qt::Key_X);
        QTest::keyClick(one, Qt::Key_Y);
        QQuickItem* there = one->activeFocusItem();
        QVERIFY2(there, field);
        QVERIFY2(there->property("text").toString().endsWith(QLatin1String("xy")), field);
        QMetaObject::invokeMethod(one, "reject");
        QTRY_VERIFY(!one->isVisible());
    }
    QQuickItem* outlined(QQuickWindow* picker) const
    {
        return picker->findChild<QQuickItem*>(QStringLiteral("iconGrid"))->property("outlined").value<QQuickItem*>();
    }
    QPointF placeOf(QQuickWindow* picker) const
    {
        return outlined(picker)->mapToItem(picker->findChild<QQuickItem*>(QStringLiteral("iconGrid")), QPointF(0, 0));
    }

private slots:
    void every_dialog_with_a_field_opens_in_it()
    {
        QMetaObject::invokeMethod(window, "changeMasterPassword");
        typed(named("masterPasswordWindow"), "master password");
        QVERIFY(QMetaObject::invokeMethod(named("exportWindow"), "ask"));
        typed(named("exportWindow"), "export");
        QVERIFY(QMetaObject::invokeMethod(named("tagDialog"), "ask", Q_ARG(QVariant, QStringLiteral("Work"))));
        typed(named("tagDialog"), "tag");
        QVERIFY(QMetaObject::invokeMethod(named("categoryDialog"), "ask", Q_ARG(QVariant, QString()), Q_ARG(QVariant, QString())));
        typed(named("categoryDialog"), "new category");
        QVERIFY(QMetaObject::invokeMethod(named("webDavPasswordWindow"), "open"));
        typed(named("webDavPasswordWindow"), "WebDAV password");
        QVERIFY(QMetaObject::invokeMethod(named("templateEditor"), "edit",
                                          Q_ARG(QVariant, QVariantMap({{"id", ""}, {"name", ""}, {"categoryId", "misc"},
                                                                       {"builtIn", false}, {"fields", QVariantList()}}))));
        typed(named("templateEditor"), "template");
        QVERIFY(QMetaObject::invokeMethod(named("newVaultDialog"), "open"));
        typed(named("newVaultDialog"), "new vault");
        QVERIFY(QMetaObject::invokeMethod(named("unlockFirstWindow"), "ask", Q_ARG(QVariant, folder.filePath(QStringLiteral("other.chronovault"))),
                                          Q_ARG(QVariant, QStringLiteral("export"))));
        typed(named("unlockFirstWindow"), "unlock first");
        QVERIFY(QMetaObject::invokeMethod(named("retryPasswordWindow"), "ask", Q_ARG(QVariant, QStringLiteral("vault:x")),
                                          Q_ARG(QVariant, QStringLiteral("x")), Q_ARG(QVariant, QVariantList()),
                                          Q_ARG(QVariant, QString())));
        typed(named("retryPasswordWindow"), "retry sync");
        // The item's title, a field's name, and the search in Choose Icon.
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        click(row(1));
        key(Qt::Key_Return);
        QTRY_VERIFY(page->property("editing").toBool());
        QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
        QTRY_VERIFY(window->findChild<QQuickItem*>(QStringLiteral("titleField"))->hasActiveFocus());
        QVERIFY(QMetaObject::invokeMethod(named("fieldOptions"), "showForNew", Q_ARG(QVariant, QStringLiteral("text"))));
        typed(named("fieldOptions"), "field options");
        QMetaObject::invokeMethod(editor, "close");
        QVERIFY(QMetaObject::invokeMethod(named("iconPicker"), "start", Q_ARG(QVariant, QString())));
        typed(named("iconPicker"), "choose icon");
        // Locked: the master password.
        controller->lock();
        QTRY_COMPARE(controller->state(), State::Locked);
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QTRY_VERIFY(window->findChild<QQuickItem*>(QStringLiteral("masterPasswordField"))->hasActiveFocus());
    }

    // Started on a locked vault: the keyboard waits in the password's field.
    void a_start_on_a_locked_vault_waits_in_the_password()
    {
        controller->lock();
        QTRY_COMPARE(controller->state(), State::Locked);
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->loadFromModule("ChronoVault", "Main");
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst());
        showMainWindow(window, false, false);
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QQuickItem* field = window->findChild<QQuickItem*>(QStringLiteral("masterPasswordField"));
        QTRY_VERIFY(field->hasActiveFocus());
        type(QStringLiteral("ab"));
        QCOMPARE(field->property("text").toString(), QStringLiteral("ab"));
    }

    // A long text breaks in a window of normal width, which can be narrower too (D-061).
    void a_long_text_breaks_in_its_dialog()
    {
        QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
        QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
        for (const char* name : {"disconnectWindow", "forgetWindow"}) {
            QQuickWindow* one = named(name);
            QVERIFY2(one, name);
            one->setProperty("vaultFile", folder.filePath(QStringLiteral("away.chronovault")));
            one->setProperty("onWebDav", true);
            QVERIFY(QMetaObject::invokeMethod(one, "open"));
            QTRY_VERIFY2(one->isExposed(), name);
            QVERIFY2(one->width() <= 450 && one->minimumWidth() <= 450, qPrintable(QString::number(one->width())));
            const QList<QQuickItem*> texts = one->findChildren<QQuickItem*>(QStringLiteral("dialogText"));
            QCOMPARE(texts.size(), 1);
            QTRY_VERIFY2(texts.constFirst()->property("lineCount").toInt() > (qstrcmp(name, "disconnectWindow") == 0 ? 6 : 1), name);
            QTRY_VERIFY2(one->height() >= one->minimumHeight(), name);
            QMetaObject::invokeMethod(one, "reject");
        }
        QMetaObject::invokeMethod(settings, "close");
    }

    void the_icons_by_keyboard()
    {
        // Two groups of icons, more than the window shows at once.
        QTemporaryDir installed;
        const auto done = qScopeGuard([]() { setIconsFoldersForTests(QString(), QString()); });
        QImage picture(16, 16, QImage::Format_ARGB32);
        picture.fill(Qt::red);
        for (const char* group : {"web", "cards"}) {
            QVERIFY(QDir(installed.path()).mkpath(QString::fromLatin1(group)));
            for (int number = 0; number < 60; ++number) {
                QVERIFY(picture.save(installed.filePath(QStringLiteral("%1/a%2.png").arg(QLatin1String(group)).arg(number, 2, 10, QLatin1Char('0')))));
            }
        }
        QFile list(installed.filePath(QStringLiteral("icons.md")));
        QVERIFY(list.open(QIODevice::WriteOnly));
        list.write("web: Web\ncards: Cards\n");
        list.close();
        setIconsFoldersForTests(installed.path(), installed.path());
        QQuickWindow* picker = named("iconPicker");
        QVERIFY(QMetaObject::invokeMethod(picker, "start", Q_ARG(QVariant, QStringLiteral("qrc:/icons/item-login.svg"))));
        QVERIFY(comesUp(picker));
        QQuickItem* search = picker->findChild<QQuickItem*>(QStringLiteral("iconSearchField"));
        QQuickItem* grid = picker->findChild<QQuickItem*>(QStringLiteral("iconGrid"));
        QTRY_VERIFY(search->hasActiveFocus());
        // Down and Enter stay in Search; only Tab goes to the icons.
        QTest::keyClick(picker, Qt::Key_Down);
        QTest::keyClick(picker, Qt::Key_Return);
        QVERIFY(search->hasActiveFocus() && picker->isVisible());
        QTest::keyClick(picker, Qt::Key_Tab);
        QTRY_VERIFY(grid->hasActiveFocus());
        QVERIFY(outlined(picker));
        QCOMPARE(outlined(picker)->property("key").toString(), QStringLiteral("default"));
        QVERIFY(seen(outlined(picker), QStringLiteral("tileOutline")));
        QTest::keyClick(picker, Qt::Key_Tab, Qt::ShiftModifier);
        QTRY_VERIFY(search->hasActiveFocus());
        // Narrowed: the first one found.
        for (const char letter : QByteArray("a")) {
            QTest::keyClick(picker, letter);
        }
        QTest::keyClick(picker, Qt::Key_Tab);
        QTRY_VERIFY(grid->hasActiveFocus() && outlined(picker));
        QQuickItem* first = outlined(picker);
        QTRY_VERIFY(placeOf(picker).y() < 60); // the narrowed list laid out
        const QPointF start = placeOf(picker);
        QTest::keyClick(picker, Qt::Key_Right);
        QVERIFY(outlined(picker) != first);
        QVERIFY(placeOf(picker).x() > start.x() && qFuzzyCompare(placeOf(picker).y(), start.y()));
        QTest::keyClick(picker, Qt::Key_Left);
        QCOMPARE(outlined(picker), first);
        QTest::keyClick(picker, Qt::Key_Down);
        QVERIFY(placeOf(picker).y() > start.y());
        QTest::keyClick(picker, Qt::Key_Up);
        QVERIFY(qFuzzyCompare(placeOf(picker).y(), start.y()));
        QTest::keyClick(picker, Qt::Key_Home);
        QCOMPARE(outlined(picker), first);
        QTest::keyClick(picker, Qt::Key_PageDown);
        QVERIFY(placeOf(picker).y() > start.y() + 100);
        QTest::keyClick(picker, Qt::Key_End);
        QQuickItem* last = outlined(picker);
        QVERIFY(last != first);
        QTest::keyClick(picker, Qt::Key_Right);
        QCOMPARE(outlined(picker), last);
        // In view.
        QQuickItem* scroll = picker->findChild<QQuickItem*>(QStringLiteral("iconScroll"));
        const double y = last->mapToItem(scroll, QPointF(0, 0)).y();
        QVERIFY(y >= 0 && y + last->height() <= scroll->height() + 1);
        QTest::keyClick(picker, Qt::Key_PageUp);
        QVERIFY(outlined(picker) != last);
        // The outlined one gone while the keys are here: the first again.
        grid->setProperty("outlined", QVariant::fromValue<QQuickItem*>(nullptr));
        QVERIFY(!outlined(picker));
        QTest::keyClick(picker, Qt::Key_Return); // never takes what is not outlined
        QVERIFY(picker->isVisible() && outlined(picker));
        QTest::keyClick(picker, Qt::Key_Right);
        QVERIFY(outlined(picker));
        // The next Tab goes on to the buttons.
        QTest::keyClick(picker, Qt::Key_Tab);
        QTRY_VERIFY(!grid->hasActiveFocus() && !search->hasActiveFocus());
        // Esc cancels from the icons; Enter takes the outlined one, not the checked one.
        QSignalSpy taken(picker, SIGNAL(iconChosen(QString)));
        grid->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(picker, Qt::Key_Escape);
        QTRY_VERIFY(!picker->isVisible());
        QCOMPARE(taken.count(), 0);
        QVERIFY(QMetaObject::invokeMethod(picker, "start", Q_ARG(QVariant, QStringLiteral("qrc:/icons/item-login.svg"))));
        QVERIFY(comesUp(picker));
        picker->setProperty("chosen", QStringLiteral("file:nowhere"));
        QTest::keyClick(picker, Qt::Key_Tab);
        QTRY_VERIFY(outlined(picker));
        QTest::keyClick(picker, Qt::Key_Return);
        QTRY_VERIFY(!picker->isVisible());
        QCOMPARE(taken.count(), 1);
        QCOMPARE(taken.at(0).at(0).toString(), QString()); // the category's own
    }

    void a_text_field_has_its_menu()
    {
        const auto menu = [this](const QObject* field) -> QObject* {
            for (QObject* one : window->findChildren<QObject*>(QStringLiteral("textMenu"))) {
                if (one->property("editor").value<QObject*>() == field && one->property("visible").toBool()) {
                    return one;
                }
            }
            for (QObject* one : field->findChildren<QObject*>(QStringLiteral("textMenu"))) {
                if (one->property("visible").toBool()) {
                    return one;
                }
            }
            return nullptr;
        };
        const auto enabled = [](const QObject* popup, const char* name) {
            return popup->findChild<QObject*>(QString::fromLatin1(name))->property("enabled").toBool();
        };
        QQuickItem* search = part("searchField");
        controller->setSearchText(QStringLiteral("Item"));
        QGuiApplication::clipboard()->setText(QStringLiteral(" 03"));
        click(search, {}, Qt::RightButton);
        QObject* popup = nullptr;
        QTRY_VERIFY(popup = menu(search));
        QVERIFY(!enabled(popup, "textCut") && !enabled(popup, "textCopy"));
        QVERIFY(enabled(popup, "textPaste") && enabled(popup, "textSelectAll"));
        QMetaObject::invokeMethod(popup->findChild<QObject*>(QStringLiteral("textSelectAll")), "triggered");
        QMetaObject::invokeMethod(popup, "close");
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(search->property("selectedText").toString(), QStringLiteral("Item"));
        // What is chosen stays chosen under the menu.
        click(search, {}, Qt::RightButton);
        QTRY_VERIFY(popup = menu(search));
        QCOMPARE(search->property("selectedText").toString(), QStringLiteral("Item"));
        QVERIFY(enabled(popup, "textCut") && enabled(popup, "textCopy") && !enabled(popup, "textSelectAll"));
        QMetaObject::invokeMethod(popup->findChild<QObject*>(QStringLiteral("textCut")), "triggered");
        QMetaObject::invokeMethod(popup, "close");
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(search->property("text").toString(), QString());
        QCOMPARE(QGuiApplication::clipboard()->text(), QStringLiteral("Item"));
        click(search, {}, Qt::RightButton);
        QTRY_VERIFY(popup = menu(search));
        QMetaObject::invokeMethod(popup->findChild<QObject*>(QStringLiteral("textPaste")), "triggered");
        QMetaObject::invokeMethod(popup, "close");
        QTRY_COMPARE(search->property("text").toString(), QStringLiteral("Item"));
        QTRY_VERIFY(!popup->property("visible").toBool());

        // A hidden password is never cut or copied.
        controller->lock();
        QTRY_COMPARE(controller->state(), State::Locked);
        QTRY_VERIFY(part("masterPasswordField"));
        QQuickItem* password = part("masterPasswordField");
        password->setProperty("text", QStringLiteral("secret"));
        QMetaObject::invokeMethod(password, "selectAll");
        QTest::mouseClick(window, Qt::RightButton, {}, password->mapToScene(QPointF(40, password->height() / 2)).toPoint());
        QTRY_VERIFY(popup = menu(password));
        QVERIFY(!enabled(popup, "textCut") && !enabled(popup, "textCopy") && enabled(popup, "textPaste"));
        QMetaObject::invokeMethod(popup, "close");
        QTRY_VERIFY(!popup->property("visible").toBool());
        QGuiApplication::clipboard()->clear();
    }
};

QTEST_MAIN(TestDialogKeys)
#include "test_dialog_keys.moc"
