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
#include "quick_unlock.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>
#include <utility>

#include <unistd.h>

// --- Version ------------------------------------------------------------
// quick_unlock_linux.cpp 0.0.23

// --- Version history ----------------------------------------------------
// v0.0.23: Initial release (D-038)

// Linux (D-038, the weaker terms the owner accepted): fprintd checks the
// finger; the vault's key is kept in the desktop keyring through the Secret
// Service, readable by any program of the person's session once unlocked.

struct SecretValue
{
    QDBusObjectPath session;
    QByteArray parameters;
    QByteArray value;
    QString contentType;
};
Q_DECLARE_METATYPE(SecretValue)
using SecretMap = QMap<QDBusObjectPath, SecretValue>;
Q_DECLARE_METATYPE(SecretMap)

QDBusArgument& operator<<(QDBusArgument& argument, const SecretValue& secret)
{
    argument.beginStructure();
    argument << secret.session << secret.parameters << secret.value << secret.contentType;
    argument.endStructure();
    return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, SecretValue& secret)
{
    argument.beginStructure();
    argument >> secret.session >> secret.parameters >> secret.value >> secret.contentType;
    argument.endStructure();
    return argument;
}

namespace chrono_vault {

namespace {

const QString fprint = QStringLiteral("net.reactivated.Fprint");
const QString secrets = QStringLiteral("org.freedesktop.secrets");
const QString secretsPath = QStringLiteral("/org/freedesktop/secrets");
const QString defaultCollection = QStringLiteral("/org/freedesktop/secrets/aliases/default");

QString userName()
{
    return qEnvironmentVariable("USER");
}

class LinuxQuickUnlock : public QuickUnlock
{
    Q_OBJECT

public:
    LinuxQuickUnlock()
    {
        qDBusRegisterMetaType<SecretValue>();
        qDBusRegisterMetaType<SecretMap>();
    }

    [[nodiscard]] bool available() const override
    {
        QDBusInterface manager(fprint, QStringLiteral("/net/reactivated/Fprint/Manager"),
                               QStringLiteral("net.reactivated.Fprint.Manager"), QDBusConnection::systemBus());
        manager.setTimeout(3000);
        const QDBusReply<QDBusObjectPath> device = manager.call(QStringLiteral("GetDefaultDevice"));
        if (!device.isValid()) {
            return false;
        }
        QDBusInterface reader(fprint, device.value().path(), QStringLiteral("net.reactivated.Fprint.Device"),
                              QDBusConnection::systemBus());
        reader.setTimeout(3000);
        const QDBusReply<QStringList> fingers = reader.call(QStringLiteral("ListEnrolledFingers"), userName());
        return fingers.isValid() && !fingers.value().isEmpty();
    }

    [[nodiscard]] QString name() const override { return QCoreApplication::translate("QuickUnlock", "fingerprint"); }
    [[nodiscard]] QString touchPrompt() const override
    {
        return QCoreApplication::translate("QuickUnlock", "Touch the fingerprint reader…");
    }
    [[nodiscard]] QString notKeptText() const override
    {
        return QCoreApplication::translate("QuickUnlock", "The key could not be kept in the desktop keyring.");
    }

    void store(const QString& vault, const QByteArray& secret) override
    {
        verify([this, vault, secret](Result result) {
            if (result != Result::Done) {
                emit stored(result);
                return;
            }
            // The keyring may ask for its password: off the window's thread.
            QPointer<LinuxQuickUnlock> self(this);
            (void)QtConcurrent::run([self, vault, secret]() {
                const bool kept = keep(vault, secret);
                QMetaObject::invokeMethod(
                    qApp,
                    [self, vault, kept]() {
                        if (self) {
                            self->remember(vault, kept);
                            emit self->stored(kept ? Result::Done : Result::NotKept);
                        }
                    },
                    Qt::QueuedConnection);
            });
        });
    }

    void retrieve(const QString& vault) override
    {
        fingerLog(QStringLiteral("unlock: finger asked"));
        verify([this, vault](Result result) {
            if (result != Result::Done) {
                fingerLog(QStringLiteral("unlock: no finger (%1)").arg(int(result)));
                emit retrieved(result, {});
                return;
            }
            fingerLog(QStringLiteral("unlock: finger recognized; asking the keyring for the key"));
            QPointer<LinuxQuickUnlock> self(this);
            (void)QtConcurrent::run([self, vault]() {
                const auto [result, secret] = find(vault);
                fingerLog(QStringLiteral("keyring: answered %1, %2 bytes").arg(int(result)).arg(secret.size()));
                QMetaObject::invokeMethod(
                    qApp,
                    [self, result, secret]() {
                        if (self) {
                            if (result == Result::Unavailable) {
                                self->setProblem(
                                    QCoreApplication::translate("QuickUnlock", "The keyring did not give the key back."));
                            }
                            emit self->retrieved(result, secret);
                        }
                    },
                    Qt::QueuedConnection);
            });
        });
    }

