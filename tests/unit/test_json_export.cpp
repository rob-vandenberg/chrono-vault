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
#include "json_export.h"
#include "json_import.h"
#include "templates.h"

#include <QTimeZone>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_json_export.cpp 0.0.22

// --- Version history ----------------------------------------------------
// v0.0.22: Initial release (D-037)

using namespace chrono_vault;

namespace {

// What a person sees of an item, compared after an export and an import.
QStringList seen(const Item& item, const VaultContents& contents)
{
    QStringList lines{item.id.toString(), item.title, item.subtitle, item.note, item.categoryId, item.templateId,
                      categoryName(item.categoryId, contents), item.tags.join(QLatin1Char(',')), item.website,
                      QString::number(item.favourite), QString::number(item.trashed), QString::number(item.archived),
                      QString::number(item.autoSubmit), item.created.toString(Qt::ISODate),
                      item.modified.toString(Qt::ISODate), QString::fromLatin1(item.icon.toBase64())};
    for (const Field& field : item.fields) {
        QString history;
        for (const FieldChange& change : field.history) {
            history += change.value + QLatin1Char('@') + change.changed.toString(Qt::ISODate) + QLatin1Char(';');
        }
        lines << QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8")
                     .arg(fieldKindName(field.kind), field.isCustom() ? field.label : roleName(field.role), field.value)
                     .arg(field.isHidden())
                     .arg(field.deleted)
                     .arg(field.hiddenType)
                     .arg(field.uid)
                     .arg(history);
    }
    for (const Attachment& attachment : item.attachments) {
        lines << attachment.id.toString() + attachment.name + QString::fromLatin1(attachment.data.toBase64());
    }
    return lines;
}

QDateTime at(qint64 seconds)
{
    return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::UTC);
}

} // namespace

class TestJsonExport : public QObject
{
    Q_OBJECT

private slots:
    void every_item_comes_back_as_it_was_exported();
    void an_empty_vault_exports_an_empty_list();
};

void TestJsonExport::every_item_comes_back_as_it_was_exported()
{
    VaultContents contents;
    contents.categories << Category{QStringLiteral("8b0e6a3e-1c1d-4c0a-9a4b-2f4a51c0d001"), QStringLiteral("Bank cards"), {}};
    contents.templates << Template{QStringLiteral("8b0e6a3e-1c1d-4c0a-9a4b-2f4a51c0d002"), contents.categories.constFirst().id,
                                   QStringLiteral("Bank cards"), {makeField({}, QStringLiteral("Number"), FieldKind::Text)},
                                   false};
    const QByteArray png = QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==");

    Item login = newItem(*findTemplate(QStringLiteral("login"), contents));
    login.title = QStringLiteral("GITHUB");
    login.fields[0].value = QStringLiteral("someone");
    login.fields[1].value = QStringLiteral("not-a-real-password");
    login.fields[1].history << FieldChange{QStringLiteral("older"), at(1600000000)};
    login.fields[1].changed = at(1700000000);
    Field heading = makeField({}, QStringLiteral("Two-step"), FieldKind::Section);
    heading.uid = 20;
    Field marker = makeField({}, QStringLiteral("2FA"), FieldKind::Hidden, QStringLiteral("abc"));
    marker.hiddenType = QStringLiteral(".twoFA");
    marker.uid = 21;
    Field gone = makeField({}, QStringLiteral("Old"), FieldKind::Text, QStringLiteral("x"));
    gone.deleted = true;
    gone.uid = 22;
    login.fields << heading << marker << gone;
    login.tags = {QStringLiteral("Work/Servers"), QStringLiteral("Home")};
    login.favourite = true;
    login.autoSubmit = true;
    login.website = QStringLiteral("github.com");
    login.created = at(1726302325);
    login.modified = at(1785260034);

    Item card = newItem(*findTemplate(QStringLiteral("card"), contents));
    card.title = QStringLiteral("Card");
    card.archived = true;
    card.icon = png;
    card.created = at(1726302325);
    card.modified = at(1785260034);
    card.attachments << Attachment{QUuid::createUuid(), QStringLiteral("scan.png"), png};

    Item own = newItem(contents.templates.constFirst());
    own.title = QStringLiteral("Mine");
    own.note = QStringLiteral("a note");
    own.subtitle = QStringLiteral("own line");
    own.trashed = true;
    own.created = at(1726302325);
    own.modified = at(1785260034);
    contents.items << login << card << own;

    const ImportResult back = readReferenceExport(writeReferenceExport(contents), contents);
    QCOMPARE(back.status, ImportStatus::Read);
    QCOMPARE(back.items.size(), 3);
    QVERIFY(back.categories.isEmpty()); // already in the vault
    QVERIFY(back.templates.isEmpty());
    for (qsizetype index = 0; index < contents.items.size(); ++index) {
        QCOMPARE(seen(back.items.at(index), contents), seen(contents.items.at(index), contents));
    }
}

void TestJsonExport::an_empty_vault_exports_an_empty_list()
{
    const ImportResult back = readReferenceExport(writeReferenceExport({}), {});
    QCOMPARE(back.status, ImportStatus::Read);
    QVERIFY(back.items.isEmpty());
}

QTEST_GUILESS_MAIN(TestJsonExport)
#include "test_json_export.moc"
