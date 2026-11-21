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

#include "single_instance.h"

#include <QCryptographicHash>
#include <QLocalSocket>
#include <QStandardPaths>

// --- Version ------------------------------------------------------------
// single_instance.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

namespace chrono_vault {

namespace {

const QByteArray showMessage = QByteArrayLiteral("show\n");
constexpr int waitMilliseconds = 1000;

} // namespace

SingleInstance::SingleInstance(const QString& serverName, QObject* parent)
    : QObject(parent), name(serverName), server(new QLocalServer(this))
{
    connect(server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket* socket = server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
                if (socket->readAll().contains("show")) {
                    emit showRequested();
                }
                // Hanging up tells the later start the message arrived.
                socket->disconnectFromServer();
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
}

QString SingleInstance::defaultName()
{
    // The person's own: built from their home folder, so two people on one
    // computer each have their own Chrono Vault.
    const QByteArray home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation).toUtf8();
    return QStringLiteral("chrono-vault-") +
           QString::fromLatin1(QCryptographicHash::hash(home, QCryptographicHash::Sha256).toHex().left(16));
}

bool SingleInstance::becomeTheOnlyOne()
{
    QLocalSocket other;
    other.connectToServer(name);
    if (other.waitForConnected(waitMilliseconds)) {
        other.write(showMessage);
        other.waitForBytesWritten(waitMilliseconds);
        // The running one hangs up once it has read the message; only then
        // does this start end, so the message is never lost on the way.
        if (other.state() == QLocalSocket::ConnectedState) {
            other.waitForDisconnected(waitMilliseconds);
        }
        return false;
    }
    // Nobody answered: a name left behind by a Chrono Vault that crashed is
    // cleared, so it never blocks a start.
    QLocalServer::removeServer(name);
    server->setSocketOptions(QLocalServer::UserAccessOption);
    // If listening fails all the same, this start still goes on: a second
    // window is better than no Chrono Vault at all.
    server->listen(name);
    return true;
}

} // namespace chrono_vault
