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

#include "item_differences.h"

#include <QtTest>

// --- Version ------------------------------------------------------------
// test_item_differences.cpp 0.5.78

// --- Version history ----------------------------------------------------
// v0.5.78: Initial release (D-061)

// Every way an imported item can differ from the vault's becomes a note for
// the person, the vault's value standing; each note settled either way does
// exactly what it says (D-061).

using namespace chrono_vault;

namespace {

const QDateTime t0 = QDateTime(QDate(2026, 11, 1), QTime(10, 0), QTimeZone::UTC);
const QDateTime now = t0.addDays(10);

Item bank()
{
    Item item;
    item.id = QUuid::createUuid();
    item.title = QStringLiteral("Bank");
    item.created = t0;
    item.modified = t0;
    Field user = makeField(QStringLiteral("username"), QString(), FieldKind::Username, QStringLiteral("rob"));
    user.uid = 1;
    Field password = makeField(QStringLiteral("password"), QString(), FieldKind::Password, QStringLiteral("old"));
    password.uid = 2;
    Field pin = makeField(QString(), QStringLiteral("PIN"), FieldKind::Pin, QStringLiteral("1234"));
    pin.uid = 3;
    item.fields = {user, password, pin};
    return item;
}

Field& fieldOf(Item& item, qint64 uid)
{
    for (Field& field : item.fields) {
        if (field.uid == uid) {
            return field;
        }
    }
    qFatal("no such field");
    static Field none; // never reached
    return none;
}

// The one note an import leaves when theirs differs from ours in one way.
MergeNote onlyNote(Item& ours, const Item& theirs)
{
    const NotedDifferences noted = noteDifferences(ours, theirs, now, true, QString());
    if (!noted.newConflict || ours.mergeNotes.size() != 1) {
        qFatal("expected exactly one new note");
    }
    return ours.mergeNotes.first();
}

} // namespace

class TestItemDifferences : public QObject
{
    Q_OBJECT

private slots:
    void the_same_item_is_no_conflict()
    {
        Item ours = bank();
        const Item theirs = ours;
        const NotedDifferences noted = noteDifferences(ours, theirs, now, true, QString());
        QVERIFY(!noted.changed);
        QVERIFY(ours.mergeNotes.isEmpty());
    }

    void every_item_property_that_differs_is_a_note_and_settles_both_ways_data()
    {
        QTest::addColumn<QString>("property");
        QTest::newRow("title") << QStringLiteral("title");
        QTest::newRow("subtitle") << QStringLiteral("subtitle");
        QTest::newRow("notes") << QStringLiteral("note");
        QTest::newRow("website") << QStringLiteral("website");
        QTest::newRow("category") << QStringLiteral("category");
        QTest::newRow("template") << QStringLiteral("template");
        QTest::newRow("trash") << QStringLiteral("trashed");
        QTest::newRow("archive") << QStringLiteral("archived");
        QTest::newRow("favourite") << QStringLiteral("favourite");
        QTest::newRow("autosubmit") << QStringLiteral("autoSubmit");
        QTest::newRow("icon") << QStringLiteral("icon");
    }
    void every_item_property_that_differs_is_a_note_and_settles_both_ways()
    {
        QFETCH(QString, property);
        Item ours = bank();
        Item theirs = ours;
        if (property == QLatin1String("title")) theirs.title = QStringLiteral("Other");
        if (property == QLatin1String("subtitle")) theirs.subtitle = QStringLiteral("Other");
        if (property == QLatin1String("note")) theirs.note = QStringLiteral("Other");
        if (property == QLatin1String("website")) theirs.website = QStringLiteral("other.example");
        if (property == QLatin1String("category")) theirs.categoryId = QStringLiteral("computer");
        if (property == QLatin1String("template")) theirs.templateId = QStringLiteral("computer");
        if (property == QLatin1String("trashed")) theirs.trashed = true;
        if (property == QLatin1String("archived")) theirs.archived = true;
        if (property == QLatin1String("favourite")) theirs.favourite = true;
        if (property == QLatin1String("autoSubmit")) theirs.autoSubmit = true;
        if (property == QLatin1String("icon")) theirs.icon = QByteArrayLiteral("\x89PNG");
        const Item before = ours;
        const MergeNote note = onlyNote(ours, theirs);
        QCOMPARE(note.property, property);
        QVERIFY(note.imported);
        // Nothing changed until the person chooses.
        Item kept = ours;
        kept.mergeNotes.clear();
        QCOMPARE(kept, before);
        // The other side taken: the item is as the import has it.
        useOtherSide(kept, note, now);
        kept.created = theirs.created;
        kept.modified = theirs.modified;
        QCOMPARE(kept, theirs);
    }

