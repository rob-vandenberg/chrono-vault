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


#include "secret_store.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QEventLoop>
#include <QMap>
#include <QObject>
#include <QTimer>

// --- Version ------------------------------------------------------------
// secret_store_linux.cpp 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

// The desktop keyring through the Secret Service (KWallet or GNOME Keyring),
// over the session's own bus.

struct KeyringSecret
{
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};
Q_DECLARE_METATYPE(KeyringSecret)
using KeyringSecrets = QMap<QDBusObjectPath, KeyringSecret>;
Q_DECLARE_METATYPE(KeyringSecrets)

QDBusArgument& operator<<(QDBusArgument& argument, const KeyringSecret& secret)
{
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value << secret.contentType;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, KeyringSecret& secret)
{
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    argument.endStructure();
    return argument;
}

namespace chrono_vault {

namespace {

const QString service = QStringLiteral("org.freedesktop.secrets");
const QString servicePath = QStringLiteral("/org/freedesktop/secrets");
const QString serviceInterface = QStringLiteral("org.freedesktop.Secret.Service");
const QString defaultCollection = QStringLiteral("/org/freedesktop/secrets/aliases/default");

// Waits for the desktop's question (a prompt) to be answered: Prompt
// returns at once, the answer comes as the Completed signal.
class PromptWaiter : public QObject
{
    Q_OBJECT

public:
    void wait(const QString& promptPath)
    {
        QEventLoop loop;
        connect(this, &PromptWaiter::answered, &loop, &QEventLoop::quit);
        QTimer::singleShot(120000, &loop, &QEventLoop::quit); // the person may walk away
        QDBusConnection::sessionBus().connect(service, promptPath, QStringLiteral("org.freedesktop.Secret.Prompt"),
                                              QStringLiteral("Completed"), this,
                                              SLOT(completed(bool, QDBusVariant)));
        QDBusInterface asking(service, promptPath, QStringLiteral("org.freedesktop.Secret.Prompt"),
                              QDBusConnection::sessionBus());
        asking.setTimeout(120000);
        const QDBusMessage started = asking.call(QStringLiteral("Prompt"), QString());
        if (started.type() == QDBusMessage::ReplyMessage && !done) {
            loop.exec();
        }
        QDBusConnection::sessionBus().disconnect(service, promptPath, QStringLiteral("org.freedesktop.Secret.Prompt"),
                                                 QStringLiteral("Completed"), this,
                                                 SLOT(completed(bool, QDBusVariant)));
    }

signals:
    void answered();

private slots:
    void completed(bool, const QDBusVariant&)
    {
        done = true;
        emit answered();
    }

private:
    bool done = false;
};

class KeyringStore : public SecretStore
{
public:
    KeyringStore()
    {
        qDBusRegisterMetaType<KeyringSecret>();
        qDBusRegisterMetaType<KeyringSecrets>();
    }

    bool keep(const QString& name, const QByteArray& secret) override
    {
        const QDBusObjectPath session = openSession();
        if (session.path().isEmpty()) {
            return false;
        }
        unlockAll({QDBusObjectPath(defaultCollection)});
        QDBusInterface collection(service, defaultCollection, QStringLiteral("org.freedesktop.Secret.Collection"),
                                  QDBusConnection::sessionBus());
        collection.setTimeout(30000);
        const QVariantMap properties{
            {QStringLiteral("org.freedesktop.Secret.Item.Label"), QStringLiteral("Chrono Vault: %1").arg(name)},
            {QStringLiteral("org.freedesktop.Secret.Item.Attributes"), QVariant::fromValue(attributes(name))},
        };
        const KeyringSecret value{session, {}, secret, QStringLiteral("text/plain")};
        // Replace: an earlier secret under the same name gives way.
        const QDBusMessage reply =
            collection.call(QStringLiteral("CreateItem"), properties, QVariant::fromValue(value), true);
        return reply.type() == QDBusMessage::ReplyMessage;
    }

