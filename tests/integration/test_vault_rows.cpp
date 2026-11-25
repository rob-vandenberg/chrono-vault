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
// test_vault_rows.cpp 0.6.100

// --- Version history ----------------------------------------------------
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
