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
// test_delete_vault_windows.cpp 0.8.130

// --- Version history ----------------------------------------------------
// v0.8.130: The warning first, then the password dialog (the owner, D-083)
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
    // Forget… and Disconnect… only on a locked vault, refused as Delete (the owner, D-060).
    void refusedAs(QQuickWindow* refused, const QString& vaultFile, const QString& action, const QString& title,
                   const QString& said)
    {
        QVariant refuses;
        QVERIFY(QMetaObject::invokeMethod(settings(), "refuses", Q_RETURN_ARG(QVariant, refuses), Q_ARG(QVariant, action),
                                          Q_ARG(QVariant, vaultFile), Q_ARG(QVariant, QStringLiteral("WebDAV"))));
        QVERIFY(refuses.toBool());
        QVERIFY(comesUp(refused));
        QCOMPARE(refused->title(), title);
        QCOMPARE(textOf(refused, "dialogText"), said);
        QVERIFY(!dialog("forgetWindow")->isVisible() && !dialog("disconnectWindow")->isVisible());
        closeIt(refused);
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
        QQuickWindow* refused = dialog("refusalWindow");
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
        refusedAs(refused, secondFile, QStringLiteral("forget"), QStringLiteral("Forget the vault"),
                  QStringLiteral("The vault “second” is currently open. The vault must be locked before it can be forgotten."));
        refusedAs(refused, secondFile, QStringLiteral("disconnect"), QStringLiteral("Disconnect from WebDAV"),
                  QStringLiteral("The vault “second” is currently open. The vault must be locked before it can be disconnected."));
        controller->closeOpenVault(secondFile);
        QTRY_COMPARE(controller->vaultStateOf(secondFile), 1);
        QVERIFY(askDelete(second));
        QVERIFY(comesUp(refused));
        QCOMPARE(textOf(refused, "dialogText"), QStringLiteral("The vault “second” is currently closed. The vault must "
                                                                "be locked before it can be deleted."));
        closeIt(refused);
        refusedAs(refused, secondFile, QStringLiteral("forget"), QStringLiteral("Forget the vault"),
                  QStringLiteral("The vault “second” is currently closed. The vault must be locked before it can be forgotten."));
        refusedAs(refused, secondFile, QStringLiteral("disconnect"), QStringLiteral("Disconnect from WebDAV"),
                  QStringLiteral("The vault “second” is currently closed. The vault must be locked before it can be disconnected."));
        QVERIFY(QFileInfo::exists(secondFile));

        // 3. Locked: first the warning (the owner, D-083), its first line bold.
        QQuickWindow* asked = dialog("deleteAskWindow");
        QVERIFY(asked);
        QObject* field = asked->findChild<QObject*>(QStringLiteral("askField"));
        QObject* ok = asked->findChild<QObject*>(QStringLiteral("askAction"));
        QVERIFY(field && ok);
        controller->lockVault(secondFile);
        QTRY_COMPARE(controller->vaultStateOf(secondFile), 0);
        QVERIFY(askDelete(second));
        QTRY_VERIFY(comesUp(question));
        QVERIFY(!asked->isVisible() && !refused->isVisible());
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

        // Continue, plain, next to Cancel at the right; Cancel the default (the owner, D-083).
        auto* remove = qobject_cast<QQuickItem*>(question->findChild<QObject*>(QStringLiteral("deleteContinue")));
        QVERIFY(remove);
        QCOMPARE(remove->property("text").toString(), QStringLiteral("Continue"));
        QVERIFY(remove->property("palette").value<QObject*>()->property("buttonText").value<QColor>() != QColor("#ff6b6b"));
        QVERIFY(remove->isEnabled());
        auto* cancel = qvariant_cast<QQuickItem*>(question->property("enterTarget"));
        QVERIFY(cancel && cancel->property("text").toString() == QStringLiteral("Cancel"));
        const qreal continueRight = remove->mapToScene(QPointF(remove->width(), 0)).x();
        QVERIFY(remove->mapToScene(QPointF(0, 0)).x() > question->width() / 2);
        QVERIFY(std::abs(cancel->mapToScene(QPointF(0, 0)).x() - continueRight - 8) <= 1); // 8 apart

        // Enter on the window presses Cancel: nothing asked, nothing deleted.
        cancel->forceActiveFocus();
        QTest::keyClick(question, Qt::Key_Return);
        QTRY_VERIFY(!question->isVisible());
        QVERIFY(!asked->isVisible());
        QVERIFY(askDelete(second));
        QTRY_VERIFY(comesUp(question));
        QTest::keyClick(question, Qt::Key_Escape);
        QTRY_VERIFY(!question->isVisible());
        QVERIFY(!asked->isVisible());
        QVERIFY(QFileInfo::exists(secondFile));

        // Delete on the warning: then the vault's password, "Password" always.
        const auto toThePassword = [&]() {
            QVERIFY(askDelete(second));
            QTRY_VERIFY(comesUp(question));
            QVERIFY(QMetaObject::invokeMethod(remove, "clicked"));
            QTRY_VERIFY(comesUp(asked));
            QTRY_VERIFY(!question->isVisible());
            QVERIFY(QFileInfo::exists(secondFile)); // not yet
        };
        toThePassword();
        QCOMPARE(asked->title(), QStringLiteral("Delete the vault"));
        QCOMPARE(asked->property("heading").toString(), QStringLiteral("Enter the password of the vault you are about to delete."));
        QCOMPARE(asked->property("okText").toString(), QStringLiteral("Delete"));
        QCOMPARE(field->property("placeholderText").toString(), QStringLiteral("Password"));
        QVERIFY(!asked->property("fingerWanted").toBool());
        // Delete red, apart at the left; Cancel the default at the right; no OK at the right.
        auto* action = qobject_cast<QQuickItem*>(ok);
        QVERIFY(action && action->isVisible());
        QCOMPARE(action->property("text").toString(), QStringLiteral("Delete"));
        QCOMPARE(action->property("palette").value<QObject*>()->property("buttonText").value<QColor>(), QColor("#ff6b6b"));
        QVERIFY(std::abs(action->mapToScene(QPointF(0, 0)).x() - 16) <= 1);
        QVERIFY(!qobject_cast<QQuickItem*>(asked->findChild<QObject*>(QStringLiteral("askOk")))->isVisible());
        auto* askCancel = qvariant_cast<QQuickItem*>(asked->property("enterTarget"));
        QVERIFY(askCancel && askCancel->property("text").toString() == QStringLiteral("Cancel"));
        QVERIFY(askCancel->mapToScene(QPointF(0, 0)).x() > asked->width() / 2);
        // Enter in the field, the right password typed, presses Cancel: nothing deleted.
        field->setProperty("text", QStringLiteral("two"));
        qobject_cast<QQuickItem*>(field)->forceActiveFocus();
        QTest::keyClick(asked, Qt::Key_Return);
        QTRY_VERIFY(!asked->isVisible());
        QTest::qWait(200);
        QVERIFY(QFileInfo::exists(secondFile));
        toThePassword();
        // Cancel there: nothing deleted, nothing proven left behind.
        QVERIFY(QMetaObject::invokeMethod(asked, "reject"));
        QTRY_VERIFY(!asked->isVisible());
        controller->deleteVault(secondFile);
        QTest::qWait(200);
        QVERIFY(QFileInfo::exists(secondFile));

        // A wrong password: the dialog stays and says so; the right one deletes.
        toThePassword();
        field->setProperty("text", QStringLiteral("wrong"));
        QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
        QTRY_VERIFY(controller->askTrouble());
        QVERIFY(asked->isVisible());
        QVERIFY(QFileInfo::exists(secondFile));
        field->setProperty("text", QStringLiteral("two"));
        QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
        QTRY_VERIFY(!asked->isVisible());
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
        QTRY_VERIFY(comesUp(question));
        QVERIFY(QMetaObject::invokeMethod(question->findChild<QObject*>(QStringLiteral("deleteContinue")), "clicked"));
        QTRY_VERIFY(comesUp(asked));
        // "Password" for the primary too (the owner, D-083).
        QObject* field = asked->findChild<QObject*>(QStringLiteral("askField"));
        QCOMPARE(field->property("placeholderText").toString(), QStringLiteral("Password"));
        field->setProperty("text", QStringLiteral("pass"));
        QVERIFY(QMetaObject::invokeMethod(asked->findChild<QObject*>(QStringLiteral("askAction")), "clicked"));
        QTRY_VERIFY(!asked->isVisible());
        QVERIFY(!QFileInfo::exists(file));
        QVERIFY(controller->vaults().isEmpty());
        QCOMPARE(controller->state(), State::NoVault);
        QMetaObject::invokeMethod(settings(), "close");
        QTRY_VERIFY(part("createNewButton") && part("createNewButton")->isVisible());
    }
};

QTEST_MAIN(TestDeleteVaultWindows)
#include "test_delete_vault_windows.moc"
