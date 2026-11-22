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

// --- Version ------------------------------------------------------------
// templates.cpp 0.0.14

// --- Version history ----------------------------------------------------
// v0.0.14: Initial release (D-027)

namespace chrono_vault {

namespace {

Field field(const char* role, FieldKind kind)
{
    return Field{QString::fromLatin1(role), {}, kind, {}};
}

Template builtIn(const char* id, const char* categoryId, QList<Field> fields)
{
    fields.append(field("notes", FieldKind::MultiLine));
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
                {field("username", Text), field("password", Password), field("url", WebAddress),
                 field("totp", OneTimeCode)}),
        builtIn("email", "login", {field("email", Text), field("password", Password), field("server", Text)}),
        builtIn("card", "card",
                {field("cardholder", Text), field("cardnumber", Password), field("expiry", Text),
                 field("cvc", Password), field("pin", Password)}),
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
                 field("iban", Text), field("bic", Text), field("pin", Password)}),
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
    }
    return item;
}

QString categoryIconSource(const QString& categoryId)
{
    return QStringLiteral("qrc:/icons/item-%1.svg").arg(isBuiltInCategory(categoryId) ? categoryId : QStringLiteral("misc"));
}

} // namespace chrono_vault
