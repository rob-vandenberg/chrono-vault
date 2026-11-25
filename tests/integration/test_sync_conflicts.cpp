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
#include "settings_files.h"
#include "vault_controller.h"
#include "vault_file.h"

#include <QDir>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_sync_conflicts.cpp 0.6.103

// --- Version history ----------------------------------------------------
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.5.82: The program's own style; the free room at a button's sides
// v0.5.81: Initial release (D-061)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

QList<QQuickItem*> visualItems(QQuickItem* root, const QString& name)
{
    QList<QQuickItem*> found;
    if (!root) {
        return found;
    }
    if (root->objectName() == name) {
        found << root;
    }
    for (QQuickItem* child : root->childItems()) {
        found << visualItems(child, name);
    }
    return found;
}

const QUuid bankId = QUuid(QStringLiteral("{11111111-1111-4111-8111-111111111111}"));
const QString bank = bankId.toString(QUuid::WithoutBraces);

Item makeBank(const QString& title, const QString& mail, const QString& password)
{
    Item item;
    item.id = bankId;
    item.title = title;
    item.created = QDateTime::fromString(QStringLiteral("2026-01-01T10:00:00.000Z"), Qt::ISODateWithMs);
    item.modified = item.created;
    Field user = makeField(QStringLiteral("username"), QString(), FieldKind::Username, QStringLiteral("rob"));
    user.uid = 1;
    Field email = makeField(QString(), QStringLiteral("Mail"), FieldKind::Email, mail);
    email.uid = 2;
    Field secret = makeField(QStringLiteral("password"), QString(), FieldKind::Password, password);
    secret.uid = 3;
    item.fields = {user, email, secret};
    return item;
}

void putIn(const QString& file, const QList<Item>& items)
{
    const OpenResult opened = openVaultFile(file, "pass");
    ContentsResult read = contentsFromJson(opened.contents);
    read.contents.items = items;
    QCOMPARE(saveVaultFile(file, contentsToJson(read.contents), opened.key), SaveStatus::Saved);
}

Item itemIn(const QString& file)
{
    const OpenResult opened = openVaultFile(file, "pass");
    const VaultContents contents = contentsFromJson(opened.contents).contents;
    return contents.items.isEmpty() ? Item() : contents.items.constFirst();
}

} // namespace

// Settling conflicts as the two windows do (D-061).
class TestSyncConflicts : public QObject
{
    Q_OBJECT

    QTemporaryDir folder;
    QString here;
    QString other;