    // The keyring needs no finger: only the new key is written.
    void replace(const QString& vault, const QByteArray& secret) override
    {
        QPointer<LinuxQuickUnlock> self(this);
        (void)QtConcurrent::run([self, vault, secret]() {
            const bool kept = keep(vault, secret);
            QMetaObject::invokeMethod(
                qApp,
                [self, vault, kept]() {
                    if (self) {
                        self->remember(vault, kept);
                        emit self->stored(kept ? Result::Done : Result::NotKept);
                    }
                },
                Qt::QueuedConnection);
        });
    }

    void cancel() override
    {
        if (waiting) {
            finish(Result::Cancelled);
        }
    }

    void remove(const QString& vault) override
    {
        remember(vault, false);
        // The keyring may be slow to answer: off the window's thread (OR-5).
        QPointer<LinuxQuickUnlock> self(this);
        (void)QtConcurrent::run([self, vault]() {
            if (!removeFromKeyring(vault)) {
                QMetaObject::invokeMethod(
                    qApp,
                    [self, vault]() {
                        if (self) {
                            emit self->removeFailed(QStringLiteral("Chrono Vault: %1").arg(vault));
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
    }

private:
    // True when no key for the vault is left in the keyring afterwards.
    static bool removeFromKeyring(const QString& vault)
    {
        QDBusInterface service(secrets, secretsPath, QStringLiteral("org.freedesktop.Secret.Service"),
                               QDBusConnection::sessionBus());
        service.setTimeout(30000);
        const auto search = [&service, &vault]() {
            QList<QDBusObjectPath> all;
            QList<QDBusObjectPath> locked;
            const QDBusMessage found =
                service.call(QStringLiteral("SearchItems"), QVariant::fromValue(attributes(vault)));
            if (found.type() == QDBusMessage::ReplyMessage && found.arguments().size() == 2) {
                all = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(0));
                locked = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(1));
            }
            return std::pair{all + locked, locked};
        };
        auto [items, locked] = search();
        if (!locked.isEmpty()) {
            unlockAll(locked); // a locked item cannot be deleted
        }
        for (const QDBusObjectPath& item : std::as_const(items)) {
            QDBusInterface one(secrets, item.path(), QStringLiteral("org.freedesktop.Secret.Item"),
                               QDBusConnection::sessionBus());
            one.call(QStringLiteral("Delete"));
        }
        return search().first.isEmpty();
    }

    static QMap<QString, QString> attributes(const QString& vault)
    {
        return {{QStringLiteral("application"), QStringLiteral("chrono-vault")}, {QStringLiteral("vault"), vault}};
    }

    // A plain session: the secret passes over the session's own bus only.
    static QDBusObjectPath openSession()
    {
        QDBusInterface service(secrets, secretsPath, QStringLiteral("org.freedesktop.Secret.Service"),
                               QDBusConnection::sessionBus());
        const QDBusMessage reply = service.call(QStringLiteral("OpenSession"), QStringLiteral("plain"),
                                                QVariant::fromValue(QDBusVariant(QString())));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2) {
            return {};
        }
        return qvariant_cast<QDBusObjectPath>(reply.arguments().at(1));
    }

    // Unlocks the keyring's items or collection when locked; the desktop may
    // ask for the login password then.
    static void unlockAll(const QList<QDBusObjectPath>& objects)
    {
        QDBusInterface service(secrets, secretsPath, QStringLiteral("org.freedesktop.Secret.Service"),
                               QDBusConnection::sessionBus());
        service.setTimeout(120000);
        const QDBusMessage reply = service.call(QStringLiteral("Unlock"), QVariant::fromValue(objects));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() != 2) {
            return;
        }
        const auto prompt = qvariant_cast<QDBusObjectPath>(reply.arguments().at(1));
        if (prompt.path() != QLatin1String("/")) {
            QDBusInterface asking(secrets, prompt.path(), QStringLiteral("org.freedesktop.Secret.Prompt"),
                                  QDBusConnection::sessionBus());
            asking.setTimeout(120000);
            asking.call(QStringLiteral("Prompt"), QString());
        }
    }

    static bool keep(const QString& vault, const QByteArray& secret)
    {
        const QDBusObjectPath session = openSession();
        if (session.path().isEmpty()) {
            return false;
        }
        unlockAll({QDBusObjectPath(defaultCollection)});
        QDBusInterface collection(secrets, defaultCollection, QStringLiteral("org.freedesktop.Secret.Collection"),
                                  QDBusConnection::sessionBus());
        collection.setTimeout(30000);
        const QVariantMap properties{
            {QStringLiteral("org.freedesktop.Secret.Item.Label"), QStringLiteral("Chrono Vault: %1").arg(vault)},
            {QStringLiteral("org.freedesktop.Secret.Item.Attributes"), QVariant::fromValue(attributes(vault))},
        };
        const SecretValue value{session, {}, secret, QStringLiteral("application/octet-stream")};
        const QDBusMessage reply = collection.call(QStringLiteral("CreateItem"), properties,
                                                   QVariant::fromValue(value), true);
        return reply.type() == QDBusMessage::ReplyMessage;
    }

    // NoKey only when the keyring is open and holds no key for the vault;
    // a keyring not reachable, or its prompt dismissed, is Unavailable, and
    // the setup stays (D-038).
    static std::pair<Result, QByteArray> find(const QString& vault)
    {
        const QDBusObjectPath session = openSession();
        if (session.path().isEmpty()) {
            return {Result::Unavailable, {}};
        }
        QDBusInterface service(secrets, secretsPath, QStringLiteral("org.freedesktop.Secret.Service"),
                               QDBusConnection::sessionBus());
        service.setTimeout(30000);
        const QDBusMessage found = service.call(QStringLiteral("SearchItems"), QVariant::fromValue(attributes(vault)));
        if (found.type() != QDBusMessage::ReplyMessage || found.arguments().size() != 2) {
            return {Result::Unavailable, {}};
        }
        QList<QDBusObjectPath> items = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(0));
        const QList<QDBusObjectPath> locked = qdbus_cast<QList<QDBusObjectPath>>(found.arguments().at(1));
        if (items.isEmpty() && locked.isEmpty()) {
            return {Result::NoKey, {}};
        }
        if (!locked.isEmpty()) {
            unlockAll(locked);
            items += locked;
        }
        const QDBusMessage reply = service.call(QStringLiteral("GetSecrets"), QVariant::fromValue(items),
                                                QVariant::fromValue(session));
        if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
            return {Result::Unavailable, {}};
        }
        const auto values = qdbus_cast<SecretMap>(reply.arguments().at(0));
        if (values.isEmpty() || values.first().value.isEmpty()) {
            return {Result::Unavailable, {}}; // still locked: the prompt was dismissed
        }
        return {Result::Done, values.first().value};
    }

