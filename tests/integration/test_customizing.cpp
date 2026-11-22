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
#include "vault_controller.h"
#include "vault_file.h"

#include <QFile>
#include "settings_files.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_customizing.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.14: Initial release: categories and templates, tags, favourites and attachments (D-027, D-028, D-029)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};

QStringList idsOf(const QVariantList& list, const char* key = "id")
{
    QStringList ids;
    for (const QVariant& one : list) {
        ids << one.toMap().value(QString::fromLatin1(key)).toString();
    }
    return ids;
}

QVariantMap findIn(const QVariantList& list, const QString& id, const char* key = "id")
{
    for (const QVariant& one : list) {
        if (one.toMap().value(QString::fromLatin1(key)).toString() == id) {
            return one.toMap();
        }
    }
    return {};
}

QStringList shownTitles(VaultController& controller)
{
    QStringList titles;
    ItemListModel* model = controller.items();
    for (int row = 0; row < model->rowCount(); ++row) {
        titles << model->data(model->index(row), ItemListModel::TitleRole).toString();
    }
    return titles;
}

} // namespace

// What the person makes their own: categories and templates, tags,
// favourites and attachments, each kept in the vault through a lock.
class TestCustomizing : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void the_built_in_categories_and_templates_are_there_and_stay();
    void an_own_category_is_made_renamed_hidden_and_deleted_only_when_empty();
    void an_own_template_is_made_copied_changed_and_deleted_and_its_items_keep_their_fields();
    void an_item_from_any_template_is_saved_in_its_category();
    void tags_nest_narrow_the_list_and_are_renamed_and_removed_everywhere();
    void a_favourite_is_kept_and_narrows_the_list();
    void an_attachment_is_kept_in_the_vault_and_saved_where_the_person_chooses();
    void a_file_too_large_or_missing_is_refused_and_said();
    void an_attachment_not_saved_with_its_item_is_dropped_at_the_lock();
    void a_save_with_attachments_interrupted_at_any_step_leaves_the_old_vault_whole();

private:
    std::unique_ptr<VaultController> unlockedController();
    void lockAndUnlock(VaultController& controller);
    QString saveItem(VaultController& controller, QVariantMap item, const QString& title);

    QTemporaryDir* folder = nullptr;
    QString vaultPath;
};

void TestCustomizing::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_customizing"));
}

void TestCustomizing::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestCustomizing::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
    vaultPath = folder->filePath(QStringLiteral("Mine.chronovault"));
}

void TestCustomizing::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
    delete folder;
    folder = nullptr;
}

std::unique_ptr<VaultController> TestCustomizing::unlockedController()
{
    auto controller = std::make_unique<VaultController>();
    controller->setKdfParameters(quick);
    controller->createVault(QUrl::fromLocalFile(vaultPath), QStringLiteral("master"), QStringLiteral("master"));
    if (!QTest::qWaitFor([&controller]() { return controller->state() == State::Unlocked; }, 10000)) {
        return nullptr;
    }
    return controller;
}

void TestCustomizing::lockAndUnlock(VaultController& controller)
{
    QTRY_VERIFY(controller.isIdle());
    controller.lock();
    QCOMPARE(controller.state(), State::Locked);
    controller.unlock(QStringLiteral("master"));
    QTRY_COMPARE(controller.state(), State::Unlocked);
}

QString TestCustomizing::saveItem(VaultController& controller, QVariantMap item, const QString& title)
{
    item.insert(QStringLiteral("title"), title);
    if (!controller.saveItem(item)) {
        return {};
    }
    return item.value(QStringLiteral("id")).toString();
}

void TestCustomizing::the_built_in_categories_and_templates_are_there_and_stay()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    QCOMPARE(idsOf(controller->categories()),
             QStringList({"login", "card", "identity", "note", "password", "finance", "computer", "misc"}));
    QCOMPARE(controller->templates().size(), 13);
    QVERIFY(!controller->deleteCategory(QStringLiteral("login")));
    QVERIFY(!controller->deleteTemplate(QStringLiteral("login")));
    QCOMPARE(controller->saveTemplate({{"id", "login"}, {"name", "Mine"}, {"categoryId", "login"}}), QString());
    QCOMPARE(findIn(controller->templates(), QStringLiteral("login")).value("name").toString(), QStringLiteral("Login"));
}

