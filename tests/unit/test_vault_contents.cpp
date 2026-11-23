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

#include "templates.h"
#include "vault_contents.h"

#include <QTimeZone>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vault_contents.cpp 0.2.56

// --- Version history ----------------------------------------------------
// v0.2.56: Format 8: a category's icon, the changes to built-in categories (D-056)
// v0.2.45: Format 7 (D-054)
// v0.1.41: Tests of the Notes box, the arrow keys, the cards, the switch and the field types (D-051)
// v0.0.40: Tests of the icon picker and the reminder's place (D-050)
// v0.0.17: Contents format 4: sections, field types, hide marks, hidden and deleted fields, history, ids; an item's
//          subtitle, note, trash, archive, autosubmit, website; format 3 still read (D-033)
// v0.0.14: The built-in templates; the contents format 3 with categories, templates, tags, favourites and
//          attachments; formats 1 and 2 still read (D-027, D-028, D-029)
// v0.0.4: Icons, and the contents format 2
// v0.0.2: Initial release

using namespace chrono_vault;

namespace {

Item itemFrom(const char* templateId)
{
    const auto itemTemplate = findTemplate(QString::fromLatin1(templateId), {});
    return itemTemplate ? newItem(*itemTemplate) : Item{};
}

QByteArray format3Item(const QByteArray& extra)
{
    return R"({"format":3,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","category":"login","title":"","created":"","modified":"","fields":[])"
           + extra + "}]}";
}

} // namespace

class TestVaultContents : public QObject
{
    Q_OBJECT

private slots:
    void a_new_login_has_the_login_fields();
    void a_new_secure_note_has_only_notes();
    void every_built_in_template_has_a_built_in_category_and_named_fields();
    void contents_come_back_exactly_as_they_were_written();
    void an_empty_vault_comes_back_empty();
    void a_newer_format_is_refused();
    void the_vaults_own_icons_are_kept();
    void the_reference_programs_other_field_types_are_stored_by_its_words();
    void the_old_formats_1_and_2_are_still_read();
    void a_tag_is_kept_trimmed_and_nested_by_slashes();
    void contents_of_the_wrong_shape_are_damaged_data();
    void contents_of_the_wrong_shape_are_damaged();
};

void TestVaultContents::a_new_login_has_the_login_fields()
{
    const Item item = itemFrom("login");
    QVERIFY(!item.id.isNull());
    QCOMPARE(item.categoryId, QStringLiteral("login"));
    QStringList roles;
    for (const Field& field : item.fields) {
        roles << field.role;
    }
    // No "Notes" field: the item has its own Notes (D-051).
    QCOMPARE(roles, QStringList({"username", "password", "url", "totp"}));
    QCOMPARE(item.fields.at(1).kind, FieldKind::Password);
    QCOMPARE(roleName(QStringLiteral("username")), QStringLiteral("User name"));
}

void TestVaultContents::a_new_secure_note_has_only_notes()
{
    const Item item = itemFrom("note");
    QCOMPARE(item.categoryId, QStringLiteral("note"));
    // No fields: its text is the item's own Notes (D-051).
    QVERIFY(item.fields.isEmpty());
}

void TestVaultContents::every_built_in_template_has_a_built_in_category_and_named_fields()
{
    QStringList ids;
    for (const Template& one : builtInTemplates()) {
        QVERIFY2(isBuiltInCategory(one.categoryId), qPrintable(one.id));
        QVERIFY2(!ids.contains(one.id), qPrintable(one.id));
        ids << one.id;
        QVERIFY(templateName(one) != one.id);
        for (const Field& field : one.fields) {
            QVERIFY2(roleName(field.role) != field.role, qPrintable(field.role));
            QVERIFY(field.value.isEmpty());
        }
    }
    QCOMPARE(builtInCategoryIds().size(), 8);
    QCOMPARE(builtInTemplates().size(), 13);
    // A category of the person's own shows the name they gave it.
    VaultContents contents;
    contents.categories << Category{QStringLiteral("own"), QStringLiteral("Cars"), {}};
    QCOMPARE(categoryName(QStringLiteral("own"), contents), QStringLiteral("Cars"));
    QCOMPARE(categoryName(QStringLiteral("card"), contents), QStringLiteral("Credit card"));
    QCOMPARE(categoryIconSource(QStringLiteral("own"), contents), QStringLiteral("qrc:/icons/item-misc.svg"));
}