    // A vault "here" holding one bank, and another vault "other" holding it
    // with another mail address and password, imported into "here".
    void importOther(VaultController& controller)
    {
        controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        controller.createVault(QUrl::fromLocalFile(here), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        QTRY_VERIFY(controller.isIdle());
        QVERIFY(QFile::copy(here, other));
        putIn(other, {makeBank(QStringLiteral("Bank"), QStringLiteral("theirs@example.com"), QStringLiteral("theirs"))});
        putIn(here, {makeBank(QStringLiteral("Bank"), QStringLiteral("mine@example.com"), QStringLiteral("mine"))});
        controller.lock();
        controller.unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        importAgain(controller);
        QCOMPARE(controller.conflictCount(), 1);
    }
    void importAgain(VaultController& controller)
    {
        QSignalSpy read(&controller, &VaultController::importRead);
        controller.readVaultImport(QUrl::fromLocalFile(other), QStringLiteral("pass"));
        QVERIFY(read.wait(10000));
        QVERIFY(controller.confirmImport());
        QTRY_VERIFY(controller.isIdle());
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        applyStyle(); // the program's own, as at its start
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_sync_conflicts"));
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        QVERIFY(folder.isValid());
        here = folder.filePath(QString::fromLatin1(QTest::currentTestFunction()) + QStringLiteral(" here.chronovault"));
        other = folder.filePath(QString::fromLatin1(QTest::currentTestFunction()) + QStringLiteral(" other.chronovault"));
    }
    void cleanup()
    {
        SettingsFile().clear();
        VaultsFile().clear();
    }

    void every_field_is_shown_and_a_split_keeps_both()
    {
        VaultController controller;
        importOther(controller);
        const QVariantMap conflict = controller.conflictOf(bank);
        QVERIFY(QRegularExpression(QStringLiteral("^\\d{4}-\\d\\d-\\d\\d \\d\\d:\\d\\d$"))
                    .match(conflict.value(QStringLiteral("found")).toString()).hasMatch());
        QStringList labels;
        for (const QVariant& row : conflict.value(QStringLiteral("allRows")).toList()) {
            const QVariantMap one = row.toMap();
            labels.append(one.value(QStringLiteral("label")).toString() +
                          (one.value(QStringLiteral("differs")).toBool() ? QStringLiteral("*") : QString()) +
                          (one.value(QStringLiteral("canSplit")).toBool() ? QStringLiteral("+") : QString()));
        }
        QCOMPARE(labels, (QStringList{QStringLiteral("Title"), QStringLiteral("User name"), QStringLiteral("Mail*+"),
                                      QStringLiteral("Password*"), QStringLiteral("Category")}));
        controller.settleConflict(bank, {QStringLiteral("both"), QStringLiteral("right")});
        QTRY_VERIFY(controller.isIdle());
        QCOMPARE(controller.conflictCount(), 0);
        const Item item = itemIn(here);
        QCOMPARE(item.fields.size(), 4);
        QCOMPARE(item.fields[1].value, QStringLiteral("mine@example.com"));
        QCOMPARE(item.fields[2].value, QStringLiteral("theirs@example.com"));
        QCOMPARE(item.fields[2].label, QStringLiteral("Mail"));
        QCOMPARE(item.fields[3].value, QStringLiteral("theirs"));
    }

    void retry_sync_reads_the_source_again()
    {
        VaultController controller;
        importOther(controller);
        QSignalSpy retried(&controller, &VaultController::retried);
        QSignalSpy asked(&controller, &VaultController::retryPasswordNeeded);
        QVERIFY(controller.conflictOf(bank).value(QStringLiteral("canRetry")).toBool());
        // Nothing changed there: the item is as it was.
        const Item before = itemIn(here);
        controller.retryConflicts({bank});
        QVERIFY(retried.wait(10000));
        QVERIFY(controller.isIdle());
        QVERIFY(itemIn(here) == before);
        // The source changed: the mail address is the same now.
        putIn(other, {makeBank(QStringLiteral("Bank"), QStringLiteral("mine@example.com"), QStringLiteral("newer"))});
        controller.retryConflicts({bank});
        QVERIFY(retried.wait(10000));
        QVERIFY(!retried.constLast().at(1).toBool());
        QCOMPARE(asked.size(), 0); // its key is at hand until the lock
        QCOMPARE(controller.conflictOf(bank).value(QStringLiteral("rows")).toList().size(), 1);
        QCOMPARE(controller.conflictOf(bank).value(QStringLiteral("rows")).toList()[0].toMap().value(QStringLiteral("right")).toString(),
                 QStringLiteral("newer"));
        QTRY_VERIFY(controller.isIdle());
        // After a lock the password is asked; a wrong one is asked again.
        controller.lock();
        controller.unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        controller.retryConflicts({bank});
        QCOMPARE(asked.size(), 1);
        const QString source = asked.constLast().at(0).toString();
        controller.retryFromVault(source, {bank}, QStringLiteral("wrong"));
        QVERIFY(asked.wait(10000));
        QVERIFY(!asked.constLast().at(3).toString().isEmpty());
        // The item no longer there: the conflict stays.
        putIn(other, {});
        controller.retryFromVault(source, {bank}, QStringLiteral("pass"));
        QVERIFY(retried.wait(10000));
        QVERIFY(retried.constLast().at(1).toBool());
        QCOMPARE(controller.conflictCount(), 1);
        QTRY_VERIFY(controller.isIdle());
        // No longer differing: no conflict.
        putIn(other, {itemIn(here)});
        controller.retryConflicts({bank});
        QVERIFY(retried.wait(10000));
        QCOMPARE(controller.conflictCount(), 0);
        QTRY_VERIFY(controller.isIdle());
    }

    void retry_sync_from_a_source_that_is_gone_keeps_the_conflict()
    {
        VaultController controller;
        importOther(controller);
        QVERIFY(QFile::remove(other));
        QSignalSpy retried(&controller, &VaultController::retried);
        controller.retryConflicts({bank});
        QVERIFY(retried.wait(10000));
        QVERIFY(retried.constLast().at(1).toBool());
        QCOMPARE(controller.conflictOf(bank).value(QStringLiteral("rows")).toList().size(), 2);
    }

    void retry_sync_of_a_sync_reads_the_vault_again()
    {
        VaultController controller;
        controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        controller.createVault(QUrl::fromLocalFile(here), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        QTRY_VERIFY(controller.isIdle());
        Item item = makeBank(QStringLiteral("Bank"), QString(), QString());
        MergeNote note;
        note.property = QStringLiteral("title");
        note.otherValue = QStringLiteral("The bank");
        item.mergeNotes = {note};
        putIn(here, {item});
        controller.lock();
        controller.unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        QCOMPARE(controller.conflictCount(), 1);
        QSignalSpy retried(&controller, &VaultController::retried);
        controller.retryConflicts({bank}); // nothing changed there
        QVERIFY(retried.wait(10000));
        QVERIFY(!retried.constLast().at(1).toBool());
        QCOMPARE(controller.conflictCount(), 1);
        QVERIFY(QFile::rename(here, other)); // not reachable
        controller.retryConflicts({bank});
        QVERIFY(retried.wait(10000));
        QVERIFY(retried.constLast().at(1).toBool());
        QCOMPARE(controller.conflictCount(), 1);
        QVERIFY(QFile::rename(other, here));
        item.mergeNotes.clear(); // settled on another device
        putIn(here, {item});
        controller.retryConflicts({bank});
        QTRY_COMPARE(controller.conflictCount(), 0);
    }

    // Deleted on the other device, changed here: kept here, and the other
    // device, which still holds the deletion, gets it back without asking.
    void keep_here_only_reaches_the_other_device()
    {
        VaultController controller;
        controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        controller.createVault(QUrl::fromLocalFile(here), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        QTRY_VERIFY(controller.isIdle());
        const Item item = makeBank(QStringLiteral("Bank"), QString(), QString());
        putIn(here, {item});
        controller.lock();
        controller.unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        VaultContents deviceB;
        deviceB.deletedItems = {DeletedItem{bankId, QDateTime::currentDateTimeUtc().addSecs(-60)}};
        {
            const OpenResult opened = openVaultFile(here, "pass");
            QCOMPARE(saveVaultFile(here, contentsToJson(deviceB), opened.key), SaveStatus::Saved);
        }
        QVariantMap mine = controller.item(bank);
        mine.insert(QStringLiteral("title"), QStringLiteral("My bank"));
        QVERIFY(controller.saveItem(mine));
        QTRY_VERIFY(controller.isIdle());
        QTRY_COMPARE(controller.conflictCount(), 1);
        QVERIFY(controller.conflictOf(bank).value(QStringLiteral("canKeepHere")).toBool());
        controller.settleConflicts({bank}, QStringLiteral("keep"));
        QTRY_VERIFY(controller.isIdle());
        QCOMPARE(controller.conflictCount(), 0);
        // Device B changed something else meanwhile and syncs now.
        const VaultContents base = deviceB;
        deviceB.items = {makeBank(QStringLiteral("Other"), QString(), QString())};
        deviceB.items[0].id = QUuid::createUuid();
        const OpenResult opened = openVaultFile(here, "pass");
        const MergeResult merged = mergeContents(&base, deviceB, contentsFromJson(opened.contents).contents,
                                                 QDateTime::currentDateTimeUtc(), QStringLiteral("B"));
        QCOMPARE(merged.contents.items.size(), 2);
        QCOMPARE(merged.itemsBack, 0);
        QCOMPARE(merged.itemsChangedOnBoth, 0);
    }

    // The two windows, driven as a person would, without a QML error.
    void the_windows()
    {
        QQmlApplicationEngine engine;
        QList<QQmlError> problems;
        connect(&engine, &QQmlEngine::warnings, this, [&problems](const QList<QQmlError>& some) { problems += some; });
        engine.loadFromModule("ChronoVault", "Main");
        QObject* window = engine.rootObjects().constFirst();
        showMainWindow(qobject_cast<QWindow*>(window), false, false);
        auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
        importOther(*controller);
        auto* list = qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QStringLiteral("syncConflictsWindow")));
        auto* single = qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QStringLiteral("conflictWindow")));
        QVERIFY(list && single);
        QVERIFY(QMetaObject::invokeMethod(list, "open"));
        QTRY_COMPARE(visualItems(list->contentItem(), QStringLiteral("conflictRow_0")).size(), 1);
        const auto button = [](QQuickWindow* in, const char* name) {
            return visualItems(in->contentItem(), QString::fromLatin1(name)).constFirst();
        };
        // 5 pixels free between a button's edge and its text (the owner).
        for (const char* name : {"useLeftButton", "keepHereButton", "invertSelectionButton"}) {
        QVERIFY2(button(list, name)->property("leftPadding").toReal() >= 5 + 2, name);
            QVERIFY2(button(list, name)->property("rightPadding").toReal() >= 5 + 2, name);
            QVERIFY2(button(list, name)->width() >= button(list, name)->property("implicitWidth").toReal(), name);
        }
        // Nothing ticked: every button greyed, none hidden.
        for (const char* name : {"useLeftButton", "useRightButton", "retryButton", "keepHereButton", "keepBothButton"}) {
            QVERIFY2(button(list, name)->isVisible() && !button(list, name)->isEnabled(), name);
        }
        QVERIFY(QMetaObject::invokeMethod(button(list, "invertSelectionButton"), "clicked"));
        QTRY_VERIFY(button(list, "retryButton")->isEnabled());
        QVERIFY(button(list, "keepBothButton")->isEnabled());
        QVERIFY(!button(list, "keepHereButton")->isEnabled()); // an import's
        // A border dragged: the two columns at its sides change, the sum stays.
        const double name = button(list, "conflictsHeader_1")->width();
        const double fields = button(list, "conflictsHeader_2")->width();
        QVERIFY(QMetaObject::invokeMethod(list, "dragBorder", Q_ARG(QVariant, 1), Q_ARG(QVariant, 20)));
        QTRY_COMPARE(button(list, "conflictsHeader_1")->width(), name + 20);
        QCOMPARE(button(list, "conflictsHeader_2")->width(), fields - 20);