void TestCustomizing::an_own_category_is_made_renamed_hidden_and_deleted_only_when_empty()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    QCOMPARE(controller->saveCategory(QString(), QStringLiteral("  ")), QString());
    QVERIFY(controller->messageIsTrouble());
    QCOMPARE(controller->saveCategory(QString(), QStringLiteral("finance")), QString()); // the name of a built-in one
    const QString cars = controller->saveCategory(QString(), QStringLiteral("Cars"));
    QVERIFY(!cars.isEmpty());
    QCOMPARE(controller->saveCategory(cars, QStringLiteral("Vehicles")), cars);

    QVariantMap item = controller->newItemFrom(QStringLiteral("note"));
    item.insert(QStringLiteral("categoryId"), cars);
    const QString id = saveItem(*controller, item, QStringLiteral("Van"));
    QVERIFY(!controller->deleteCategory(cars)); // it holds an item

    controller->setFilter(QLatin1String("category:") + cars);
    QCOMPARE(shownTitles(*controller), QStringList{QStringLiteral("Van")});
    controller->setCategoryHidden(cars, true);
    QCOMPARE(controller->filter(), QString()); // a hidden category no longer narrows the list

    lockAndUnlock(*controller);
    const QVariantMap kept = findIn(controller->categories(), cars);
    QCOMPARE(kept.value("name").toString(), QStringLiteral("Vehicles"));
    QVERIFY(kept.value("hidden").toBool());
    QCOMPARE(kept.value("count").toInt(), 1);

    controller->deleteItem(id);
    QVERIFY(controller->deleteCategory(cars));
    QVERIFY(findIn(controller->categories(), cars).isEmpty());
}

void TestCustomizing::an_own_template_is_made_copied_changed_and_deleted_and_its_items_keep_their_fields()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    QVariantMap copy = controller->copyOfTemplate(QStringLiteral("card"));
    QCOMPARE(copy.value("name").toString(), QStringLiteral("Copy of Credit card"));
    QVERIFY(copy.value("id").toString().isEmpty());
    QVERIFY(!copy.value("builtIn").toBool());

    // Its fields reordered, one taken off and one of its own added.
    QVariantList fields = copy.value("fields").toList();
    fields.move(0, 1);
    fields.removeLast();
    fields.append(QVariantMap{{"role", ""}, {"label", "Bank"}, {"kind", "multiline"}});
    copy.insert(QStringLiteral("fields"), fields);
    copy.insert(QStringLiteral("name"), QStringLiteral("Debit card"));
    const QString id = controller->saveTemplate(copy);
    QVERIFY(!id.isEmpty());

    const QVariantMap item = controller->newItemFrom(id);
    const QVariantList itemFields = item.value("fields").toList();
    QCOMPARE(itemFields.first().toMap().value("role").toString(), QStringLiteral("cardnumber"));
    QCOMPARE(itemFields.last().toMap().value("label").toString(), QStringLiteral("Bank"));
    QCOMPARE(itemFields.last().toMap().value("kind").toString(), QStringLiteral("multiline"));
    QCOMPARE(item.value("categoryId").toString(), QStringLiteral("card"));
    const QString itemId = saveItem(*controller, item, QStringLiteral("My debit card"));

    lockAndUnlock(*controller);
    QCOMPARE(findIn(controller->templates(), id).value("name").toString(), QStringLiteral("Debit card"));

    // Deleted: the item made from it keeps every field.
    QVERIFY(controller->deleteTemplate(id));
    QVERIFY(findIn(controller->templates(), id).isEmpty());
    QCOMPARE(controller->item(itemId).value("fields").toList().size(), itemFields.size());
    QCOMPARE(controller->item(itemId).value("templateName").toString(), QStringLiteral("Credit card"));
}

void TestCustomizing::an_item_from_any_template_is_saved_in_its_category()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    for (const QVariant& one : controller->templates()) {
        const QString templateId = one.toMap().value("id").toString();
        QVERIFY(!saveItem(*controller, controller->newItemFrom(templateId), templateId).isEmpty());
    }
    lockAndUnlock(*controller);
    QCOMPARE(controller->items()->rowCount(), 13);
    controller->setFilter(QStringLiteral("category:identity"));
    QCOMPARE(shownTitles(*controller), QStringList({"driving", "identity", "passport"}));
    controller->setFilter(QStringLiteral("category:computer"));
    QCOMPARE(shownTitles(*controller), QStringList({"licence", "server", "wifi"}));
}

