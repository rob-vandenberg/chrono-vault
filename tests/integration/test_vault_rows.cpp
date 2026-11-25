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

#include <QQmlComponent>

// --- Version ------------------------------------------------------------
// test_vault_rows.cpp 0.6.105

// --- Version history ----------------------------------------------------
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.6.104: A click on the eye
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.6.102: An outdated kept password is said in Settings; the gear is "Options" (D-071)
// v0.6.101: A menu fits its entries and closes with its window (D-072)
// v0.6.100: Initial release (D-071)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// Settings > Vaults: the padlocks, Open and Lock, and the round buttons (D-071).
class TestVaultRows : public IntuitiveFixture
{
    Q_OBJECT

    QQuickWindow* settings() const { return window->findChild<QQuickWindow*>(QStringLiteral("settingsWindow")); }
    QQuickItem* there(const QString& name) { return seen(settings()->contentItem(), name); }
    // An entry of a vault's menu, by its button.
    QObject* entry(const QString& name, const QString& vault)
    {
        QQuickItem* button = there(QStringLiteral("vaultMenuButton_") + vault);
        QObject* menu = button ? button->findChild<QObject*>(QStringLiteral("vaultMenu_") + vault) : nullptr;
        if (!menu || name.isEmpty()) {
            return menu;
        }
        for (int index = 0; index < menu->property("count").toInt(); ++index) {
            QQuickItem* one = nullptr;
            QMetaObject::invokeMethod(menu, "itemAt", Q_RETURN_ARG(QQuickItem*, one), Q_ARG(int, index));
            if (one && one->objectName() == name + vault) {
                return one;
            }
        }
        return nullptr;
    }
    bool opened(const QString& name)
    {
        QQuickItem* lock = there(QStringLiteral("padlock_") + name);
        return lock && lock->property("open").toBool();
    }

private slots:
    // A menu is as wide as its longest entry, 120 at least, and closes when
    // another window takes the keyboard; a choice box's list too (D-072).
    void a_menu_fits_its_entries_and_closes_with_its_window()
    {
        QQmlComponent made(engine.get());
        made.setData("import QtQuick\nimport QtQuick.Controls\n"
                     "Item { id: all; property Menu small: Menu { MenuItem { text: 'Copy' } MenuSeparator {} }\n"
                     "property Menu large: Menu { MenuItem { text: 'Copy' } MenuItem { objectName: 'long'; text: 'An entry that is a good deal longer' } }\n"
                     "ComboBox { objectName: 'box'; y: 200; model: ['One', 'Two'] } }", QUrl());
        std::unique_ptr<QObject> all(made.create());
        QVERIFY2(all, qPrintable(made.errorString()));
        auto* item = qobject_cast<QQuickItem*>(all.get());
        item->setParentItem(window->contentItem());
        QObject* small = item->property("small").value<QObject*>();
        QObject* large = item->property("large").value<QObject*>();
        QMetaObject::invokeMethod(small, "popup", Q_ARG(QQuickItem*, item));
        QTRY_VERIFY(small->property("opened").toBool());
        QCOMPARE(small->property("width").toDouble(), 120.0);
        QMetaObject::invokeMethod(small, "close");
        QMetaObject::invokeMethod(large, "popup", Q_ARG(QQuickItem*, item));
        QTRY_VERIFY(large->property("opened").toBool());
        QQuickItem* entryOf = nullptr;
        QMetaObject::invokeMethod(large, "itemAt", Q_RETURN_ARG(QQuickItem*, entryOf), Q_ARG(int, 1));
        QVERIFY(entryOf && entryOf->implicitWidth() > 120);
        QVERIFY(qAbs(large->property("width").toDouble() - entryOf->implicitWidth()) <= 4);
        // Another window has the keyboard.
        QWindow other;
        other.show();
        other.requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), &other);
        QTRY_VERIFY(!large->property("visible").toBool());
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QObject* list = seen(item, QStringLiteral("box"))->property("popup").value<QObject*>();
        QMetaObject::invokeMethod(list, "open");
        QTRY_VERIFY(list->property("opened").toBool());
        other.requestActivate();
        QTRY_VERIFY(!list->property("visible").toBool());
    }

    // Another vault the primary: only the crown moves; "Remember my credentials"
    // keeps a typed password in the primary; Open, Closed and Locked (D-074).
    void another_primary_and_remembered_credentials()
    {
        const QString first = controller->vaultPath();
        const QString second = folder.filePath(QStringLiteral("mine.chronovault"));
        const auto is = [this](const QString& vault) { return controller->vaultPath() == vault && controller->state() == State::Unlocked; };
        controller->createVault(QUrl::fromLocalFile(second), QStringLiteral("two"), QStringLiteral("two"));
        QTRY_VERIFY(is(second) && controller->isIdle());
        QCOMPARE(controller->primaryVault(), first);
        // Its key kept, its own password not: Closed and Open; the primary left is Closed.
        QCOMPARE(controller->vaultStateOf(second), 2);
        QCOMPARE(controller->vaultStateOf(first), 1);
        controller->lock();
        QCOMPARE(controller->vaultStateOf(second), 0);
        QCOMPARE(controller->vaultStateOf(first), 0);

        QMetaObject::invokeMethod(settings(), "showWindow");
        auto* ask = settings()->findChild<QQuickWindow*>(QStringLiteral("unlockFirstWindow"));
        auto* box = ask->findChild<QQuickItem*>(QStringLiteral("askRemember"));
        const auto opens = [&](const QString& vault, const QString& password) {
            QVERIFY(QMetaObject::invokeMethod(ask, "askVault", Q_ARG(QVariant, vault)));
            ask->findChild<QObject*>(QStringLiteral("askField"))->setProperty("text", password);
            QMetaObject::invokeMethod(ask, "go");
        };
        // Everything locked: gray and not ticked; nothing is kept.
        opens(second, QStringLiteral("two"));
        QVERIFY(box->property("wanted").toBool() && !box->isEnabled() && !box->property("checked").toBool());
        QTRY_VERIFY(is(second));
        QCOMPARE(controller->vaultStateOf(second), 2);
        // The primary: no box.
        opens(first, QStringLiteral("pass"));
        QVERIFY(!box->property("wanted").toBool());
        QTRY_VERIFY(is(first));
        QCOMPARE(controller->vaultStateOf(second), 0);
        // Open: ticked, taken away, and that is remembered.
        QVERIFY(QMetaObject::invokeMethod(ask, "askVault", Q_ARG(QVariant, second)));
        QVERIFY(box->isEnabled() && box->property("checked").toBool());
        QMetaObject::invokeMethod(box, "toggle");
        QMetaObject::invokeMethod(box, "toggled");
        QMetaObject::invokeMethod(ask, "reject");
        QVERIFY(!controller->remembersFor(second));
        opens(second, QStringLiteral("two"));
        QVERIFY(!box->property("checked").toBool());
        QTRY_VERIFY(is(second) && controller->isIdle());
        controller->switchToVault(first);
        QTRY_VERIFY(is(first));
        QCOMPARE(controller->vaultStateOf(second), 0); // not kept
        // Ticked: kept in the primary, once, and Closed from then on.
        controller->setRemembersFor(second, true);
        opens(second, QStringLiteral("two"));
        QTRY_VERIFY(is(second));
        controller->switchToVault(first);
        QTRY_VERIFY(is(first) && controller->vaultStateOf(second) == 1);
        QVERIFY(controller->opensAtOnce(second));
        controller->lock();
        controller->unlock(QStringLiteral("pass"));
        QTRY_VERIFY(is(first));
        QCOMPARE(controller->vaultStateOf(second), 1); // out of the primary's own file

        // The crown moves; nothing else. The old primary's keeping is its own.
        QMetaObject::invokeMethod(settings()->findChild<QObject*>(QStringLiteral("primaryWindow")), "open");
        settings()->findChild<QObject*>(QStringLiteral("primaryWindow"))->setProperty("vaultFile", second);
        QMetaObject::invokeMethod(settings()->findChild<QObject*>(QStringLiteral("makePrimaryButton")), "clicked");
        QCOMPARE(controller->primaryVault(), second);
        QCOMPARE(controller->vaultStateOf(second), 0);
        controller->lock();
        opens(second, QStringLiteral("two"));
        QTRY_VERIFY(is(second));
        QCOMPARE(controller->vaultStateOf(first), 0); // the new one knows nothing yet
        opens(first, QStringLiteral("pass"));
        QVERIFY(box->property("checked").toBool());
        QTRY_VERIFY(is(first));
        QTRY_VERIFY(controller->vaultStateOf(second) == 1);
        controller->switchToVault(second);
        QTRY_VERIFY(is(second) && controller->vaultStateOf(first) == 1);
        // Back: what the first kept before is used again, without asking.
        controller->makePrimary(first);
        controller->lock();
        controller->switchToVault(first);
        controller->unlock(QStringLiteral("pass"));
        QTRY_VERIFY(is(first));
        QCOMPARE(controller->vaultStateOf(second), 1);
        QMetaObject::invokeMethod(settings(), "close");
    }

    // The eye lies over the typed text: a click shows the password, another hides it.
    void the_eye_is_clicked()
    {
        controller->lock();
        QTRY_COMPARE(controller->state(), State::Locked);
        QQuickItem* field = part("masterPasswordField");
        QTRY_VERIFY(field && field->hasActiveFocus());
        type(QStringLiteral("abc"));
        QCOMPARE(field->property("text").toString(), QStringLiteral("abc"));
        window->grabWindow(); // laid out
        click(seen(field, QStringLiteral("passwordEye")));
        QVERIFY(field->property("revealed").toBool());
        click(seen(field, QStringLiteral("passwordEye")));
        QVERIFY(!field->property("revealed").toBool());
        // Beside it, a click still goes to the text.
        QTest::mouseClick(window, Qt::LeftButton, {}, field->mapToScene(QPointF(20, field->height() / 2)).toPoint());
        QVERIFY(field->hasActiveFocus());
    }

    // A kept password that no longer opens its vault: the reason in Settings too.
    void an_outdated_kept_password_is_said_in_settings()
    {
        const QString primary = controller->vaultPath();
        const QString second = folder.filePath(QStringLiteral("kept.chronovault"));
        const QString other = folder.filePath(QStringLiteral("other.chronovault"));
        controller->createVault(QUrl::fromLocalFile(second), QStringLiteral("two"), QStringLiteral("two"), QUrl(), true);
        QTRY_VERIFY(controller->vaultPath() == second && controller->state() == State::Unlocked && controller->isIdle());
        controller->switchToVault(primary);
        QTRY_VERIFY(controller->vaultPath() == primary && controller->state() == State::Unlocked && controller->isIdle());
        // As another device would: the same vault with another master password.
        controller->createVault(QUrl::fromLocalFile(other), QStringLiteral("new"), QStringLiteral("new"));
        QTRY_VERIFY(controller->vaultPath() == other && controller->state() == State::Unlocked && controller->isIdle());
        controller->switchToVault(primary);
        QTRY_VERIFY(controller->vaultPath() == primary && controller->state() == State::Unlocked && controller->isIdle());
        QVERIFY(QFile::remove(second) && QFile::copy(other, second));
        QVERIFY(controller->opensAtOnce(second));
        QMetaObject::invokeMethod(settings(), "showWindow");
        QMetaObject::invokeMethod(settings(), "openKnown", Q_ARG(QVariant, second));
        QTRY_VERIFY(!settings()->property("problem").toString().isEmpty());
        QCOMPARE(controller->state(), State::Locked);
        // The next Open asks its master password, and the reason is gone.
        QMetaObject::invokeMethod(settings(), "openKnown", Q_ARG(QVariant, second));
        QVERIFY(settings()->property("problem").toString().isEmpty());
        QTRY_VERIFY(settings()->findChild<QQuickWindow*>(QStringLiteral("unlockFirstWindow"))->isVisible());
        QMetaObject::invokeMethod(settings()->findChild<QObject*>(QStringLiteral("unlockFirstWindow")), "reject");
        QMetaObject::invokeMethod(settings(), "close");
    }

    void a_vault_is_opened_and_locked_in_its_row()
    {
        const QString first = QStringLiteral("a_vault_is_opened_and_locked_in_its_row");
        controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("second.chronovault"))),
                                QStringLiteral("two"), QStringLiteral("two"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QTRY_VERIFY(controller->isIdle());
        const QString second = QStringLiteral("second");
        QMetaObject::invokeMethod(settings(), "showWindow");
        QTRY_VERIFY(settings()->isExposed());
        settings()->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 2);
        QTRY_VERIFY(there(QStringLiteral("padlock_") + first) && there(QStringLiteral("padlock_") + second));
        QVERIFY(opened(second) && !opened(first));
        QVERIFY(entry(QStringLiteral("openVault_"), first));

        // A round button of 32 by 32; a right click on the row opens its menu.
        QQuickItem* dots = there(QStringLiteral("vaultMenuButton_") + first);
        QVERIFY(dots);
        QCOMPARE(dots->width(), 32.0);
        QCOMPARE(dots->height(), 32.0);
        QObject* menu = entry(QString(), first);
        QQuickItem* lock = there(QStringLiteral("padlock_") + first);
        settings()->grabWindow(); // laid out
        QTest::mouseClick(settings(), Qt::RightButton, {}, lock->mapToScene(QPointF(40, 8)).toPoint());
        QTRY_VERIFY(menu->property("opened").toBool());
        QVERIFY(entry(QStringLiteral("openVault_"), first)->property("height").toDouble() > 0);
        QCOMPARE(entry(QStringLiteral("lockVault_"), first)->property("height").toDouble(), 0.0);
        QMetaObject::invokeMethod(menu, "close");
        QTRY_VERIFY(!menu->property("visible").toBool());
        // The open vault's has Lock.
        click(there(QStringLiteral("vaultMenuButton_") + second));
        QTRY_VERIFY(entry(QString(), second)->property("opened").toBool());
        QVERIFY(entry(QStringLiteral("lockVault_"), second)->property("height").toDouble() > 0);
        QCOMPARE(entry(QStringLiteral("openVault_"), second)->property("height").toDouble(), 0.0);
        QMetaObject::invokeMethod(entry(QString(), second), "close");
        QTRY_VERIFY(!entry(QString(), second)->property("visible").toBool());

        // The primary, its key kept from this session, opens at once.
        QVERIFY(QMetaObject::invokeMethod(entry(QStringLiteral("openVault_"), first), "triggered"));
        QTRY_VERIFY(opened(first));
        QVERIFY(!opened(second));
        // Lock, from the same menu.
        QVERIFY(QMetaObject::invokeMethod(entry(QStringLiteral("lockVault_"), first), "triggered"));
        QTRY_VERIFY(!opened(first) && !opened(second));
        // Open asks the master password; a wrong one is said, the right one opens it.
        QVERIFY(QMetaObject::invokeMethod(entry(QStringLiteral("openVault_"), second), "triggered"));
        QQuickWindow* ask = settings()->findChild<QQuickWindow*>(QStringLiteral("unlockFirstWindow"));
        QTRY_VERIFY(ask->isVisible());
        QObject* password = ask->findChild<QObject*>(QStringLiteral("askField"));
        password->setProperty("text", QStringLiteral("wrong"));
        QMetaObject::invokeMethod(ask, "go");
        QTRY_VERIFY(!ask->property("said").toString().isEmpty());
        QVERIFY(ask->isVisible() && !opened(second));
        // In the look of the start-up page (D-073), its list with every vault.
        QCOMPARE(ask->width(), 534);
        QVERIFY(ask->findChild<QObject*>(QStringLiteral("askVaultBox"))->property("isList").toBool());
        password->setProperty("text", QStringLiteral("two"));
        QMetaObject::invokeMethod(ask, "go");
        QTRY_VERIFY(!ask->isVisible());
        QTRY_VERIFY(opened(second));
        QVERIFY(!opened(first));
        QMetaObject::invokeMethod(settings(), "close");
    }
};

QTEST_MAIN(TestVaultRows)
#include "test_vault_rows.moc"