void TestVaultContents::contents_come_back_exactly_as_they_were_written()
{
    VaultContents contents;
    Item login = itemFrom("login");
    login.title = QStringLiteral("Bank – €uro \"quoted\"");
    login.fields[0].value = QStringLiteral("anna@example.org");
    login.fields[1].value = QStringLiteral("p@ss\nword");
    login.fields.append(makeField({}, QStringLiteral("PIN"), FieldKind::Password, QStringLiteral("1234")));
    Field heading = makeField({}, QStringLiteral("Github 2FA Authentication"), FieldKind::Section);
    Field token = makeField({}, QStringLiteral("Token"), FieldKind::Text, QStringLiteral("secret"));
    token.sensitive = true;
    token.uid = 7396;
    token.changed = QDateTime::fromSecsSinceEpoch(1785260034, QTimeZone::UTC);
    token.history << FieldChange{QStringLiteral("older"), QDateTime::fromSecsSinceEpoch(1700000000, QTimeZone::UTC)};
    Field marker = makeField({}, QStringLiteral("2FA"), FieldKind::Hidden, QStringLiteral("x"));
    marker.hiddenType = QStringLiteral(".twoFA");
    Field gone = makeField({}, QStringLiteral("Old"), FieldKind::Date, QStringLiteral("08/2029"));
    gone.deleted = true;
    login.fields << heading << token << marker << gone;
    login.subtitle = QStringLiteral("own subtitle");
    login.note = QStringLiteral("a note\nof two lines");
    login.archived = true;
    login.autoSubmit = true;
    login.website = QStringLiteral("https://example.org");
    login.icon = QByteArray("\x89PNG not really, but bytes all the same", 40);
    login.tags = {QStringLiteral("Work/Servers"), QStringLiteral("Home")};
    login.favourite = true;
    login.attachments << Attachment{QUuid::createUuid(), QStringLiteral("scan.pdf"), QByteArray("%PDF\0bytes", 10)};
    Item trashed = itemFrom("note");
    trashed.trashed = true;
    Item card = itemFrom("card");
    card.templateId = QStringLiteral("8a2e");
    card.categoryId = QStringLiteral("own");
    contents.items << login << trashed << card;
    contents.categories << Category{QStringLiteral("own"), QStringLiteral("Cars"), {}};
    contents.templates << Template{QStringLiteral("8a2e"), QStringLiteral("own"), QStringLiteral("Car"),
                                   {makeField({}, QStringLiteral("Plate"), FieldKind::Text)}, false};
    contents.hiddenCategories << QStringLiteral("finance");
    // Format 8 (D-056): a category's icon, and a built-in one as the person changed it.
    contents.categories[0].icon = QByteArrayLiteral("\x89PNG own");
    contents.builtInCategoryChanges << Category{QStringLiteral("login"), QStringLiteral("Websites"), {}}
                                    << Category{QStringLiteral("card"), QString(), QByteArrayLiteral("\x89PNG card")};

    const ContentsResult result = contentsFromJson(contentsToJson(contents));
    QCOMPARE(result.status, ContentsStatus::Read);
    QCOMPARE(result.contents, contents);
}

void TestVaultContents::an_empty_vault_comes_back_empty()
{
    const ContentsResult result = contentsFromJson(contentsToJson(VaultContents{}));
    QCOMPARE(result.status, ContentsStatus::Read);
    QVERIFY(result.contents.items.isEmpty());
}

void TestVaultContents::a_newer_format_is_refused()
{
    QCOMPARE(contentsFromJson(R"({"format":9,"items":[]})").status, ContentsStatus::NewerFormat);
}

// The vault's own icons are kept and read back; a damaged list fails the
// contents alone, never the program (D-050, OR-4).
void TestVaultContents::the_vaults_own_icons_are_kept()
{
    VaultContents contents;
    contents.customIcons = {QByteArray("\x89PNG one"), QByteArray("\x89PNG two")};
    const ContentsResult result = contentsFromJson(contentsToJson(contents));
    QCOMPARE(result.status, ContentsStatus::Read);
    QCOMPARE(result.contents.customIcons, contents.customIcons);
    QCOMPARE(contentsFromJson(R"({"format":5,"items":[],"customIcons":[3]})").status, ContentsStatus::Damaged);
    QCOMPARE(contentsFromJson(R"({"format":5,"items":[],"customIcons":"x"})").status, ContentsStatus::Damaged);
    // A format 4 vault has no list: its items' icons become it, each once.
    VaultContents older;
    Item first;
    first.id = QUuid::createUuid();
    first.title = QStringLiteral("Bank");
    first.icon = QByteArray("\x89PNG one");
    Item second = first;
    second.id = QUuid::createUuid();
    older.items = {first, second};
    QByteArray json = contentsToJson(older);
    json.replace("\"format\":8", "\"format\":4");
    const ContentsResult read = contentsFromJson(json);
    QCOMPARE(read.status, ContentsStatus::Read);
    QCOMPARE(read.contents.customIcons, QList<QByteArray>{first.icon});
}

