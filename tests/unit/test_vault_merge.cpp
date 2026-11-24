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
#include "vault_merge.h"

#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vault_merge.cpp 0.5.81

// --- Version history ----------------------------------------------------
// v0.5.81: Which side deleted an item; an item brought back (D-061)
// v0.5.78: This device's value stands; every two-sided difference noted, the person's own categories and templates too (D-061)
// v0.2.56: fieldOf returns on every path (Microsoft's compiler); the changes to built-in categories merged (D-056)
// v0.2.45: Initial release (D-054)

using namespace chrono_vault;

namespace {

const QDateTime t0 = QDateTime(QDate(2026, 11, 1), QTime(10, 0), QTimeZone::UTC);
const QDateTime now = t0.addDays(10);
const QString here = QStringLiteral("this-device");

Item login(const QString& title)
{
    Item item;
    item.id = QUuid::createUuid();
    item.title = title;
    item.created = t0;
    item.modified = t0;
    Field user = makeField(QStringLiteral("username"), QString(), FieldKind::Username, QStringLiteral("rob"));
    user.uid = 1;
    user.changed = t0;
    Field password = makeField(QStringLiteral("password"), QString(), FieldKind::Password, QStringLiteral("old"));
    password.uid = 2;
    password.changed = t0;
    item.fields = {user, password};
    return item;
}

// A field's value set at a moment, as the editor sets it.
void setValue(Item& item, qint64 uid, const QString& value, const QDateTime& when)
{
    for (Field& field : item.fields) {
        if (field.uid == uid) {
            field.history.append(FieldChange{field.value, field.changed});
            field.value = value;
            field.changed = when;
        }
    }
    item.modified = when;
}

const Field& fieldOf(const Item& item, qint64 uid)
{
    for (const Field& field : item.fields) {
        if (field.uid == uid) {
            return field;
        }
    }
    qFatal("no such field");
    static const Field none; // never reached: Microsoft's compiler does not know qFatal ends here
    return none;
}

} // namespace

