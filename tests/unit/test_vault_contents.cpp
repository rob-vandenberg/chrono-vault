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

#include "vault_contents.h"

#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vault_contents.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Icons, and the contents format 2
// v0.0.2: Initial release

using namespace chrono_vault;

class TestVaultContents : public QObject
{
    Q_OBJECT

private slots:
    void a_new_login_has_the_login_fields();
    void a_new_secure_note_has_only_notes();
    void contents_come_back_exactly_as_they_were_written();
    void an_empty_vault_comes_back_empty();
    void a_newer_format_is_refused();
    void the_old_format_1_is_still_read();
    void contents_of_the_wrong_shape_are_damaged_data();
    void contents_of_the_wrong_shape_are_damaged();
};

void TestVaultContents::a_new_login_has_the_login_fields()
{
    const Item item = newItem(ItemTemplate::Login);
    QVERIFY(!item.id.isNull());
    QStringList roles;
    for (const Field& field : item.fields) {
        roles << field.role;
    }
    QCOMPARE(roles, QStringList({"username", "password", "url", "totp", "notes"}));
    QCOMPARE(item.fields.at(1).kind, FieldKind::Password);
    QCOMPARE(roleName(QStringLiteral("username")), QStringLiteral("User name"));
}

void TestVaultContents::a_new_secure_note_has_only_notes()
{
    const Item item = newItem(ItemTemplate::SecureNote);
    QCOMPARE(item.fields.size(), 1);
    QCOMPARE(item.fields.constFirst().kind, FieldKind::MultiLine);
}

void TestVaultContents::contents_come_back_exactly_as_they_were_written()
{
    VaultContents contents;
    Item login = newItem(ItemTemplate::Login);
    login.title = QStringLiteral("Bank – €uro \"quoted\"");
    login.fields[0].value = QStringLiteral("anna@example.org");
    login.fields[1].value = QStringLiteral("p@ss\nword");
    login.fields.append({{}, QStringLiteral("PIN"), FieldKind::Password, QStringLiteral("1234")});
    login.icon = QByteArray("\x89PNG not really, but bytes all the same", 40);
    contents.items << login << newItem(ItemTemplate::SecureNote);

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
    QCOMPARE(contentsFromJson(R"({"format":3,"items":[]})").status, ContentsStatus::NewerFormat);
}

void TestVaultContents::the_old_format_1_is_still_read()
{
    const ContentsResult result = contentsFromJson(
        R"({"format":1,"items":[{"id":"6f9619ff-8b86-d011-b42d-00c04fc964ff","template":"login","title":"Old","created":"","modified":"","fields":[]}]})");
    QCOMPARE(result.status, ContentsStatus::Read);
    QCOMPARE(result.contents.items.size(), 1);
    QVERIFY(result.contents.items.constFirst().icon.isEmpty());
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
    QTest::newRow("unknown template") << QByteArray(
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
}

void TestVaultContents::contents_of_the_wrong_shape_are_damaged()
{
    QFETCH(QByteArray, json);
    const ContentsResult result = contentsFromJson(json);
    QCOMPARE(result.status, ContentsStatus::Damaged);
    QVERIFY(result.contents.items.isEmpty());
}

QTEST_GUILESS_MAIN(TestVaultContents)
#include "test_vault_contents.moc"
