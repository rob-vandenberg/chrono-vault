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
// test_vault_rows.cpp 0.6.102

// --- Version history ----------------------------------------------------
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
        QObject* password = ask->findChild<QObject*>(QStringLiteral("unlockFirstPassword"));
        password->setProperty("text", QStringLiteral("wrong"));
        QMetaObject::invokeMethod(ask, "unlockNow");
        QTRY_VERIFY(!ask->property("problem").toString().isEmpty());
        QVERIFY(ask->isVisible() && !opened(second));
        password->setProperty("text", QStringLiteral("two"));
        QMetaObject::invokeMethod(ask, "unlockNow");
        QTRY_VERIFY(!ask->isVisible());
        QTRY_VERIFY(opened(second));
        QVERIFY(!opened(first));
        QMetaObject::invokeMethod(settings(), "close");
    }
};

QTEST_MAIN(TestVaultRows)
#include "test_vault_rows.moc"