    // fprintd: claim the reader, ask for a finger, wait for its answer; every
    // call sent without waiting on the window's thread (OR-5).
    // Each check has its number: an answer of a check already ended or
    // stopped is not acted on, and a reader it claimed late is given back.
    void verify(std::function<void(Result)> done)
    {
        cancel(); // a check still waiting gives the reader back first
        waiting = std::move(done);
        const quint64 mine = ++check;
        setProblem(QString());
        fingerLog(QStringLiteral("reader: check %1 starts, asking fprintd for the reader").arg(mine));
        const QDBusMessage getDevice = QDBusMessage::createMethodCall(
            fprint, QStringLiteral("/net/reactivated/Fprint/Manager"), QStringLiteral("net.reactivated.Fprint.Manager"),
            QStringLiteral("GetDefaultDevice"));
        then(QDBusConnection::systemBus().asyncCall(getDevice), [this, mine](const QDBusMessage& reply) {
            if (mine != check) {
                return;
            }
            if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                fingerLog(QStringLiteral("reader: no reader: %1 %2").arg(reply.errorName(), reply.errorMessage()));
                setProblem(QCoreApplication::translate("QuickUnlock", "No fingerprint reader was found."));
                finish(Result::Unavailable);
                return;
            }
            devicePath = qvariant_cast<QDBusObjectPath>(reply.arguments().constFirst()).path();
            fingerLog(QStringLiteral("reader: %1, claiming it").arg(devicePath));
            then(QDBusConnection::systemBus().asyncCall(deviceCall(QStringLiteral("Claim"), userName())),
                 [this, mine, path = devicePath](const QDBusMessage& claimed) {
                     if (mine != check) {
                         if (claimed.type() == QDBusMessage::ReplyMessage) {
                             // Stopped while the reader was being claimed.
                             QDBusConnection::systemBus().asyncCall(QDBusMessage::createMethodCall(
                                 fprint, path, QStringLiteral("net.reactivated.Fprint.Device"),
                                 QStringLiteral("Release")));
                         }
                         return;
                     }
                     if (claimed.type() != QDBusMessage::ReplyMessage) {
                         fingerLog(QStringLiteral("reader: claim refused: %1 %2")
                                       .arg(claimed.errorName(), claimed.errorMessage()));
                         setProblem(claimed.errorName().endsWith(QLatin1String("AlreadyInUse"))
                                        ? QCoreApplication::translate("QuickUnlock",
                                                                      "The fingerprint reader is in use by another program.")
                                        : QCoreApplication::translate("QuickUnlock",
                                                                      "The fingerprint reader refused to start."));
                         finish(Result::Unavailable);
                         return;
                     }
                     holding = true;
                     const bool listening = QDBusConnection::systemBus().connect(
                         fprint, devicePath, QStringLiteral("net.reactivated.Fprint.Device"),
                         QStringLiteral("VerifyStatus"), this, SLOT(onVerifyStatus(QString, bool)));
                     fingerLog(QStringLiteral("reader: claimed; listening for its answer: %1, starting the check")
                                   .arg(listening ? QStringLiteral("yes") : QStringLiteral("NO")));
                     then(QDBusConnection::systemBus().asyncCall(deviceCall(QStringLiteral("VerifyStart"),
                                                                            QStringLiteral("any"))),
                          [this, mine](const QDBusMessage& started) {
                              if (mine != check) {
                                  return;
                              }
                              if (started.type() != QDBusMessage::ReplyMessage) {
                                  fingerLog(QStringLiteral("reader: check refused: %1 %2")
                                                .arg(started.errorName(), started.errorMessage()));
                                  setProblem(QCoreApplication::translate("QuickUnlock",
                                                                         "The fingerprint reader refused to start."));
                                  finish(Result::Unavailable);
                                  return;
                              }
                              fingerLog(QStringLiteral("reader: check running, waiting for a finger"));
                              emit fingerReady(); // the person can touch it now
                          });
                 });
        });
    }

