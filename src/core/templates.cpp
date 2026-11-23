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

#include <QCoreApplication>
#include <QDateTime>
#include <QUrl>

// --- Version ------------------------------------------------------------
// templates.cpp 0.1.41

// --- Version history ----------------------------------------------------
// v0.1.41: No Notes field in the built-in templates (D-051)
// v0.0.20: The website an item belongs to, for its icon (D-036)
// v0.0.17: A new item's fields get their ids at once; fields made with makeField, a PIN hidden (D-033)
// v0.0.14: Initial release (D-027)

namespace chrono_vault {

namespace {

Field field(const char* role, FieldKind kind)
{
    return makeField(QString::fromLatin1(role), {}, kind);
}

Template builtIn(const char* id, const char* categoryId, QList<Field> fields)
{
    // No "Notes" field: every item has its own Notes, apart from its fields
    // (the owner, D-051).
    return Template{QString::fromLatin1(id), QString::fromLatin1(categoryId), {}, fields, true};
}

} // namespace

QStringList builtInCategoryIds()
{
    return {QStringLiteral("login"),    QStringLiteral("card"),     QStringLiteral("identity"),
            QStringLiteral("note"),     QStringLiteral("password"), QStringLiteral("finance"),
            QStringLiteral("computer"), QStringLiteral("misc")};
}

bool isBuiltInCategory(const QString& id)
{
    return builtInCategoryIds().contains(id);
}

const QList<Template>& builtInTemplates()
{
    using enum FieldKind;
    static const QList<Template> templates{
        builtIn("login", "login",
                {field("username", Username), field("password", Password), field("url", WebAddress),
                 field("totp", OneTimeCode)}),
        builtIn("email", "login", {field("email", Email), field("password", Password), field("server", Text)}),
        builtIn("card", "card",
                {field("cardholder", Text), field("cardnumber", Password), field("expiry", CardExpiry),
                 field("cvc", CardCvc), field("pin", CardPin)}),
        builtIn("identity", "identity",
                {field("fullname", Text), field("birthday", Text), field("address", MultiLine), field("phone", Text),
                 field("email", Text)}),
        builtIn("passport", "identity",
                {field("fullname", Text), field("number", Text), field("nationality", Text), field("issued", Text),
                 field("expires", Text)}),
        builtIn("driving", "identity",
                {field("fullname", Text), field("number", Text), field("issued", Text), field("expires", Text)}),
        builtIn("note", "note", {}),
        builtIn("password", "password", {field("password", Password)}),
        builtIn("bank", "finance",
                {field("bankname", Text), field("accountholder", Text), field("accountnumber", Text),
                 field("iban", Text), field("bic", Text), field("pin", CardPin)}),
        builtIn("wifi", "computer", {field("network", Text), field("password", Password), field("security", Text)}),
        builtIn("server", "computer",
                {field("url", WebAddress), field("username", Text), field("password", Password)}),
        builtIn("licence", "computer",
                {field("product", Text), field("licencekey", Text), field("licensedto", Text), field("email", Text)}),
        builtIn("membership", "misc",
                {field("organisation", Text), field("membernumber", Text), field("fullname", Text),
                 field("expires", Text)}),
    };
    return templates;
}

QString categoryName(const QString& id, const VaultContents& contents)
{
    static const QHash<QString, const char*> names{
        {QStringLiteral("login"), QT_TRANSLATE_NOOP("Category", "Login")},
        {QStringLiteral("card"), QT_TRANSLATE_NOOP("Category", "Credit card")},
        {QStringLiteral("identity"), QT_TRANSLATE_NOOP("Category", "Identity")},
        {QStringLiteral("note"), QT_TRANSLATE_NOOP("Category", "Secure note")},
        {QStringLiteral("password"), QT_TRANSLATE_NOOP("Category", "Password")},
        {QStringLiteral("finance"), QT_TRANSLATE_NOOP("Category", "Finance")},
        {QStringLiteral("computer"), QT_TRANSLATE_NOOP("Category", "Computer")},
        {QStringLiteral("misc"), QT_TRANSLATE_NOOP("Category", "Miscellaneous")},
    };
    if (const auto found = names.constFind(id); found != names.constEnd()) {
        return QCoreApplication::translate("Category", found.value());
    }
    for (const Category& category : contents.categories) {
        if (category.id == id) {
            return category.name;
        }
    }
    return id;
}

QString templateName(const Template& itemTemplate)
{
    if (!itemTemplate.builtIn) {
        return itemTemplate.name;
    }
    static const QHash<QString, const char*> names{
        {QStringLiteral("login"), QT_TRANSLATE_NOOP("Template", "Login")},
        {QStringLiteral("email"), QT_TRANSLATE_NOOP("Template", "Email account")},
        {QStringLiteral("card"), QT_TRANSLATE_NOOP("Template", "Credit card")},
        {QStringLiteral("identity"), QT_TRANSLATE_NOOP("Template", "Identity")},
        {QStringLiteral("passport"), QT_TRANSLATE_NOOP("Template", "Passport")},
        {QStringLiteral("driving"), QT_TRANSLATE_NOOP("Template", "Driver's license")},
        {QStringLiteral("note"), QT_TRANSLATE_NOOP("Template", "Secure note")},
        {QStringLiteral("password"), QT_TRANSLATE_NOOP("Template", "Password")},
        {QStringLiteral("bank"), QT_TRANSLATE_NOOP("Template", "Bank account")},
        {QStringLiteral("wifi"), QT_TRANSLATE_NOOP("Template", "Wi-Fi")},
        {QStringLiteral("server"), QT_TRANSLATE_NOOP("Template", "Server")},
        {QStringLiteral("licence"), QT_TRANSLATE_NOOP("Template", "Software license")},
        {QStringLiteral("membership"), QT_TRANSLATE_NOOP("Template", "Membership")},
    };
    const auto found = names.constFind(itemTemplate.id);
    return found == names.constEnd() ? itemTemplate.id : QCoreApplication::translate("Template", found.value());
}

std::optional<Template> findTemplate(const QString& id, const VaultContents& contents)
{
    for (const Template& one : builtInTemplates()) {
        if (one.id == id) {
            return one;
        }
    }
    for (const Template& one : contents.templates) {
        if (one.id == id) {
            return one;
        }
    }
    return std::nullopt;
}

Item newItem(const Template& itemTemplate)
{
    Item item;
    item.id = QUuid::createUuid();
    item.templateId = itemTemplate.id;
    item.categoryId = itemTemplate.categoryId;
    item.created = QDateTime::currentDateTimeUtc();
    item.modified = item.created;
    item.fields = itemTemplate.fields;
    for (Field& one : item.fields) {
        one.value.clear();
        one.uid = 0;
    }
    giveFieldsIds(item); // so its first change is kept in the history (D-033)
    return item;
}

QString websiteHostOf(const Item& item)
{
    QString address = item.website;
    if (address.isEmpty()) {
        for (const Field& field : item.fields) {
            if (field.kind == FieldKind::WebAddress && !field.deleted && !field.value.trimmed().isEmpty()) {
                address = field.value.trimmed();
                break;
            }
        }
    }
    if (address.isEmpty()) {
        return {};
    }
    const QUrl url = QUrl::fromUserInput(address);
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        return {};
    }
    return url.host().toLower();
}

QString categoryIconSource(const QString& categoryId)
{
    return QStringLiteral("qrc:/icons/item-%1.svg").arg(isBuiltInCategory(categoryId) ? categoryId : QStringLiteral("misc"));
}

} // namespace chrono_vault
