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
// test_vault_merge.cpp 0.2.56

// --- Version history ----------------------------------------------------
// v0.2.56: fieldOf returns on every path (Microsoft's compiler); the changes to built-in categories merged (D-056)
// v0.2.45: Initial release (D-054)

using namespace chrono_vault;

namespace {

const QDateTime t0 = QDateTime(QDate(2026, 11, 1), QTime(10, 0), QTimeZone::UTC);
const QDateTime now = t0.addDays(10);

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
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
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
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
        QCOMPARE(merged.contents.categories.size(), 1);
        QCOMPARE(merged.contents.categories[0].icon, QByteArrayLiteral("png"));
        QCOMPARE(merged.contents.builtInCategoryChanges.size(), 1);
        QCOMPARE(merged.contents.builtInCategoryChanges[0].name, QStringLiteral("Websites"));

        // Reset on the other side, unchanged here: it stays reset.
        VaultContents changedBase = merged.contents;
        VaultContents reset = changedBase;
        reset.builtInCategoryChanges.clear();
        const MergeResult afterReset = mergeContents(&changedBase, changedBase, reset, now);
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
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
        QCOMPARE(fieldOf(merged.contents.items[0], 1).value, QStringLiteral("robert"));
        QCOMPARE(fieldOf(merged.contents.items[0], 2).value, QStringLiteral("new"));
        QVERIFY(merged.contents.items[0].mergeNotes.isEmpty());
        QCOMPARE(merged.contents.items[0].modified, t0.addDays(2));
    }

    void the_same_field_changed_on_both_keeps_the_newest_and_notes_the_other()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs = base;
        setValue(mine.items[0], 2, QStringLiteral("mine"), t0.addDays(2));
        setValue(theirs.items[0], 2, QStringLiteral("theirs"), t0.addDays(1));
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
        const Item& item = merged.contents.items[0];
        QCOMPARE(fieldOf(item, 2).value, QStringLiteral("mine"));
        QCOMPARE(item.mergeNotes.size(), 1);
        QCOMPARE(item.mergeNotes[0].kind, MergeNote::Kind::ChangedOnBoth);
        QCOMPARE(item.mergeNotes[0].fieldUid, 2);
        QCOMPARE(item.mergeNotes[0].otherValue, QStringLiteral("theirs"));
        // Nothing lost: the other value and the old one are in the history.
        QVERIFY(fieldOf(item, 2).history.contains(FieldChange{QStringLiteral("theirs"), t0.addDays(1)}));
        QVERIFY(fieldOf(item, 2).history.contains(FieldChange{QStringLiteral("old"), t0}));
        QCOMPARE(merged.itemsChangedOnBoth, 1);
    }

    void without_a_base_a_difference_is_settled_by_the_newest_with_a_note()
    {
        VaultContents mine;
        mine.items = {login(QStringLiteral("Bank"))};
        VaultContents theirs = mine;
        setValue(theirs.items[0], 2, QStringLiteral("theirs"), t0.addDays(3));
        const MergeResult merged = mergeContents(nullptr, mine, theirs, now);
        QCOMPARE(fieldOf(merged.contents.items[0], 2).value, QStringLiteral("theirs"));
        QCOMPARE(merged.contents.items[0].mergeNotes.size(), 1);
        QCOMPARE(merged.contents.items[0].mergeNotes[0].otherValue, QStringLiteral("old"));
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
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
        QCOMPARE(merged.contents.items[0].title, QStringLiteral("The bank"));
        QCOMPARE(merged.contents.items[0].mergeNotes.size(), 1);
        QCOMPARE(merged.contents.items[0].mergeNotes[0].property, QStringLiteral("title"));
        QCOMPARE(merged.contents.items[0].mergeNotes[0].otherValue, QStringLiteral("My bank"));
    }

    void new_items_of_both_sides_are_kept()
    {
        VaultContents base;
        VaultContents mine;
        VaultContents theirs;
        mine.items = {login(QStringLiteral("Mine"))};
        theirs.items = {login(QStringLiteral("Theirs"))};
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
        QCOMPARE(merged.contents.items.size(), 2);
    }

    void an_item_deleted_there_and_unchanged_here_is_deleted()
    {
        VaultContents base;
        base.items = {login(QStringLiteral("Bank"))};
        VaultContents mine = base;
        VaultContents theirs;
        theirs.deletedItems = {DeletedItem{base.items[0].id, t0.addDays(1)}};
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
        QVERIFY(merged.contents.items.isEmpty());
        QCOMPARE(merged.contents.deletedItems.size(), 1);
        // The same without a base: the deletion is newer than the item.
        const MergeResult unbased = mergeContents(nullptr, mine, theirs, now);
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
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
        QCOMPARE(merged.contents.items.size(), 1);
        QCOMPARE(merged.contents.items[0].mergeNotes.size(), 1);
        QCOMPARE(merged.contents.items[0].mergeNotes[0].kind, MergeNote::Kind::DeletedElsewhere);
        QVERIFY(merged.contents.deletedItems.isEmpty());
        QCOMPARE(merged.itemsBack, 1);
        // The other way round gives the same.
        const MergeResult reversed = mergeContents(&base, theirs, mine, now);
        QCOMPARE(reversed.contents.items.size(), 1);
        QCOMPARE(reversed.itemsBack, 1);
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
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
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
        const MergeResult merged = mergeContents(&base, mine, theirs, now);
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
        contents.items[0].mergeNotes = {MergeNote{MergeNote::Kind::ChangedOnBoth, 2, QString(), QStringLiteral("x"), now},
                                        MergeNote{MergeNote::Kind::DeletedElsewhere, 0, QString(), QString(), now}};
        contents.deletedItems = {DeletedItem{QUuid::createUuid(), now}};
        const ContentsResult read = contentsFromJson(contentsToJson(contents));
        QCOMPARE(read.status, ContentsStatus::Read);
        QCOMPARE(read.contents.items[0].mergeNotes, contents.items[0].mergeNotes);
        QCOMPARE(read.contents.deletedItems, contents.deletedItems);
    }
};

QTEST_GUILESS_MAIN(TestVaultMerge)
#include "test_vault_merge.moc"