    void a_tag_only_the_import_holds_is_a_note()
    {
        Item ours = bank();
        Item theirs = ours;
        theirs.tags = {QStringLiteral("Work")};
        const MergeNote note = onlyNote(ours, theirs);
        QCOMPARE(note.property, QStringLiteral("tag"));
        QVERIFY(ours.tags.isEmpty());
        useOtherSide(ours, note, now);
        QCOMPARE(ours.tags, QStringList{QStringLiteral("Work")});
    }

    void an_attachment_only_the_import_holds_or_with_other_content_is_a_note()
    {
        Item ours = bank();
        Item theirs = ours;
        const Attachment scan{QUuid::createUuid(), QStringLiteral("scan.pdf"), QByteArrayLiteral("new")};
        theirs.attachments = {scan};
        const MergeNote added = onlyNote(ours, theirs);
        QCOMPARE(added.property, QStringLiteral("attachment"));
        QVERIFY(ours.attachments.isEmpty());
        useOtherSide(ours, added, now);
        QCOMPARE(ours.attachments, QList<Attachment>{scan});

        // The same attachment with other content: taking the import's keeps
        // the vault's beside it as an earlier version, so nothing is lost.
        Item mine = bank();
        mine.attachments = {Attachment{scan.id, scan.name, QByteArrayLiteral("mine")}};
        const MergeNote other = onlyNote(mine, theirs);
        useOtherSide(mine, other, now);
        QCOMPARE(mine.attachments.size(), 2);
        QCOMPARE(mine.attachments[0], scan);
        QCOMPARE(mine.attachments[1].name, QStringLiteral("scan (earlier).pdf"));
        QCOMPARE(mine.attachments[1].data, QByteArrayLiteral("mine"));
    }

    void field_differences_are_notes_and_settle_both_ways()
    {
        // The value.
        Item ours = bank();
        Item theirs = ours;
        fieldOf(theirs, 2).value = QStringLiteral("new");
        MergeNote note = onlyNote(ours, theirs);
        QCOMPARE(note.property, QString());
        QCOMPARE(fieldOf(ours, 2).value, QStringLiteral("old"));
        useOtherSide(ours, note, now);
        QCOMPARE(fieldOf(ours, 2).value, QStringLiteral("new"));
        QCOMPARE(fieldOf(ours, 2).history.last().value, QStringLiteral("old"));

        // The name and the hide mark.
        ours = bank();
        theirs = ours;
        fieldOf(theirs, 3).label = QStringLiteral("Card PIN");
        note = onlyNote(ours, theirs);
        QCOMPARE(note.property, QStringLiteral("label"));
        useOtherSide(ours, note, now);
        QCOMPARE(fieldOf(ours, 3).label, QStringLiteral("Card PIN"));
        ours = bank();
        theirs = ours;
        fieldOf(theirs, 3).sensitive = false;
        note = onlyNote(ours, theirs);
        QCOMPARE(note.property, QStringLiteral("hidden"));
        useOtherSide(ours, note, now);
        QVERIFY(!fieldOf(ours, 3).sensitive);

        // An earlier value only the import holds.
        ours = bank();
        theirs = ours;
        fieldOf(theirs, 2).history = {FieldChange{QStringLiteral("older"), t0.addDays(-5)}};
        note = onlyNote(ours, theirs);
        QCOMPARE(note.property, QStringLiteral("history"));
        QVERIFY(fieldOf(ours, 2).history.isEmpty());
        useOtherSide(ours, note, now);
        QCOMPARE(fieldOf(ours, 2).history, fieldOf(theirs, 2).history);
    }