    std::pair<Result, QByteArray> find(const QString& name) override
    {
        const QDBusObjectPath session = openSession();
        if (session.path().isEmpty()) {
            return {Result::Unavailable, {}};
        }
        QDBusInterface secrets(service, servicePath, serviceInterface, QDBusConnection::sessionBus());
        secrets.setTimeout(30000);
        const QDBusMessage found =
            secrets.call(QStringLiteral("SearchItems"), QVariant::fromValue(attributes(name)));
        if (found.type() != QDBusMessage::ReplyMessage || found.arguments().size() != 2) {
            return {Result::Unavailable, {}};
        }
        QList<QDBusObjectPath> items = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(0));
        const QList<QDBusObjectPath> locked = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(1));
        if (items.isEmpty() && locked.isEmpty()) {
            return {Result::NoSecret, {}};
        }
        if (!locked.isEmpty()) {
            unlockAll(locked);
            items += locked;
        }
        const QDBusMessage reply =
            secrets.call(QStringLiteral("GetSecrets"), QVariant::fromValue(items), QVariant::fromValue(session));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            return {Result::Unavailable, {}};
        }
        const auto values = qdbus_cast<KeyringSecrets>(reply.arguments().at(0));
        if (values.isEmpty() || values.first().value.isEmpty()) {
            return {Result::Unavailable, {}}; // still locked: the desktop's question was dismissed
        }
        return {Result::Done, values.first().value};
    }

    bool remove(const QString& name) override
    {
        QDBusInterface secrets(service, servicePath, serviceInterface, QDBusConnection::sessionBus());
        secrets.setTimeout(30000);
        const auto search = [&secrets, &name]() {
            QList<QDBusObjectPath> all;
            QList<QDBusObjectPath> locked;
            const QDBusMessage found =
                secrets.call(QStringLiteral("SearchItems"), QVariant::fromValue(attributes(name)));
            if (found.type() != QDBusMessage::ReplyMessage || found.arguments().size() != 2) {
                return std::pair{all, false};
            }
            all = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(0));
            locked = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(1));
            if (!locked.isEmpty()) {
                unlockAll(locked); // a locked item cannot be deleted
            }
            return std::pair{all + locked, true};
        };
        auto [items, answered] = search();
        if (!answered) {
            return false;
        }
        for (const QDBusObjectPath& item : std::as_const(items)) {
            QDBusInterface one(service, item.path(), QStringLiteral("org.freedesktop.Secret.Item"),
                               QDBusConnection::sessionBus());
            one.call(QStringLiteral("Delete"));
        }
        const auto [left, again] = search();
        return again && left.isEmpty();
    }

private:
    static QMap<QString, QString> attributes(const QString& name)
    {
        return {{QStringLiteral("application"), QStringLiteral("chrono-vault")}, {QStringLiteral("secret"), name}};
    }

    static QDBusObjectPath openSession()
    {
        QDBusInterface secrets(service, servicePath, serviceInterface, QDBusConnection::sessionBus());
        secrets.setTimeout(30000);
        const QDBusMessage reply = secrets.call(QStringLiteral("OpenSession"), QStringLiteral("plain"),
                                                QVariant::fromValue(QDBusVariant(QString())));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2) {
            return {};
        }
        return qvariant_cast<QDBusObjectPath>(reply.arguments().at(1));
    }

    // The desktop may ask for the login password here.
    static void unlockAll(const QList<QDBusObjectPath>& objects)
    {
        QDBusInterface secrets(service, servicePath, serviceInterface, QDBusConnection::sessionBus());
        secrets.setTimeout(120000);
        const QDBusMessage reply = secrets.call(QStringLiteral("Unlock"), QVariant::fromValue(objects));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2) {
            return;
        }
        const auto prompt = qvariant_cast<QDBusObjectPath>(reply.arguments().at(1));
        if (prompt.path() != QLatin1String("/")) {
            PromptWaiter().wait(prompt.path()); // until the person answers the desktop's question
        }
    }
};

} // namespace

std::unique_ptr<SecretStore> makeSecretStore()
{
    return std::make_unique<KeyringStore>();
}

} // namespace chrono_vault

#include "secret_store_linux.moc"
