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
#include "vault_controller.h"

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_website_icons.cpp 0.0.20

// --- Version history ----------------------------------------------------
// v0.0.20: Initial release (D-036)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

// Stands in for the websites: answers /<host> with a red picture, or with
// "not found" for the host named missing.example.
class IconServer : public QTcpServer
{
public:
    IconServer()
    {
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    const QByteArray request = socket->readAll();
                    asked << QString::fromLatin1(request.split(' ').value(1));
                    const bool missing = request.contains("missing.example");
                    const QByteArray body = missing ? QByteArray() : png;
                    socket->write((missing ? "HTTP/1.1 404 Not Found\r\n" : "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n") +
                                  QByteArray("Content-Length: ") + QByteArray::number(body.size()) +
                                  "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QByteArray png;
    QStringList asked;
};

} // namespace

class TestWebsiteIcons : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void a_website_icon_is_shown_while_the_setting_is_on_and_forgotten_at_the_lock();
    void off_by_default_and_nothing_asked();
};

void TestWebsiteIcons::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_website_icons"));
    QSettings().clear();
}

void TestWebsiteIcons::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestWebsiteIcons::a_website_icon_is_shown_while_the_setting_is_on_and_forgotten_at_the_lock()
{
    QSettings().clear();
    IconServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir folder;
    VaultController controller;
    controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller.setIconAddressForTests([&server](const QString& host) {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(server.serverPort()).arg(host));
    });
    controller.createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("icons.chronovault"))),
                           QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller.state(), State::Unlocked);

    const auto withAddress = [&controller](const QString& title, const QString& address) {
        QVariantMap item = controller.newLogin();
        QVariantList fields = item.value(QStringLiteral("fields")).toList();
        for (QVariant& field : fields) {
            QVariantMap map = field.toMap();
            if (map.value(QStringLiteral("role")).toString() == QLatin1String("url")) {
                map.insert(QStringLiteral("value"), address);
                field = map;
            }
        }
        item.insert(QStringLiteral("fields"), fields);
        item.insert(QStringLiteral("title"), title);
        controller.saveItem(item);
        return item.value(QStringLiteral("id")).toString();
    };
    const QString shop = withAddress(QStringLiteral("Shop"), QStringLiteral("https://shop.example/login"));
    withAddress(QStringLiteral("Shop again"), QStringLiteral("https://SHOP.example/other"));
    const QString gone = withAddress(QStringLiteral("Gone"), QStringLiteral("https://missing.example/"));
    QTRY_VERIFY(controller.isIdle());
    controller.moveToTrash(withAddress(QStringLiteral("Binned"), QStringLiteral("https://binned.example/")), true);
    QTRY_VERIFY(controller.isIdle());

    controller.setUseWebsiteIcons(true);
    QTRY_VERIFY(controller.item(shop).value(QStringLiteral("defaultIcon")).toString().startsWith(QLatin1String("data:image/png")));
    QTRY_COMPARE(server.asked.size(), 2); // each website once, however many items; not the Trash's
    QVERIFY(!server.asked.join(QLatin1Char(' ')).contains(QLatin1String("binned")));
    QVERIFY(controller.item(gone).value(QStringLiteral("defaultIcon")).toString().startsWith(QLatin1String("qrc:")));

    controller.lock();
    controller.unlock(QStringLiteral("a"));
    QTRY_COMPARE(controller.state(), State::Unlocked);
    // Forgotten at the lock, asked again after it.
    QTRY_COMPARE(server.asked.size(), 4);
    QTRY_VERIFY(controller.item(shop).value(QStringLiteral("defaultIcon")).toString().startsWith(QLatin1String("data:image/png")));

    controller.setUseWebsiteIcons(false);
    QVERIFY(controller.item(shop).value(QStringLiteral("defaultIcon")).toString().startsWith(QLatin1String("qrc:")));
    controller.lock();
}

void TestWebsiteIcons::off_by_default_and_nothing_asked()
{
    QSettings().clear();
    IconServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    QTemporaryDir folder;
    VaultController controller;
    QVERIFY(!controller.useWebsiteIcons());
    QVERIFY(!controller.settingsDefaults().value(QStringLiteral("useWebsiteIcons")).toBool());
    controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller.setIconAddressForTests([&server](const QString& host) {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/%2").arg(server.serverPort()).arg(host));
    });
    controller.createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("off.chronovault"))),
                           QStringLiteral("a"), QStringLiteral("a"));
    QTRY_COMPARE(controller.state(), State::Unlocked);
    QVariantMap item = controller.newLogin();
    item.insert(QStringLiteral("website"), QStringLiteral("example.org"));
    QVERIFY(controller.saveItem(item));
    QTRY_VERIFY(controller.isIdle());
    QTest::qWait(200); // a request would have reached the local server by now
    QVERIFY(server.asked.isEmpty());
    controller.lock();
}

QTEST_MAIN(TestWebsiteIcons)
#include "test_website_icons.moc"