    // P54: a field the vault deleted, the import still holds, values the same
    // or not: the person chooses; taking the import's brings it back.
    void a_field_deleted_here_and_held_by_the_import_is_a_note()
    {
        for (const QString& value : {QStringLiteral("1234"), QStringLiteral("5678")}) {
            Item ours = bank();
            fieldOf(ours, 3).deleted = true;
            Item theirs = bank();
            theirs.id = ours.id;
            fieldOf(theirs, 3).value = value;
            const MergeNote note = onlyNote(ours, theirs);
            QCOMPARE(note.property, QStringLiteral("present"));
            QVERIFY(fieldOf(ours, 3).deleted);
            useOtherSide(ours, note, now);
            QVERIFY(!fieldOf(ours, 3).deleted);
            QCOMPARE(fieldOf(ours, 3).value, value);
        }
    }

    void a_field_the_import_deleted_is_a_note()
    {
        Item ours = bank();
        Item theirs = ours;
        fieldOf(theirs, 3).deleted = true;
        const MergeNote note = onlyNote(ours, theirs);
        QCOMPARE(note.property, QStringLiteral("absent"));
        QVERIFY(!fieldOf(ours, 3).deleted);
        useOtherSide(ours, note, now);
        QVERIFY(fieldOf(ours, 3).deleted);
    }

    // A field only the import has is kept on the item, deleted, until the
    // person chooses: nothing of it is lost meanwhile (OR-15).
    void a_field_only_the_import_has_is_a_note()
    {
        Item ours = bank();
        Item theirs = ours;
        Field extra = makeField(QString(), QStringLiteral("Code"), FieldKind::Text, QStringLiteral("1111"));
        extra.uid = 4;
        theirs.fields.append(extra);
        const MergeNote note = onlyNote(ours, theirs);
        QCOMPARE(note.property, QStringLiteral("present"));
        QVERIFY(fieldOf(ours, 4).deleted);
        useOtherSide(ours, note, now);
        QVERIFY(!fieldOf(ours, 4).deleted);
        QCOMPARE(fieldOf(ours, 4).value, QStringLiteral("1111"));
    }

    void the_same_difference_is_noted_once()
    {
        Item ours = bank();
        Item theirs = ours;
        theirs.title = QStringLiteral("Other");
        QVERIFY(noteDifferences(ours, theirs, now, true, QString()).newConflict);
        QVERIFY(!noteDifferences(ours, theirs, now.addDays(1), true, QString()).newConflict);
        QCOMPARE(ours.mergeNotes.size(), 1);
    }

    void a_vault_note_puts_the_other_category_or_template_in_place()
    {
        VaultContents contents;
        contents.categories = {Category{QStringLiteral("own"), QStringLiteral("Cars"), {}}};
        contents.templates = {Template{QStringLiteral("car"), QStringLiteral("own"), QStringLiteral("Car"), {}, false}};
        MergeNote category;
        category.property = QStringLiteral("category");
        category.subject = QStringLiteral("own");
        category.otherValue = QStringLiteral("Vehicles");
        useOtherSide(contents, category);
        QCOMPARE(contents.categories[0].name, QStringLiteral("Vehicles"));
        MergeNote one;
        one.property = QStringLiteral("template");
        one.subject = QStringLiteral("car");
        one.otherData = templateToJson(Template{QStringLiteral("car"), QStringLiteral("own"), QStringLiteral("Vehicle"), {}, false});
        useOtherSide(contents, one);
        QCOMPARE(contents.templates[0].name, QStringLiteral("Vehicle"));
    }
};

QTEST_GUILESS_MAIN(TestItemDifferences)
#include "test_item_differences.moc"