        // By keys (D-062): Ctrl+A ticks all, the number pad's Enter opens.
        list->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), list);
        QTRY_COMPARE(button(list, "conflictsList")->property("currentIndex").toInt(), 0);
        button(list, "conflictsList")->forceActiveFocus();
        QVERIFY(QMetaObject::invokeMethod(button(list, "invertSelectionButton"), "clicked"));
        QTRY_VERIFY(!button(list, "retryButton")->isEnabled());
        QTest::keyClick(list, Qt::Key_A, Qt::ControlModifier);
        QTRY_VERIFY(button(list, "retryButton")->isEnabled());
        QTest::keyClick(list, Qt::Key_End);
        QTest::keyClick(list, Qt::Key_Enter, Qt::KeypadModifier);
        QTRY_VERIFY(single->isVisible());
        QVERIFY(single->width() >= 560);
        QQuickItem* apply = button(single, "conflictApplyButton");
        QVERIFY(!apply->isEnabled());
        QVERIFY(!button(single, "conflictKeepHereButton")->isEnabled());
        const auto click = [&](const char* what) { QMetaObject::invokeMethod(button(single, what), "clicked"); };
        // The header chooses the differing rows.
        click("conflictAllRight");
        QTRY_VERIFY(button(single, "conflictRight_0")->property("checked").toBool());
        QVERIFY(apply->isEnabled());
        // Keep both sets the circles aside; a circle then decides again.
        click("conflictKeepBothButton");
        QTRY_VERIFY(!button(single, "conflictRight_0")->property("checked").toBool());
        QVERIFY(apply->isEnabled());
        click("conflictLeft_1");
        QTRY_VERIFY(!apply->isEnabled());
        // A split settles its row; Undo joins it again with its choice.
        click("conflictLeft_0");
        QTRY_VERIFY(apply->isEnabled());
        click("conflictSplit_0");
        QTRY_VERIFY(!button(single, "conflictSplit_0")->isEnabled());
        QVERIFY(apply->isEnabled());
        QVERIFY(!button(single, "conflictLeft_0")->isVisible());
        single->requestActivate();
        QTRY_VERIFY(single->isActive());
        QTest::keyClick(single, Qt::Key_Z, Qt::ControlModifier);
        QTRY_VERIFY(button(single, "conflictLeft_0")->isVisible());
        QVERIFY(button(single, "conflictLeft_0")->property("checked").toBool());
        click("conflictSplit_0");
        single->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), single);
        QTest::keyClick(single, Qt::Key_Return); // Enter applies
        QTRY_VERIFY(!single->isVisible());
        QTRY_VERIFY(controller->isIdle());
        QCOMPARE(itemIn(here).fields.size(), 4);
        QCOMPARE(itemIn(here).fields[3].value, QStringLiteral("mine"));
        QCOMPARE(controller->conflictCount(), 0);

        // Retry sync is carried out by Apply, and asks the password when it must.
        importAgain(*controller);
        controller->lock();
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QVERIFY(QMetaObject::invokeMethod(list, "showItem", Q_ARG(QVariant, bank)));
        QTRY_VERIFY(single->isVisible());
        click("conflictRetryButton");
        QVERIFY(QMetaObject::invokeMethod(button(single, "conflictApplyButton"), "clicked"));
        auto* password = qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QStringLiteral("retryPasswordWindow")));
        QVERIFY(password);
        QTRY_VERIFY(password->isVisible());
        button(password, "askField")->setProperty("text", QStringLiteral("pass"));
        QVERIFY(QMetaObject::invokeMethod(password, "go"));
        QTRY_VERIFY(!button(list, "conflictsSaid")->property("text").toString().isEmpty());
        QTRY_VERIFY(controller->isIdle());
        QVERIFY(QMetaObject::invokeMethod(list, "reject"));
        // Closed as it was dragged: its columns are kept.
        QVERIFY(!controller->windowValue(QStringLiteral("syncConflicts/columns")).isEmpty());
        QString said;
        for (const QQmlError& problem : std::as_const(problems)) {
            said += problem.toString() + QLatin1Char('\n');
        }
        QVERIFY2(said.isEmpty(), qPrintable(said));
        controller->lock();
    }
};

QTEST_MAIN(TestSyncConflicts)
#include "test_sync_conflicts.moc"