void TestVaultContents::the_old_formats_1_and_2_are_still_read()
{
    ContentsResult result = contentsFromJson(
        R"({"format":1,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"Old","created":"","modified":"","fields":[]}]})");
    QCOMPARE(result.status, ContentsStatus::Read);
    QCOMPARE(result.contents.items.size(), 1);
    QVERIFY(result.contents.items.constFirst().icon.isEmpty());
    QCOMPARE(result.contents.items.constFirst().categoryId, QStringLiteral("login"));

    result = contentsFromJson(
        R"({"format":2,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"note","title":"Old","created":"","modified":"","fields":[]}]})");
    QCOMPARE(result.status, ContentsStatus::Read);
    QCOMPARE(result.contents.items.constFirst().templateId, QStringLiteral("note"));
    QCOMPARE(result.contents.items.constFirst().categoryId, QStringLiteral("note"));
    QVERIFY(result.contents.items.constFirst().tags.isEmpty());
    QVERIFY(!result.contents.items.constFirst().favourite);

    // Format 3: its fields get ids, for the history from now on (D-033).
    result = contentsFromJson(format3Item("").replace(R"("fields":[])",
        R"("fields":[{"role":"username","kind":"text","value":"a"},{"label":"x","kind":"text","value":"b"}])"));
    QCOMPARE(result.status, ContentsStatus::Read);
    QCOMPARE(result.contents.items.constFirst().fields.at(0).uid, 1);
    QCOMPARE(result.contents.items.constFirst().fields.at(1).uid, 2);
}

void TestVaultContents::a_tag_is_kept_trimmed_and_nested_by_slashes()
{
    QCOMPARE(normalizedTag(QStringLiteral("  Work /  / Servers ")), QStringLiteral("Work/Servers"));
    QCOMPARE(normalizedTag(QStringLiteral("/// ")), QString());
    const ContentsResult result = contentsFromJson(format3Item(R"(,"tags":[" Work / Servers","Work/Servers"])"));
    QCOMPARE(result.status, ContentsStatus::Read);
    QCOMPARE(result.contents.items.constFirst().tags, QStringList{QStringLiteral("Work/Servers")});
}