class TestVaultMerge : public QObject
{
    Q_OBJECT

private slots:
    void a_change_on_one_side_is_taken()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        setValue(theirs.items[0], 2, QStringLiteral("new"), t0.addDays(1));
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(fieldOf(merged.contents.items[0], 2).value, QStringLiteral("new"));
        QVERIFY(merged.contents.items[0].mergeNotes.isEmpty());
        QCOMPARE(merged.itemsChangedOnBoth, 0);
    }

    // A category's name and icon, built-in or the person's own, come along
    // from the other side like any other change (D-056).
    void a_category_changed_on_one_side_is_taken()
    {
        VaultContents base;
        base.categories = {Category{QStringLiteral("own"), QStringLiteral("Cars"), {}}};
        VaultContents mine = base;
        VaultContents theirs = base;
        theirs.categories[0].icon = QByteArrayLiteral("png");
        theirs.builtInCategoryChanges = {Category{QStringLiteral("login"), QStringLiteral("Websites"), {}}};
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(merged.contents.categories.size(), 1);
        QCOMPARE(merged.contents.categories[0].icon, QByteArrayLiteral("png"));
        QCOMPARE(merged.contents.builtInCategoryChanges.size(), 1);
        QCOMPARE(merged.contents.builtInCategoryChanges[0].name, QStringLiteral("Websites"));

        // Reset on the other side, unchanged here: it stays reset.
        VaultContents changedBase = merged.contents;
        VaultContents reset = changedBase;
        reset.builtInCategoryChanges.clear();
        const MergeResult afterReset = mergeContents(&changedBase, changedBase, reset, now, here);
        QVERIFY(afterReset.contents.builtInCategoryChanges.isEmpty());
    }

    void changes_to_different_fields_are_both_kept()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        setValue(mine.items[0], 1, QStringLiteral("robert"), t0.addDays(1));
        setValue(theirs.items[0], 2, QStringLiteral("new"), t0.addDays(2));
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(fieldOf(merged.contents.items[0], 1).value, QStringLiteral("robert"));
        QCOMPARE(fieldOf(merged.contents.items[0], 2).value, QStringLiteral("new"));
        QVERIFY(merged.contents.items[0].mergeNotes.isEmpty());
        QCOMPARE(merged.contents.items[0].modified, t0.addDays(2));
    }

    // The program never chooses (D-061): this device's value stands, even
    // when the other is newer, and the other waits for the person.
    void the_same_field_changed_on_both_keeps_this_devices_value_and_notes_the_other()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        setValue(mine.items[0], 2, QStringLiteral("mine"), t0.addDays(1));
        setValue(theirs.items[0], 2, QStringLiteral("theirs"), t0.addDays(2));
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        const Item& item = merged.contents.items[0];
        QCOMPARE(fieldOf(item, 2).value, QStringLiteral("mine"));
        QCOMPARE(item.mergeNotes.size(), 1);
        QCOMPARE(item.mergeNotes[0].kind, MergeNote::Kind::ChangedOnBoth);
        QCOMPARE(item.mergeNotes[0].fieldUid, 2);
        QCOMPARE(item.mergeNotes[0].otherValue, QStringLiteral("theirs"));
        QCOMPARE(item.mergeNotes[0].device, here);
        // Nothing lost: the other value and the old one are in the history.
        QVERIFY(fieldOf(item, 2).history.contains(FieldChange{QStringLiteral("theirs"), t0.addDays(2)}));
        QVERIFY(fieldOf(item, 2).history.contains(FieldChange{QStringLiteral("old"), t0}));
        QCOMPARE(merged.itemsChangedOnBoth, 1);
    }

    // With nothing to judge by, every difference waits for the person.
    void without_a_base_every_difference_waits_with_this_devices_value_standing()
    {
        VaultContents mine;
        mine.items = {login(QStringLiteral("Bank"))};
        VaultContents theirs = mine;
        setValue(theirs.items[0], 2, QStringLiteral("theirs"), t0.addDays(3));
        theirs.items[0].tags = {QStringLiteral("Work")};
        theirs.items[0].trashed = true;
        const MergeResult merged = mergeContents(nullptr, mine, theirs, now, here);
        const Item& item = merged.contents.items[0];
        QCOMPARE(fieldOf(item, 2).value, QStringLiteral("old"));
        QVERIFY(item.tags.isEmpty());
        QVERIFY(!item.trashed);
        QStringList properties;
        for (const MergeNote& note : item.mergeNotes) {
            properties.append(note.property);
        }
        // The other side also holds the earlier value "old" in its history.
        QCOMPARE(properties, QStringList({QStringLiteral("trashed"), QStringLiteral("tag"), QString(),
                                          QStringLiteral("history")}));
        QCOMPARE(merged.itemsChangedOnBoth, 1);
    }

    void the_title_changed_on_both_leaves_a_note()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        mine.items[0].title = QStringLiteral("My bank");
        mine.items[0].modified = t0.addDays(1);
        theirs.items[0].title = QStringLiteral("The bank");
        theirs.items[0].modified = t0.addDays(2);
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(merged.contents.items[0].title, QStringLiteral("My bank"));
        QCOMPARE(merged.contents.items[0].mergeNotes.size(), 1);
        QCOMPARE(merged.contents.items[0].mergeNotes[0].property, QStringLiteral("title"));
        QCOMPARE(merged.contents.items[0].mergeNotes[0].otherValue, QStringLiteral("The bank"));
    }

    void new_items_of_both_sides_are_kept()
    {
        VaultContents base;
        VaultContents mine;
        VaultContents theirs;
        mine.items = {login(QStringLiteral("Mine"))};
        theirs.items = {login(QStringLiteral("Theirs"))};
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(merged.contents.items.size(), 2);
    }

    void an_item_deleted_there_and_unchanged_here_is_deleted()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs;
        theirs.deletedItems = {DeletedItem{base.items[0].id, t0.addDays(1)}};
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QVERIFY(merged.contents.items.isEmpty());
        QCOMPARE(merged.contents.deletedItems.size(), 1);
        // The same without a base: the deletion is newer than the item.
        const MergeResult unbased = mergeContents(nullptr, mine, theirs, now, here);
        QVERIFY(unbased.contents.items.isEmpty());
    }

    void an_item_deleted_there_but_changed_here_comes_back_with_a_note()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        setValue(mine.items[0], 2, QStringLiteral("new"), t0.addDays(2));
        VaultContents theirs;
        theirs.deletedItems = {DeletedItem{base.items[0].id, t0.addDays(1)}};
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(merged.contents.items.size(), 1);
        QCOMPARE(merged.contents.items[0].mergeNotes.size(), 1);
        QCOMPARE(merged.contents.items[0].mergeNotes[0].kind, MergeNote::Kind::DeletedElsewhere);
        QVERIFY(merged.contents.deletedItems.isEmpty());
        QCOMPARE(merged.itemsBack, 1);
        // The other way round gives the same.
        const MergeResult reversed = mergeContents(&base, theirs, mine, now, here);
        QCOMPARE(reversed.contents.items.size(), 1);
        QCOMPARE(reversed.itemsBack, 1);
        // Which side deleted it is said (D-061).
        QCOMPARE(merged.contents.items[0].mergeNotes[0].device, here);
        QVERIFY(merged.contents.items[0].mergeNotes[0].property.isEmpty());
        QCOMPARE(reversed.contents.items[0].mergeNotes[0].property, QStringLiteral("deletedOn"));
    }

    // Keep here only, or an import, brought back what both sides had agreed
    // was deleted: the other device takes it without asking (D-061).
    void an_item_brought_back_on_one_side_is_taken()
    {
        VaultContents base;
        const Item item = login(QStringLiteral("Bank"));
        base.deletedItems = {DeletedItem{item.id, t0}};
        VaultContents mine = base;
        mine.items = {login(QStringLiteral("Other"))}; // changed here meanwhile
        VaultContents theirs;
        theirs.items = {item};
        theirs.items[0].modified = t0.addDays(1);
        for (const bool reversed : {false, true}) {
            const MergeResult merged = reversed ? mergeContents(&base, theirs, mine, now, here)
                                                : mergeContents(&base, mine, theirs, now, here);
            QCOMPARE(merged.contents.items.size(), 2);
            QCOMPARE(merged.itemsBack, 0);
            QVERIFY(merged.contents.deletedItems.isEmpty());
            for (const Item& one : merged.contents.items) {
                QVERIFY(one.mergeNotes.isEmpty());
            }
        }
    }

    void tags_added_and_removed_on_both_sides_are_combined()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        base.items[0].tags = {QStringLiteral("a"), QStringLiteral("b")};
        VaultContents mine = base;
        VaultContents theirs = base;
        mine.items[0].tags = {QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
        mine.items[0].modified = t0.addDays(1);
        theirs.items[0].tags = {QStringLiteral("b")};
        theirs.items[0].modified = t0.addDays(1);
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(merged.contents.items[0].tags, QStringList({QStringLiteral("b"), QStringLiteral("c")}));
    }

    void a_field_added_on_each_side_keeps_both_in_place()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        Field pin = makeField(QString(), QStringLiteral("PIN"), FieldKind::Pin, QStringLiteral("1234"));
        pin.uid = 3;
        mine.items[0].fields.append(pin);
        mine.items[0].modified = t0.addDays(1);
        Field url = makeField(QStringLiteral("url"), QString(), FieldKind::WebAddress, QStringLiteral("bank.example"));
        url.uid = 4;
        theirs.items[0].fields.insert(1, url);
        theirs.items[0].modified = t0.addDays(1);
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QList<qint64> uids;
        for (const Field& field : merged.contents.items[0].fields) {
            uids.append(field.uid);
        }
        QCOMPARE(uids, QList<qint64>({1, 4, 2, 3}));
    }

    void notes_and_deletions_survive_the_vault_file()
    {
        VaultContents contents;
        contents.items = {login(QStringLiteral("Bank"))};
        MergeNote changed;
        changed.fieldUid = 2;
        changed.otherValue = QStringLiteral("x");
        changed.when = now;
        changed.device = here;
        MergeNote deleted;
        deleted.kind = MergeNote::Kind::DeletedElsewhere;
        deleted.when = now;
        MergeNote attachment;
        attachment.property = QStringLiteral("attachment");
        attachment.subject = QUuid::createUuid().toString(QUuid::WithoutBraces);
        attachment.otherValue = QStringLiteral("scan.pdf");
        attachment.otherData = QByteArrayLiteral("%PDF");
        attachment.when = now;
        attachment.imported = true;
        MergeNote history;
        history.fieldUid = 2;
        history.property = QStringLiteral("history");
        history.otherValue = QStringLiteral("older");
        history.otherWhen = t0.addDays(-3);
        history.when = now;
        contents.items[0].mergeNotes = {changed, deleted, attachment, history};
        MergeNote category;
        category.property = QStringLiteral("category");
        category.subject = QStringLiteral("own");
        category.otherValue = QStringLiteral("Cars");
        category.when = now;
        contents.vaultNotes = {category};
        contents.deletedItems = {DeletedItem{QUuid::createUuid(), now}};
        const ContentsResult read = contentsFromJson(contentsToJson(contents));
        QCOMPARE(read.status, ContentsStatus::Read);
        QCOMPARE(read.contents.items[0].mergeNotes, contents.items[0].mergeNotes);
        QCOMPARE(read.contents.vaultNotes, contents.vaultNotes);
        QCOMPARE(read.contents.deletedItems, contents.deletedItems);
    }

    // D-061: whatever both devices changed differently waits for the person,
    // this device's standing meanwhile.
    void the_category_trash_archive_favourite_and_autosubmit_changed_on_both_are_noted()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        mine.items[0].categoryId = QStringLiteral("finance");
        theirs.items[0].categoryId = QStringLiteral("computer");
        mine.items[0].trashed = true;
        theirs.items[0].trashed = true; // the same on both: no conflict
        mine.items[0].modified = t0.addDays(1);
        theirs.items[0].modified = t0.addDays(2);
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        const Item& item = merged.contents.items[0];
        QCOMPARE(item.categoryId, QStringLiteral("finance"));
        QVERIFY(item.trashed);
        QCOMPARE(item.mergeNotes.size(), 1);
        QCOMPARE(item.mergeNotes[0].property, QStringLiteral("category"));
        QCOMPARE(item.mergeNotes[0].otherValue, QStringLiteral("computer"));

        // Moved to the Archive on one side only: taken, no conflict.
        VaultContents archived = base;
        archived.items[0].archived = true;
        archived.items[0].modified = t0.addDays(1);
        const MergeResult oneSided = mergeContents(&base, base, archived, now, here);
        QVERIFY(oneSided.contents.items[0].archived);
        QVERIFY(oneSided.contents.items[0].mergeNotes.isEmpty());
    }

    void a_field_deleted_on_one_side_and_changed_on_the_other_waits_for_the_person()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        mine.items[0].fields[1].deleted = true;
        mine.items[0].modified = t0.addDays(1);
        setValue(theirs.items[0], 2, QStringLiteral("changed"), t0.addDays(2));
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        const Item& item = merged.contents.items[0];
        QVERIFY(fieldOf(item, 2).deleted);
        QCOMPARE(item.mergeNotes.size(), 1);
        QCOMPARE(item.mergeNotes[0].property, QStringLiteral("present"));
        QCOMPARE(item.mergeNotes[0].otherValue, QStringLiteral("changed"));
        // The other way round: this device changed it, the other deleted it.
        const MergeResult reversed = mergeContents(&base, theirs, mine, now, here);
        QVERIFY(!fieldOf(reversed.contents.items[0], 2).deleted);
        QCOMPARE(fieldOf(reversed.contents.items[0], 2).value, QStringLiteral("changed"));
        QCOMPARE(reversed.contents.items[0].mergeNotes[0].property, QStringLiteral("absent"));
        // Deleted on one side, unchanged on the other: deleted, no conflict.
        const MergeResult deletion = mergeContents(&base, base, mine, now, here);
        QVERIFY(fieldOf(deletion.contents.items[0], 2).deleted);
        QVERIFY(deletion.contents.items[0].mergeNotes.isEmpty());
    }

    void a_label_and_a_hide_mark_changed_on_both_are_noted()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        Field account = makeField(QString(), QStringLiteral("Account"), FieldKind::Text, QStringLiteral("NL00"));
        account.uid = 3;
        base.items[0].fields.append(account);
        VaultContents mine = base;
        VaultContents theirs = base;
        mine.items[0].fields[2].label = QStringLiteral("Account number");
        mine.items[0].fields[2].sensitive = true;
        mine.items[0].modified = t0.addDays(1);
        theirs.items[0].fields[2].label = QStringLiteral("IBAN");
        theirs.items[0].fields[2].sensitive = false;
        theirs.items[0].fields[2].value = QStringLiteral("NL00 changed");
        theirs.items[0].modified = t0.addDays(2);
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        const Item& item = merged.contents.items[0];
        QCOMPARE(fieldOf(item, 3).label, QStringLiteral("Account number"));
        QVERIFY(fieldOf(item, 3).sensitive);
        QCOMPARE(fieldOf(item, 3).value, QStringLiteral("NL00 changed")); // changed on one side only
        QCOMPARE(item.mergeNotes.size(), 1);
        QCOMPARE(item.mergeNotes[0].property, QStringLiteral("label"));
        QCOMPARE(item.mergeNotes[0].otherValue, QStringLiteral("IBAN"));
    }

    void attachments_changed_on_both_or_changed_and_removed_wait_for_the_person()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        const Attachment scan{QUuid::createUuid(), QStringLiteral("scan.pdf"), QByteArrayLiteral("one")};
        base.items[0].attachments = {scan};
        VaultContents mine = base;
        VaultContents theirs = base;
        mine.items[0].attachments[0].data = QByteArrayLiteral("mine");
        mine.items[0].modified = t0.addDays(1);
        theirs.items[0].attachments[0].data = QByteArrayLiteral("theirs");
        theirs.items[0].modified = t0.addDays(2);
        const MergeResult both = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(both.contents.items[0].attachments[0].data, QByteArrayLiteral("mine"));
        QCOMPARE(both.contents.items[0].mergeNotes[0].property, QStringLiteral("attachment"));
        QCOMPARE(both.contents.items[0].mergeNotes[0].otherData, QByteArrayLiteral("theirs"));

        VaultContents removed = base;
        removed.items[0].attachments.clear();
        removed.items[0].modified = t0.addDays(2);
        const MergeResult keptHere = mergeContents(&base, mine, removed, now, here);
        QCOMPARE(keptHere.contents.items[0].attachments.size(), 1);
        QCOMPARE(keptHere.contents.items[0].mergeNotes[0].property, QStringLiteral("attachmentAbsent"));
        const MergeResult removedHere = mergeContents(&base, removed, theirs, now, here);
        QVERIFY(removedHere.contents.items[0].attachments.isEmpty());
        QCOMPARE(removedHere.contents.items[0].mergeNotes[0].property, QStringLiteral("attachment"));
        QCOMPARE(removedHere.contents.items[0].mergeNotes[0].otherData, QByteArrayLiteral("theirs"));
    }

    void own_categories_and_templates_changed_on_both_wait_in_the_vaults_notes()
    {
        VaultContents base;
        base.categories = {Category{QStringLiteral("own"), QStringLiteral("Cars"), {}}};
        base.templates = {Template{QStringLiteral("car"), QStringLiteral("own"), QStringLiteral("Car"), {}, false}};
        VaultContents mine = base;
        VaultContents theirs = base;
        mine.categories[0].name = QStringLiteral("My cars");
        theirs.categories[0].name = QStringLiteral("Vehicles");
        mine.templates[0].name = QStringLiteral("My car");
        theirs.templates[0].name = QStringLiteral("Vehicle");
        const MergeResult merged = mergeContents(&base, mine, theirs, now, here);
        QCOMPARE(merged.contents.categories[0].name, QStringLiteral("My cars"));
        QCOMPARE(merged.contents.templates[0].name, QStringLiteral("My car"));
        QCOMPARE(merged.contents.vaultNotes.size(), 2);
        QCOMPARE(merged.contents.vaultNotes[0].property, QStringLiteral("category"));
        QCOMPARE(merged.contents.vaultNotes[0].otherValue, QStringLiteral("Vehicles"));
        QCOMPARE(merged.contents.vaultNotes[1].property, QStringLiteral("template"));
        QCOMPARE(templateFromJson(merged.contents.vaultNotes[1].otherData)->name, QStringLiteral("Vehicle"));
    }
};

QTEST_GUILESS_MAIN(TestVaultMerge)
#include "test_vault_merge.moc"