void TestCustomizing::tags_nest_narrow_the_list_and_are_renamed_and_removed_everywhere()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    QVariantMap first = controller->newLogin();
    first.insert(QStringLiteral("tags"), QStringList{QStringLiteral(" Work / Servers "), QStringLiteral("Home")});
    saveItem(*controller, first, QStringLiteral("Router"));
    QVariantMap second = controller->newLogin();
    second.insert(QStringLiteral("tags"), QStringList{QStringLiteral("Work")});
    saveItem(*controller, second, QStringLiteral("Mail"));

    const QVariantList tags = controller->tags();
    QCOMPARE(idsOf(tags, "tag"), QStringList({"Home", "Work", "Work/Servers"}));
    QCOMPARE(findIn(tags, "Work", "tag").value("count").toInt(), 2);
    QCOMPARE(findIn(tags, "Work/Servers", "tag").value("depth").toInt(), 1);
    QCOMPARE(findIn(tags, "Work/Servers", "tag").value("name").toString(), QStringLiteral("Servers"));

    controller->setFilter(QStringLiteral("tag:Work"));
    QCOMPARE(shownTitles(*controller), QStringList({"Mail", "Router"})); // a nested tag counts
    controller->setFilter(QStringLiteral("tag:Work/Servers"));
    QCOMPARE(shownTitles(*controller), QStringList{QStringLiteral("Router")});

    QVERIFY(controller->renameTag(QStringLiteral("Work"), QStringLiteral("Office")));
    QCOMPARE(controller->filter(), QStringLiteral("tag:Office/Servers")); // the list stays on the same tag
    lockAndUnlock(*controller);
    QCOMPARE(idsOf(controller->tags(), "tag"), QStringList({"Home", "Office", "Office/Servers"}));
    QVERIFY(!controller->renameTag(QStringLiteral("Office"), QStringLiteral(" / ")));

    controller->removeTag(QStringLiteral("Office"));
    QCOMPARE(idsOf(controller->tags(), "tag"), QStringList{QStringLiteral("Home")});
    QCOMPARE(controller->items()->rowCount(), 2); // the items stay
}

void TestCustomizing::a_favourite_is_kept_and_narrows_the_list()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    const QString id = saveItem(*controller, controller->newLogin(), QStringLiteral("Bank"));
    saveItem(*controller, controller->newLogin(), QStringLiteral("Shop"));
    controller->setFavourite(id, true);
    lockAndUnlock(*controller);
    QVERIFY(controller->item(id).value("favourite").toBool());
    controller->setFilter(QStringLiteral("favourites"));
    QCOMPARE(shownTitles(*controller), QStringList{QStringLiteral("Bank")});
    controller->setFavourite(id, false);
    QCOMPARE(shownTitles(*controller), QStringList());
}

void TestCustomizing::an_attachment_is_kept_in_the_vault_and_saved_where_the_person_chooses()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    const QByteArray bytes("scan of an id card\0with a zero", 30);
    const QString source = folder->filePath(QStringLiteral("scan.pdf"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }
    QSignalSpy read(controller.get(), &VaultController::attachmentRead);
    const QString requestId = controller->readAttachment(QUrl::fromLocalFile(source));
    QVERIFY(!requestId.isEmpty());
    QTRY_COMPARE(read.size(), 1);
    QCOMPARE(read.first().at(0).toString(), requestId);
    QCOMPARE(read.first().at(1).toString(), QStringLiteral("scan.pdf"));
    QCOMPARE(read.first().at(2).toLongLong(), bytes.size());
    QVERIFY(read.first().at(3).toBool());

    QVariantMap item = controller->newItemFrom(QStringLiteral("passport"));
    item.insert(QStringLiteral("attachments"), QVariantList{QVariantMap{{"id", requestId}}});
    const QString id = saveItem(*controller, item, QStringLiteral("Passport"));
    QVERIFY(QFile::remove(source));

    lockAndUnlock(*controller);
    const QVariantList attachments = controller->item(id).value("attachments").toList();
    QCOMPARE(attachments.size(), 1);
    QCOMPARE(attachments.first().toMap().value("size").toLongLong(), bytes.size());

    // Saved again under the same attachments: nothing lost by an edit.
    QVariantMap again = controller->item(id);
    QVERIFY(controller->saveItem(again));
    QCOMPARE(controller->item(id).value("attachments").toList().size(), 1);

    const QString target = folder->filePath(QStringLiteral("copy.pdf"));
    controller->saveAttachment(id, attachments.first().toMap().value("id").toString(), QUrl::fromLocalFile(target));
    QTRY_VERIFY(QFile::exists(target));
    QFile written(target);
    QVERIFY(written.open(QIODevice::ReadOnly));
    QCOMPARE(written.readAll(), bytes);
    QVERIFY(!controller->messageIsTrouble());

    // A folder that cannot be written to is said, never silent.
    controller->saveAttachment(id, attachments.first().toMap().value("id").toString(),
                               QUrl::fromLocalFile(folder->filePath(QStringLiteral("no/such/folder/a.pdf"))));
    QTRY_VERIFY(controller->messageIsTrouble());
}