void TestVaultContents::contents_of_the_wrong_shape_are_damaged_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::newRow("not json") << QByteArray("not json");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("an array") << QByteArray("[]");
    QTest::newRow("no format") << QByteArray(R"({"items":[]})");
    QTest::newRow("format as text") << QByteArray(R"({"format":"1","items":[]})");
    QTest::newRow("items not a list") << QByteArray(R"({"format":1,"items":{}})");
    QTest::newRow("item not an object") << QByteArray(R"({"format":1,"items":[3]})");
    QTest::newRow("item without id") << QByteArray(
        R"({"format":1,"items":[{"template":"login","title":"","created":"","modified":"","fields":[]}]})");
    QTest::newRow("unknown template before format 3") << QByteArray(
        R"({"format":1,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"car","title":"","created":"","modified":"","fields":[]}]})");
    QTest::newRow("field with role and label") << QByteArray(
        R"({"format":1,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"","created":"","modified":"","fields":[{"role":"password","label":"x","kind":"password","value":""}]}]})");
    QTest::newRow("field of unknown kind") << QByteArray(
        R"({"format":1,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"","created":"","modified":"","fields":[{"role":"password","kind":"colour","value":""}]}]})");
    QTest::newRow("icon not base64") << QByteArray(
        R"({"format":2,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"","created":"","modified":"","fields":[],"icon":"***"}]})");
    QTest::newRow("icon as number") << QByteArray(
        R"({"format":2,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"","created":"","modified":"","fields":[],"icon":7}]})");
    QTest::newRow("icon too large") << (QByteArray(
        R"({"format":2,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"","created":"","modified":"","fields":[],"icon":")")
        + QByteArray(300 * 1024, 'A').toBase64() + QByteArray(R"("}]})"));
    QTest::newRow("value as number") << QByteArray(
        R"({"format":1,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"","created":"","modified":"","fields":[{"role":"password","kind":"password","value":5}]}]})");
    QTest::newRow("format 3 without category") << QByteArray(
        R"({"format":3,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"","created":"","modified":"","fields":[]}]})");
    QTest::newRow("tags not a list") << format3Item(R"(,"tags":"Work")");
    QTest::newRow("tag empty") << format3Item(R"(,"tags":[" / "])");
    QTest::newRow("tag a number") << format3Item(R"(,"tags":[4])");
    QTest::newRow("favourite as text") << format3Item(R"(,"favourite":"yes")");
    QTest::newRow("attachment not base64") << format3Item(
        R"(,"attachments":[{"id":"7f9619ff-8b86-d011-b42d-00c04fc964ff","name":"a.txt","data":"***"}])");
    QTest::newRow("attachment without name") << format3Item(
        R"(,"attachments":[{"id":"7f9619ff-8b86-d011-b42d-00c04fc964ff","name":"","data":"QQ=="}])");
    QTest::newRow("attachment without id") << format3Item(R"(,"attachments":[{"name":"a.txt","data":"QQ=="}])");
    QTest::newRow("attachment too large") << format3Item(
        R"(,"attachments":[{"id":"7f9619ff-8b86-d011-b42d-00c04fc964ff","name":"a.txt","data":")"
        + QByteArray(largestAttachmentBytes + 1, 'A').toBase64() + R"("}])");
    QTest::newRow("categories in format 2") << QByteArray(R"({"format":2,"items":[],"categories":[]})");
    QTest::newRow("category without name") << QByteArray(R"({"format":3,"items":[],"categories":[{"id":"x"}]})");
    QTest::newRow("template without fields") << QByteArray(
        R"({"format":3,"items":[],"templates":[{"id":"x","category":"misc","name":"Car"}]})");
    QTest::newRow("hidden category a number") << QByteArray(R"({"format":3,"items":[],"hiddenCategories":[1]})");
    const auto format4Field = [](const QByteArray& field) {
        return QByteArray(R"({"format":4,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","category":"login","title":"","created":"","modified":"","fields":[)")
               + field + "]}]}";
    };
    QTest::newRow("sensitive as text") << format4Field(R"({"label":"a","kind":"text","value":"","sensitive":"yes"})");
    QTest::newRow("uid negative") << format4Field(R"({"label":"a","kind":"text","value":"","uid":-1})");
    QTest::newRow("history not a list") << format4Field(R"({"label":"a","kind":"text","value":"","history":{}})");
    QTest::newRow("history entry without time") << format4Field(R"({"label":"a","kind":"text","value":"","history":[{"value":"x"}]})");
    QTest::newRow("unknown kind") << format4Field(R"({"label":"a","kind":"colour","value":""})");
    QTest::newRow("trashed in format 3") << format3Item(R"(,"trashed":true)");
    QTest::newRow("note as number") << QByteArray(
        R"({"format":4,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","category":"login","title":"","created":"","modified":"","fields":[],"note":5}]})");
}

void TestVaultContents::contents_of_the_wrong_shape_are_damaged()
{
    QFETCH(QByteArray, json);
    const ContentsResult result = contentsFromJson(json);
    QCOMPARE(result.status, ContentsStatus::Damaged);
    QVERIFY(result.contents.items.isEmpty());
}

// The six field types added after the reference program's list, stored under
// its own words so an import and an export keep them (D-051).
void TestVaultContents::the_reference_programs_other_field_types_are_stored_by_its_words()
{
    const QList<std::pair<FieldKind, QString>> kinds{
        {FieldKind::Pin, QStringLiteral("pin")},
        {FieldKind::CardName, QStringLiteral("ccName")},
        {FieldKind::CardNumber, QStringLiteral("ccNumber")},
        {FieldKind::CardBankName, QStringLiteral("ccBankname")},
        {FieldKind::CardTransactionPassword, QStringLiteral("ccTxnpassword")},
        {FieldKind::CardValidFrom, QStringLiteral("ccValidfrom")},
    };
    for (const auto& [kind, name] : kinds) {
        QCOMPARE(fieldKindName(kind), name);
        QCOMPARE(fieldKindFromName(name), std::optional<FieldKind>(kind));
    }
}

QTEST_GUILESS_MAIN(TestVaultContents)
#include "test_vault_contents.moc"