    QDBusMessage deviceCall(const QString& method, const QString& argument = {}) const
    {
        QDBusMessage call = QDBusMessage::createMethodCall(fprint, devicePath,
                                                           QStringLiteral("net.reactivated.Fprint.Device"), method);
        if (!argument.isNull()) {
            call << argument;
        }
        return call;
    }

    void then(const QDBusPendingCall& pending, std::function<void(const QDBusMessage&)> next)
    {
        auto* watcher = new QDBusPendingCallWatcher(pending, this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [watcher, next](QDBusPendingCallWatcher*) {
            watcher->deleteLater();
            next(watcher->reply());
        });
    }

    // Gives the reader back, without waiting, and answers the caller.
    void finish(Result result)
    {
        fingerLog(QStringLiteral("reader: check %1 ends with %2; reader %3")
                      .arg(check)
                      .arg(int(result))
                      .arg(holding ? QStringLiteral("given back") : QStringLiteral("was not held")));
        ++check; // later answers of this check are not acted on
        if (std::exchange(holding, false)) {
            QDBusConnection::systemBus().disconnect(fprint, devicePath, QStringLiteral("net.reactivated.Fprint.Device"),
                                                    QStringLiteral("VerifyStatus"), this,
                                                    SLOT(onVerifyStatus(QString, bool)));
            QDBusConnection::systemBus().asyncCall(deviceCall(QStringLiteral("VerifyStop")));
            QDBusConnection::systemBus().asyncCall(deviceCall(QStringLiteral("Release")));
        }
        if (auto done = std::exchange(waiting, nullptr)) {
            done(result);
        }
    }

    // A match settles it; a finger not recognized keeps the reader held and
    // asks again, since the question is still open (the owner, D-042).
    Q_SLOT void onVerifyStatus(const QString& status, bool done)
    {
        fingerLog(QStringLiteral("reader: answered %1 (done: %2)").arg(status, done ? QStringLiteral("yes") : QStringLiteral("no")));
        if (status == QLatin1String("verify-match")) {
            finish(Result::Done);
        } else if (status == QLatin1String("verify-no-match")) {
            emit notRecognized();
            const quint64 mine = check;
            QDBusConnection::systemBus().asyncCall(deviceCall(QStringLiteral("VerifyStop")));
            then(QDBusConnection::systemBus().asyncCall(deviceCall(QStringLiteral("VerifyStart"), QStringLiteral("any"))),
                 [this, mine](const QDBusMessage& started) {
                     if (mine == check && started.type() != QDBusMessage::ReplyMessage) {
                         finish(Result::Unavailable);
                     }
                 });
        } else if (done) {
            setProblem(QCoreApplication::translate("QuickUnlock", "The fingerprint reader reported an error."));
            finish(Result::Unavailable); // disconnected, or the reader's own error
        }
    }

    QString devicePath;
    quint64 check = 0;    // the number of the check now running
    bool holding = false; // the reader is claimed for it
    std::function<void(Result)> waiting;
};

} // namespace

std::unique_ptr<QuickUnlock> makeQuickUnlock()
{
    return std::make_unique<LinuxQuickUnlock>();
}

} // namespace chrono_vault

#include "quick_unlock_linux.moc"
