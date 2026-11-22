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
#include "json_import.h"
#include "templates.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_json_import.cpp 0.0.19

// --- Version history ----------------------------------------------------
// v0.0.19: Initial release (D-035)

using namespace chrono_vault;

namespace {

// A PNG of one pixel, as the export writes a custom icon: hexadecimal.
const QByteArray onePixelPng = QByteArray::fromBase64(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");

QJsonObject field(const char* type, const char* label, const char* value, int order, int sensitive = 0,
                  int deleted = 0)
{
    return QJsonObject{{"type", type}, {"label", label}, {"value", value}, {"order", order},
                       {"sensitive", sensitive}, {"deleted", deleted}, {"uid", order * 10},
                       {"updated_at", 1785260034}, {"value_updated_at", 1785260034}};
}

// A made-up export in the reference program's format, with every part
// the owner's own export has (D-035), and none of its values.
QJsonObject sampleExport()
{
    QJsonObject withHistory = field("url", "Repository", "https://example.org/new", 7);
    withHistory.insert("history", QJsonArray{QJsonObject{{"value", "https://example.org/old"}, {"updated_at", 1700000000},
                                                         {"encrypted", false}},
                                             QJsonObject{{"value", "https://example.org/older"}, {"updated_at", 1600000000},
                                                         {"encrypted", false}}});
    const QJsonArray loginFields{
        field("url", "Website", "https://example.org/login", 1),
        field("email", "E-mail", "someone@example.org", 2),
        field("username", "Username", "someone", 3),
        field("text", "Password", "not-a-real-password", 4, 1),
        field("section", "Two-step", "", 5),
        field("totp", "One-time code", "JBSWY3DPEHPK3PXP", 6),
        withHistory,
        field("text", "Security question", "", 10),
        field(".passkey_supported", "", "true_supported", 10),
        field("section", "Old section", "", 10, 0, 1),
        field("multiline", "Recovery codes", "a\nb", 13),
    };
    const QJsonArray cardFields{
        field("text", "Name", "A. Person", 1),
        field("ccExpiry", "Valid until", "08/2029", 2),
        field("ccCvc", "CVC", "123", 3),
        field("ccPin", "PIN", "0000", 4, 1),
        field("date", "Issued", "14-09-2024", 5),
        field("numeric", "Number", "1234", 6),
        field("phone", "Phone", "", 7),
        field(".twoFA", "2FA", "abcdefghijklmnopqrstuvwxyz012345", 8),
        field("ccType", "Type", "Visa", 9),
    };
    const QString customCategory = QStringLiteral("8b0e6a3e-1c1d-4c0a-9a4b-2f4a51c0d001");
    const QString customTemplate = QStringLiteral("8b0e6a3e-1c1d-4c0a-9a4b-2f4a51c0d002");
    const QString iconUuid = QStringLiteral("8b0e6a3e-1c1d-4c0a-9a4b-2f4a51c0d003");
    return QJsonObject{
        {"custom_icons", QJsonArray{QJsonObject{{"uuid", iconUuid}, {"data", QString::fromLatin1(onePixelPng.toHex())}},
                                    QJsonObject{{"uuid", iconUuid}, {"data", QString::fromLatin1(onePixelPng.toHex())}}}},
        {"folders", QJsonArray{QJsonObject{{"uuid", "f1"}, {"title", "Work"}, {"parent_uuid", ""}},
                               QJsonObject{{"uuid", "f2"}, {"title", "Servers"}, {"parent_uuid", "f1"}}}},
        {"items",
         QJsonArray{
             QJsonObject{{"uuid", "6f9619ff-8b86-d011-b42d-00c04fc96401"}, {"title", "GITHUB"}, {"subtitle", "someone"},
                         {"note", ""}, {"category", "login"}, {"category_name", "Login"},
                         {"template_type", "login.default"}, {"favorite", 1}, {"trashed", 0}, {"archived", 0},
                         {"auto_submit", 1}, {"createdAt", 1726302325}, {"updated_at", 1785260034},
                         {"field_updated_at", 1785260034},
                         {"icon", QJsonObject{{"fav", "github.com"}, {"type", 1}, {"uuid", ""},
                                              {"image", QJsonObject{{"file", "misc/login"}}}}},
                         {"folders", QJsonArray{"f2"}}, {"fields", loginFields}},
             QJsonObject{{"uuid", "6f9619ff-8b86-d011-b42d-00c04fc96402"}, {"title", "BANK"}, {"subtitle", ""},
                         {"note", "a note"}, {"category", customCategory}, {"category_name", "Bank cards"},
                         {"template_type", customTemplate}, {"favorite", 0}, {"trashed", 0}, {"archived", 1},
                         {"auto_submit", 0}, {"createdAt", 1726302325}, {"updated_at", 1785260034},
                         {"icon", QJsonObject{{"fav", ""}, {"type", 2}, {"uuid", iconUuid},
                                              {"image", QJsonObject{{"file", "misc/login"}}}}},
                         {"attachments", QJsonArray{QJsonObject{{"name", "scan.png"}, {"kind", "image/png"},
                                                                {"data", QString::fromLatin1(onePixelPng.toBase64())},
                                                                {"order", 1}, {"updated", 1785260034},
                                                                {"uuid", "6f9619ff-8b86-d011-b42d-00c04fc96410"}}}},
                         {"fields", cardFields}},
             QJsonObject{{"uuid", "6f9619ff-8b86-d011-b42d-00c04fc96403"}, {"title", "Old note"}, {"subtitle", ""},
                         {"note", "gone"}, {"category", "note"}, {"category_name", "Note"},
                         {"template_type", "note.default"}, {"favorite", 0}, {"trashed", 1}, {"archived", 0},
                         {"auto_submit", 0}, {"createdAt", 1726302325}, {"updated_at", 1785260034},
                         {"icon", QJsonObject{{"fav", ""}, {"type", 1}, {"uuid", ""}}}},
         }},
    };
}

QByteArray toJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

class TestJsonImport : public QObject
{
    Q_OBJECT

private slots:
    void every_item_is_read_as_the_reference_program_holds_it();
    void fields_keep_their_order_types_marks_and_history();
    void categories_templates_icons_tags_and_attachments_come_along();
    void categories_and_templates_already_in_the_vault_are_not_made_again();
    void an_export_of_the_wrong_shape_is_refused_whole_data();
    void an_export_of_the_wrong_shape_is_refused_whole();
};

void TestJsonImport::every_item_is_read_as_the_reference_program_holds_it()
{
    const ImportResult result = readReferenceExport(toJson(sampleExport()), {});
    QCOMPARE(result.status, ImportStatus::Read);
    QCOMPARE(result.items.size(), 3);
    const Item& login = result.items.at(0);
    QCOMPARE(login.id, QUuid::fromString(QStringLiteral("6f9619ff-8b86-d011-b42d-00c04fc96401")));
    QCOMPARE(login.title, QStringLiteral("GITHUB"));
    QCOMPARE(login.subtitle, QStringLiteral("someone"));
    QCOMPARE(login.categoryId, QStringLiteral("login"));
    QCOMPARE(login.templateId, QStringLiteral("login"));
    QVERIFY(login.favourite);
    QVERIFY(login.autoSubmit);
    QVERIFY(!login.trashed && !login.archived);
    QCOMPARE(login.created, QDateTime::fromSecsSinceEpoch(1726302325, QTimeZone::UTC));
    QCOMPARE(login.modified, QDateTime::fromSecsSinceEpoch(1785260034, QTimeZone::UTC));
    QCOMPARE(login.website, QStringLiteral("github.com"));
    QVERIFY(login.icon.isEmpty());
    QVERIFY(result.items.at(1).archived);
    QVERIFY(result.items.at(2).trashed);
    QCOMPARE(result.items.at(1).note, QStringLiteral("a note"));
    QCOMPARE(referenceProgramName(), QStringLiteral("Enpass"));
}

void TestJsonImport::fields_keep_their_order_types_marks_and_history()
{
    const ImportResult result = readReferenceExport(toJson(sampleExport()), {});
    const QList<Field>& fields = result.items.at(0).fields;
    QStringList shape;
    for (const Field& field : fields) {
        shape << fieldKindName(field.kind) + QLatin1Char(':') + field.label + (field.sensitive ? QStringLiteral("*") : QString()) +
                     (field.deleted ? QStringLiteral("-") : QString()) + (field.hiddenType.isEmpty() ? QString() : QLatin1Char('=') + field.hiddenType);
    }
    QCOMPARE(shape, QStringList({"url:Website", "email:E-mail", "username:Username", "text:Password*", "section:Two-step",
                                 "totp:One-time code", "url:Repository", "text:Security question",
                                 "hidden:=.passkey_supported", "section:Old section-", "multiline:Recovery codes"}));
    QCOMPARE(fields.at(3).value, QStringLiteral("not-a-real-password"));
    QCOMPARE(fields.at(0).uid, 10);
    QCOMPARE(fields.at(0).changed, QDateTime::fromSecsSinceEpoch(1785260034, QTimeZone::UTC));
    const QList<FieldChange>& history = fields.at(6).history;
    QCOMPARE(history.size(), 2);
    QCOMPARE(history.at(0).value, QStringLiteral("https://example.org/older")); // oldest first
    QCOMPARE(history.at(1).value, QStringLiteral("https://example.org/old"));
    QStringList cardKinds;
    for (const Field& field : result.items.at(1).fields) {
        cardKinds << fieldKindName(field.kind);
    }
    QCOMPARE(cardKinds, QStringList({"text", "ccExpiry", "ccCvc", "ccPin", "date", "numeric", "phone", "hidden", "ccType"}));
    QVERIFY(result.items.at(1).fields.at(3).sensitive);
    QCOMPARE(result.items.at(1).fields.at(7).value, QStringLiteral("abcdefghijklmnopqrstuvwxyz012345"));
}

void TestJsonImport::categories_templates_icons_tags_and_attachments_come_along()
{
    const ImportResult result = readReferenceExport(toJson(sampleExport()), {});
    QCOMPARE(result.categories.size(), 1);
    QCOMPARE(result.categories.constFirst().name, QStringLiteral("Bank cards"));
    QCOMPARE(result.templates.size(), 1);
    const Template& made = result.templates.constFirst();
    QCOMPARE(made.categoryId, result.categories.constFirst().id);
    QCOMPARE(made.fields.size(), result.items.at(1).fields.size());
    QVERIFY(std::all_of(made.fields.cbegin(), made.fields.cend(), [](const Field& one) { return one.value.isEmpty(); }));
    const Item& bank = result.items.at(1);
    QCOMPARE(bank.categoryId, result.categories.constFirst().id);
    QCOMPARE(bank.templateId, made.id);
    QCOMPARE(bank.icon, onePixelPng);
    QCOMPARE(bank.attachments.size(), 1);
    QCOMPARE(bank.attachments.constFirst().name, QStringLiteral("scan.png"));
    QCOMPARE(bank.attachments.constFirst().data, onePixelPng);
    QCOMPARE(result.attachmentCount, 1);
    QCOMPARE(result.items.at(0).tags, QStringList{QStringLiteral("Work/Servers")});
}

void TestJsonImport::categories_and_templates_already_in_the_vault_are_not_made_again()
{
    const ImportResult first = readReferenceExport(toJson(sampleExport()), {});
    VaultContents vault;
    vault.categories = first.categories;
    vault.templates = first.templates;
    const ImportResult again = readReferenceExport(toJson(sampleExport()), vault);
    QCOMPARE(again.status, ImportStatus::Read);
    QVERIFY(again.categories.isEmpty());
    QVERIFY(again.templates.isEmpty());
    QCOMPARE(again.items.size(), 3);
}

void TestJsonImport::an_export_of_the_wrong_shape_is_refused_whole_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<ImportStatus>("status");
    QTest::newRow("not json") << QByteArray("not json") << ImportStatus::NotTheFormat;
    QTest::newRow("no items") << QByteArray(R"({"folders":[]})") << ImportStatus::NotTheFormat;
    QTest::newRow("items not a list") << QByteArray(R"({"items":{}})") << ImportStatus::NotTheFormat;
    QTest::newRow("item not an object") << QByteArray(R"({"items":[3]})") << ImportStatus::Damaged;
    QTest::newRow("title a number") << QByteArray(R"({"items":[{"title":5}]})") << ImportStatus::Damaged;
    QTest::newRow("fields not a list") << QByteArray(R"({"items":[{"title":"a","fields":{}}]})") << ImportStatus::Damaged;
    QTest::newRow("field value a number")
        << QByteArray(R"({"items":[{"title":"a","fields":[{"type":"text","label":"x","value":5}]}]})") << ImportStatus::Damaged;
    QTest::newRow("sensitive as text")
        << QByteArray(R"({"items":[{"title":"a","fields":[{"type":"text","label":"x","value":"","sensitive":"yes"}]}]})")
        << ImportStatus::Damaged;
    QTest::newRow("history not a list")
        << QByteArray(R"({"items":[{"title":"a","fields":[{"type":"text","label":"x","value":"","history":5}]}]})")
        << ImportStatus::Damaged;
    QTest::newRow("icon not a PNG") << QByteArray(R"({"custom_icons":[{"uuid":"a","data":"414243"}],"items":[]})")
                                    << ImportStatus::Damaged;
    QTest::newRow("attachment not base64")
        << QByteArray(R"({"items":[{"title":"a","attachments":[{"name":"x","data":"***"}]}]})") << ImportStatus::Damaged;
    QTest::newRow("attachment too large")
        << (QByteArray(R"({"items":[{"title":"a","attachments":[{"name":"x","data":")") +
            QByteArray(largestAttachmentBytes + 1, 'A').toBase64() + R"("}]}]})")
        << ImportStatus::Damaged;
    QTest::newRow("favorite as text") << QByteArray(R"({"items":[{"title":"a","favorite":"yes"}]})") << ImportStatus::Damaged;
}

void TestJsonImport::an_export_of_the_wrong_shape_is_refused_whole()
{
    QFETCH(QByteArray, json);
    QFETCH(ImportStatus, status);
    const ImportResult result = readReferenceExport(json, {});
    QCOMPARE(result.status, status);
    QVERIFY(result.items.isEmpty());
}

QTEST_GUILESS_MAIN(TestJsonImport)
#include "test_json_import.moc"
