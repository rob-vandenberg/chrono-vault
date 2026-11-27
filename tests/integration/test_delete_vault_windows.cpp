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
// test_delete_vault_windows.cpp 0.8.129

// --- Version history ----------------------------------------------------
// v0.8.129: Two dialogs: the password dialog first, then the warning (the owner, D-083)
// v0.8.123: The question on one line; whose password it asks; "Password" always; requires its keyfile (D-083)
// v0.8.122: Initial release: Delete… in Settings > Vaults, its three dialogs (D-083)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// Delete… in every vault's menu: why not now, in the owner's order, then the
// question with the vault's own password (the owner, D-083).
class TestDeleteVaultWindows : public IntuitiveFixture
{
    Q_OBJECT

    QQuickWindow* settings() const { return window->findChild<QQuickWindow*>(QStringLiteral("settingsWindow")); }
    QQuickItem* there(const QString& name) { return seen(settings()->contentItem(), name); }
    QObject* entry(const QString& name, const QString& vault)
    {
        QQuickItem* button = there(QStringLiteral("vaultMenuButton_") + vault);
        QObject* menu = button ? button->findChild<QObject*>(QStringLiteral("vaultMenu_") + vault) : nullptr;
        if (!menu) {
            return nullptr;
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
    QQuickWindow* dialog(const char* name) const { return settings()->findChild<QQuickWindow*>(QString::fromLatin1(name)); }
    static QString textOf(QQuickWindow* one, const char* name)
    {
        QQuickItem* label = seen(one->contentItem(), QString::fromLatin1(name));
        return label ? label->property("text").toString() : QString();
    }
    bool askDelete(const QString& vault)
    {
        QObject* item = entry(QStringLiteral("deleteVault_"), vault);
        return item && item->property("height").toDouble() > 0 && QMetaObject::invokeMethod(item, "triggered");
    }
    void closeIt(QQuickWindow* one)
    {
        QMetaObject::invokeMethod(one, "reject");
        QTRY_VERIFY(!one->isVisible());
    }

private slots:
    void the_three_dialogs_in_the_owners_order()
    {
        const QString first = QStringLiteral("the_three_dialogs_in_the_owners_order");
        const QString secondFile = folder.filePath(QStringLiteral("second.chronovault"));
        // Its password kept in the primary, so it can be Closed too.
        controller->createVault(QUrl::fromLocalFile(secondFile), QStringLiteral("two"), QStringLiteral("two"), QUrl(), true);
        QTRY_VERIFY(controller->vaultPath() == secondFile && controller->state() == State::Unlocked);
        QTRY_VERIFY(controller->isIdle());
        const QString second = QStringLiteral("second");
        QMetaObject::invokeMethod(settings(), "showWindow");
        QTRY_VERIFY(settings()->isExposed());
        settings()->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 2);
        QTRY_VERIFY(there(QStringLiteral("vaultMenuButton_") + first) && there(QStringLiteral("vaultMenuButton_") + second));
        QQuickWindow* refused = dialog("noDeleteWindow");
        QQuickWindow* question = dialog("deleteVaultWindow");
        QVERIFY(refused && question);

        // 1. The primary while another vault exists: Close only.
        QVERIFY(askDelete(first));
        QVERIFY(comesUp(refused));
        QCOMPARE(textOf(refused, "dialogText"),
                 QStringLiteral("“%1” is your primary vault and cannot be deleted while you have other vaults. To delete "
                                "it, first make another vault primary with Make primary.").arg(first));
        QVERIFY(!question->isVisible());
        QTest::keyClick(refused, Qt::Key_Escape); // a window without a field: Esc closes it
        QTRY_VERIFY(!refused->isVisible());

        // 2. Open, then closed: Close only, two whole sentences.
        QVERIFY(askDelete(second));
        QVERIFY(comesUp(refused));
        QCOMPARE(textOf(refused, "dialogText"), QStringLiteral("The vault “second” is currently open. The vault must be "
                                                                "locked before it can be deleted."));
        closeIt(refused);
        controller->closeOpenVault(secondFile);
        QTRY_COMPARE(controller->vaultStateOf(secondFile), 1);
        QVERIFY(askDelete(second));
        QVERIFY(comesUp(refused));
        QCOMPARE(textOf(refused, "dialogText"), QStringLiteral("The vault “second” is currently closed. The vault must "
                                                                "be locked before it can be deleted."));
        closeIt(refused);
        QVERIFY(QFileInfo::exists(secondFile));

        // 3. Locked: first the vault's password in the standard dialog
        // (the owner, D-083), "Password" always; the warning only after it.
        QQuickWindow* asked = dialog("deleteAskWindow");
        QVERIFY(asked);
        QObject* field = asked->findChild<QObject*>(QStringLiteral("askField"));
        QObject* ok = asked->findChild<QObject*>(QStringLiteral("askOk"));
        QVERIFY(field && ok);
        controller->lockVault(secondFile);
        QTRY_COMPARE(controller->vaultStateOf(secondFile), 0);
        QVERIFY(askDelete(second));
        QTRY_VERIFY(comesUp(asked));
        QVERIFY(!question->isVisible() && !refused->isVisible());
        QCOMPARE(asked->title(), QStringLiteral("Delete the vault"));
        QCOMPARE(asked->property("heading").toString(), QStringLiteral("Enter the password of the vault you are about to delete."));
        QCOMPARE(asked->property("okText").toString(), QStringLiteral("Delete"));
        QCOMPARE(field->property("placeholderText").toString(), QStringLiteral("Password"));
        QVERIFY(!asked->property("fingerWanted").toBool());
        // A wrong password: the password dialog stays and says so; no warning.
        field->setProperty("text", QStringLiteral("wrong"));
        QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
        QTRY_VERIFY(controller->askTrouble());
        QVERIFY(asked->isVisible() && !question->isVisible());
        // The right one: the warning, nothing deleted yet.
        field->setProperty("text", QStringLiteral("two"));
        QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
        QTRY_VERIFY(comesUp(question));
        QTRY_VERIFY(!asked->isVisible());
        QVERIFY(QFileInfo::exists(secondFile));

        // The warning: the first line bold, the question on one line.
        QQuickItem* warning = seen(question->contentItem(), QStringLiteral("deleteWarning"));
        QCOMPARE(warning->property("text").toString(), QStringLiteral("WARNING: You are about to delete the vault “second”."));
        QVERIFY(warning->property("font").value<QFont>().bold());
        const QString said = textOf(question, "dialogText");
        QVERIFY(said.contains(QStringLiteral("The vault and all information it contains will be permanently lost!")));
        QVERIFY(said.contains(QStringLiteral("This action cannot be undone. Are you sure you want to delete the vault?")));
        QQuickItem* paragraphs = seen(question->contentItem(), QStringLiteral("dialogText"));
        QVERIFY2(paragraphs->implicitWidth() <= paragraphs->width() + 0.5,
                 qPrintable(QStringLiteral("%1 > %2").arg(paragraphs->implicitWidth()).arg(paragraphs->width())));
        QVERIFY(question->width() > 440); // wider than before, for the question

        // Delete in red at the far left; Cancel the default at the right.
        auto* remove = qobject_cast<QQuickItem*>(question->findChild<QObject*>(QStringLiteral("deleteVaultButton")));
        QVERIFY(remove);
        QCOMPARE(remove->property("palette").value<QObject*>()->property("buttonText").value<QColor>(), QColor("#ff6b6b"));
        QVERIFY(std::abs(remove->mapToScene(QPointF(0, 0)).x() - 16) <= 1);
        QVERIFY(remove->isEnabled());
        auto* cancel = qvariant_cast<QQuickItem*>(question->property("enterTarget"));
        QVERIFY(cancel && cancel->property("text").toString() == QStringLiteral("Cancel"));
        QVERIFY(cancel->mapToScene(QPointF(0, 0)).x() > question->width() / 2);

        // Enter never deletes, not on Delete itself.
        remove->forceActiveFocus();
        QTest::keyClick(question, Qt::Key_Return);
        QTest::qWait(200);
        QVERIFY(question->isVisible());
        QVERIFY(QFileInfo::exists(secondFile));
        // Esc is Cancel: nothing deleted, nothing proven left behind.
        QTest::keyClick(question, Qt::Key_Escape);
        QTRY_VERIFY(!question->isVisible());
        QVERIFY(QFileInfo::exists(secondFile));
        controller->deleteVault(secondFile);
        QTest::qWait(200);
        QVERIFY(QFileInfo::exists(secondFile));

        // Enter is Cancel, the default: nothing deleted.
        QVERIFY(askDelete(second));
        QTRY_VERIFY(comesUp(asked));
        field->setProperty("text", QStringLiteral("two"));
        QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
        QTRY_VERIFY(comesUp(question));
        QTest::keyClick(question, Qt::Key_Return);
        QTRY_VERIFY(!question->isVisible());
        QVERIFY(QFileInfo::exists(secondFile));

        // Again, and Delete: deleted, the warning closes, the row goes.
        QVERIFY(askDelete(second));
        QTRY_VERIFY(comesUp(asked));
        field->setProperty("text", QStringLiteral("two"));
        QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
        QTRY_VERIFY(comesUp(question));
        QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
        QTRY_VERIFY(!question->isVisible());
        QVERIFY(!QFileInfo::exists(secondFile));
        QCOMPARE(controller->vaults(), QStringList{controller->primaryVault()});
        QTRY_VERIFY(!there(QStringLiteral("vaultMenuButton_") + second));
        QMetaObject::invokeMethod(settings(), "close");
    }

    // The only vault, locked: deleted, the start page (the owner, Q10).
    void the_only_vault_leads_to_the_start_page()
    {
        const QString only = QStringLiteral("the_only_vault_leads_to_the_start_page");
        const QString file = controller->vaultPath();
        controller->lock();
        QTRY_COMPARE(controller->state(), State::Locked);
        QMetaObject::invokeMethod(settings(), "showWindow");
        QTRY_VERIFY(settings()->isExposed());
        settings()->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 2);
        QTRY_VERIFY(there(QStringLiteral("vaultMenuButton_") + only));
        QQuickWindow* question = dialog("deleteVaultWindow");
        QQuickWindow* asked = dialog("deleteAskWindow");
        QVERIFY(askDelete(only));
        QTRY_VERIFY(comesUp(asked));
        // "Password" for the primary too (the owner, D-083).
        QObject* field = asked->findChild<QObject*>(QStringLiteral("askField"));
        QCOMPARE(field->property("placeholderText").toString(), QStringLiteral("Password"));
        field->setProperty("text", QStringLiteral("pass"));
        QVERIFY(QMetaObject::invokeMethod(asked->findChild<QObject*>(QStringLiteral("askOk")), "clicked"));
        QTRY_VERIFY(comesUp(question));
        QVERIFY(QMetaObject::invokeMethod(question->findChild<QObject*>(QStringLiteral("deleteVaultButton")), "clicked"));
        QTRY_VERIFY(!question->isVisible());
        QVERIFY(!QFileInfo::exists(file));
        QVERIFY(controller->vaults().isEmpty());
        QCOMPARE(controller->state(), State::NoVault);
        QMetaObject::invokeMethod(settings(), "close");
        QTRY_VERIFY(part("createNewButton") && part("createNewButton")->isVisible());
    }
};

QTEST_MAIN(TestDeleteVaultWindows)
#include "test_delete_vault_windows.moc"