void TestCustomizing::a_file_too_large_or_missing_is_refused_and_said()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    const QString large = folder->filePath(QStringLiteral("large.bin"));
    {
        QFile file(large);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(largestAttachmentBytes + 1));
    }
    QSignalSpy read(controller.get(), &VaultController::attachmentRead);
    controller->readAttachment(QUrl::fromLocalFile(large));
    QTRY_COMPARE(read.size(), 1);
    QVERIFY(!read.first().at(3).toBool());
    QVERIFY(controller->messageIsTrouble());
    QVERIFY(controller->message().contains(QStringLiteral("large.bin")));

    controller->readAttachment(QUrl::fromLocalFile(folder->filePath(QStringLiteral("gone.txt"))));
    QTRY_COMPARE(read.size(), 2);
    QVERIFY(!read.at(1).at(3).toBool());
    QVERIFY(controller->message().contains(QStringLiteral("gone.txt")));
}

void TestCustomizing::an_attachment_not_saved_with_its_item_is_dropped_at_the_lock()
{
    auto controller = unlockedController();
    QVERIFY(controller);
    const QString source = folder->filePath(QStringLiteral("note.txt"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("words");
    }
    QSignalSpy read(controller.get(), &VaultController::attachmentRead);
    const QString requestId = controller->readAttachment(QUrl::fromLocalFile(source));
    QTRY_COMPARE(read.size(), 1);
    lockAndUnlock(*controller);
    QVariantMap item = controller->newLogin();
    item.insert(QStringLiteral("attachments"), QVariantList{QVariantMap{{"id", requestId}}});
    const QString id = saveItem(*controller, item, QStringLiteral("Late"));
    QVERIFY(controller->item(id).value("attachments").toList().isEmpty());
}

void TestCustomizing::a_save_with_attachments_interrupted_at_any_step_leaves_the_old_vault_whole()
{
    const VaultKey key = makeVaultKey("master", quick);
    QVERIFY(key.isValid());
    VaultContents old;
    Item item;
    item.id = QUuid::createUuid();
    item.attachments << Attachment{QUuid::createUuid(), QStringLiteral("old.bin"), QByteArray(1024 * 1024, 'o')};
    old.items << item;
    QCOMPARE(saveVaultFile(vaultPath, contentsToJson(old), key), SaveStatus::Saved);

    VaultContents changed = old;
    changed.items.first().attachments << Attachment{QUuid::createUuid(), QStringLiteral("new.bin"),
                                                    QByteArray(largestAttachmentBytes, 'n')};
    for (const SaveStep step : {SaveStep::WritingHalf, SaveStep::Written, SaveStep::Flushed, SaveStep::Verified}) {
        QCOMPARE(saveVaultFile(vaultPath, contentsToJson(changed), key,
                               [step](SaveStep reached) { return reached != step; }),
                 SaveStatus::Interrupted);
        const OpenResult opened = openVaultFile(vaultPath, "master");
        QCOMPARE(opened.status, OpenStatus::Opened);
        const ContentsResult read = contentsFromJson(opened.contents);
        QCOMPARE(read.status, ContentsStatus::Read);
        QCOMPARE(read.contents, old);
    }
}

QTEST_MAIN(TestCustomizing)
#include "test_customizing.moc"
