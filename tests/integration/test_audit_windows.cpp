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

#include "intuitive_fixture.h"

#include <QCryptographicHash>
#include <QTcpServer>
#include <QTcpSocket>

// --- Version ------------------------------------------------------------
// test_audit_windows.cpp 0.8.121

// --- Version history ----------------------------------------------------
// v0.8.121: A window that fits the test screen, used once it has that size (P2)
// v0.6.86: Initial release (D-065)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

namespace {

// Stands in for Have I Been Pwned: "welkom" leaked 1234 times, and
// shop.example was breached.
class LeakServer : public QTcpServer
{
public:
    LeakServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this]() {
            while (QTcpSocket* socket = nextPendingConnection()) {
                connect(socket, &QTcpSocket::readyRead, socket, [this, socket]() {
                    const QByteArray request = socket->readAll();
                    const QByteArray path = request.split(' ').value(1);
                    asked << path;
                    padded = padded && (path == "/breaches" || request.toLower().contains("add-padding: true"));
                    named = named && request.contains("Chrono-Vault/");
                    QByteArray body;
                    if (path == "/breaches") {
                        body = R"([{"Title":"Shop","Domain":"shop.example","BreachDate":"2026-01-15","IsVerified":true,"DataClasses":["Passwords"]}])";
                    } else {
                        const QByteArray hash = QCryptographicHash::hash("welkom", QCryptographicHash::Sha1).toHex().toUpper();
                        body = QByteArray(35, '0') + ":5\r\n" + QByteArray(35, 'F') + ":0\r\n";
                        if (path.endsWith(hash.left(5))) {
                            body += hash.mid(5) + ":1234\r\n";
                        }
                    }
                    socket->write((broken ? "HTTP/1.1 503 Busy\r\n" : "HTTP/1.1 200 OK\r\n")
                                  + QByteArray("Content-Length: ") + QByteArray::number(body.size())
                                  + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            }
        });
    }
    QList<QByteArray> asked;
    bool padded = true;
    bool named = true;
    bool broken = false;
};

} // namespace

// The audit in the windows, by mouse and keys (D-065).
class TestAuditWindows : public IntuitiveFixture
{
    Q_OBJECT

    LeakServer server;

    QString add(const QString& title, const QString& password, const QString& site = {})
    {
        QVariantMap item = controller->newLogin();
        QVariantList fields = item.value(QStringLiteral("fields")).toList();
        for (QVariant& field : fields) {
            QVariantMap map = field.toMap();
            const QString role = map.value(QStringLiteral("role")).toString();
            map.insert(QStringLiteral("value"), role == QLatin1String("password") ? password
                                                : role == QLatin1String("url")    ? site
                                                                                  : QString());
            field = map;
        }
        item.insert(QStringLiteral("fields"), fields);
        item.insert(QStringLiteral("title"), title);
        controller->saveItem(item);
        return item.value(QStringLiteral("id")).toString();
    }
    int number(const char* list) const { return controller->audit().value(QString::fromLatin1(list)).toInt(); }
    QString state(const char* check) const { return controller->audit().value(QString::fromLatin1(check)).toString(); }
    QString cardText(const char* list)
    {
        QQuickItem* card = part(qPrintable(QStringLiteral("auditCard_") + QString::fromLatin1(list)));
        return card ? card->property("instead").toString() : QStringLiteral("no card");
    }
    void useServer()
    {
        if (!server.isListening()) {
            QVERIFY(server.listen(QHostAddress::LocalHost));
        }
        const QString base = QStringLiteral("http://127.0.0.1:%1/").arg(server.serverPort());
        controller->setAuditAddressesForTests([base](const QByteArray& start) { return QUrl(base + QString::fromLatin1(start)); },
                                              QUrl(base + QStringLiteral("breaches")));
        server.asked.clear();
        server.broken = false;
    }
    const QString strong = QStringLiteral("vK9#mQ2x!Lp7@zRt4&Wb");
    // A window that fits the test screen (800 by 800), worked with only once
    // it has that size: a larger one is cut down by the screen later, and a
    // click meanwhile lands where the list was (P2, 0.8.121).
    void roomy()
    {
        window->resize(780, 760);
        QTRY_COMPARE(window->contentItem()->size(), QSizeF(780, 760));
    }

private slots:
    void the_cards_the_lists_and_the_remarks()
    {
        useServer();
        const QString weak = add(QStringLiteral("Weak one"), QStringLiteral("welkom"));
        const QString first = add(QStringLiteral("Same 1"), strong);
        const QString second = add(QStringLiteral("Same 2"), strong);
        add(QStringLiteral("Fine"), strong + QStringLiteral("x"));
        QTRY_COMPARE(controller->audit().value(QStringLiteral("passwords")).toInt(), 4);
        QCOMPARE(number("weak"), 1);
        QCOMPARE(number("reused"), 2);
        QCOMPARE(controller->audit().value(QStringLiteral("score")).toInt(), 25);
        QCOMPARE(server.asked.size(), 0); // off: nothing is asked
        QCOMPARE(state("leakState"), QStringLiteral("off"));
        roomy();
        QTRY_VERIFY(part("auditBlock"));
        QCOMPARE(part("auditScore")->property("text").toString(), QStringLiteral("25%"));
        QCOMPARE(cardText("leaked"), QStringLiteral("Off"));
        QCOMPARE(cardText("weak"), QString());

        click(part("auditCard_reused"));
        QTRY_COMPARE(controller->filter(), QStringLiteral("audit:reused"));
        QTRY_COMPARE(controller->items()->rowCount(), 2);
        QVERIFY(row(0));
        click(row(0));
        QTRY_VERIFY(part("auditRemarks"));
        const QVariantList remarks =
            controller->auditRemarks(first, controller->item(first).value(QStringLiteral("fields")).toList().at(1).toMap()
                                                .value(QStringLiteral("uid")).toLongLong());
        QCOMPARE(remarks.size(), 1);
        QCOMPARE(remarks.at(0).toMap().value(QStringLiteral("items")).toList().at(0).toMap().value(QStringLiteral("id")).toString(), second);
        // The other one, also from a list that does not hold it.
        QVERIFY(QMetaObject::invokeMethod(page, "showAnywhere", Q_ARG(QVariant, weak)));
        QTRY_COMPARE(page->property("chosenItemId").toString(), weak);
        QCOMPARE(controller->filter(), QString());
        // A list of the audit that is empty says so; the sidebar's group goes.
        click(part("audit_old"));
        QTRY_VERIFY(part("emptyHeadline"));
        QCOMPARE(part("emptyHeadline")->property("text").toString(), QStringLiteral("No old passwords"));
        controller->setShowAudit(false);
        QTRY_COMPARE(controller->filter(), QString());
        QVERIFY(!part("audit_weak"));
        controller->setShowAudit(true);
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }

    void left_out_and_back_again()
    {
        useServer();
        const QString weak = add(QStringLiteral("A weak one"), QStringLiteral("welkom"));
        QTRY_COMPARE(number("weak"), 1);
        QVERIFY(QMetaObject::invokeMethod(page, "choose", Q_ARG(QVariant, QStringLiteral("audit:weak"))));
        QTRY_VERIFY(row(0));
        click(row(0));
        QObject* action = window->findChild<QObject*>(QStringLiteral("auditAction"));
        QCOMPARE(action->property("text").toString(), QStringLiteral("Exclude from Audit"));
        QMetaObject::invokeMethod(action, "trigger");
        QTRY_COMPARE(number("weak"), 0);
        QVERIFY(controller->item(weak).value(QStringLiteral("noAudit")).toBool());
        // Saving it in the editor keeps it left out.
        QVERIFY(controller->saveItem(controller->item(weak)));
        QVERIFY(controller->item(weak).value(QStringLiteral("noAudit")).toBool());
        QVERIFY(QMetaObject::invokeMethod(page, "choose", Q_ARG(QVariant, QString())));
        QVERIFY(QMetaObject::invokeMethod(page, "showItem", Q_ARG(QVariant, weak)));
        QTRY_COMPARE(action->property("text").toString(), QStringLiteral("Include in Audit"));
        QMetaObject::invokeMethod(action, "trigger");
        QTRY_COMPARE(number("weak"), 1);
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }

    void the_online_checks()
    {
        useServer();
        add(QStringLiteral("Leaked"), QStringLiteral("welkom"));
        add(QStringLiteral("Shop"), strong, QStringLiteral("https://www.shop.example/login"));
        QTRY_COMPARE(controller->audit().value(QStringLiteral("passwords")).toInt(), 2);
        // Its password was set just now, after the breach: nothing to say.
        controller->setCheckLeaks(true);
        QTRY_COMPARE(state("leakState"), QStringLiteral("done"));
        QTRY_COMPARE(state("breachState"), QStringLiteral("done"));
        QCOMPARE(number("leaked"), 1);
        QCOMPARE(number("breached"), 0);
        QVERIFY(server.padded && server.named);
        QCOMPARE(server.asked.size(), 3);
        const QByteArray hash = QCryptographicHash::hash("welkom", QCryptographicHash::Sha1).toHex().toUpper();
        for (const QByteArray& path : std::as_const(server.asked)) {
            QVERIFY(path == "/breaches" || path.size() == 6); // five digits and no more
        }
        QVERIFY(server.asked.contains("/" + hash.left(5)));
        // A new password is asked by itself; the others are known.
        add(QStringLiteral("Another"), strong + QStringLiteral("y"));
        QTRY_COMPARE(server.asked.size(), 4);
        QTRY_COMPARE(state("leakState"), QStringLiteral("done"));
        // A lock forgets; the next unlock asks again.
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
        controller->lock();
        QCOMPARE(number("leaked"), 0);
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE(state("leakState"), QStringLiteral("done"));
        QCOMPARE(number("leaked"), 1);
        QCOMPARE(server.asked.size(), 8);
        controller->setCheckLeaks(false);
        QTRY_COMPARE(number("leaked"), 0);
        QCOMPARE(state("leakState"), QStringLiteral("off"));
    }

    void not_reached_is_never_called_safe()
    {
        useServer();
        server.broken = true;
        add(QStringLiteral("Leaked"), QStringLiteral("welkom"));
        QTRY_COMPARE(number("weak"), 1);
        controller->setCheckLeaks(true);
        QTRY_COMPARE(state("leakState"), QStringLiteral("failed"));
        QTRY_COMPARE(state("breachState"), QStringLiteral("failed"));
        roomy();
        QTRY_COMPARE(cardText("leaked"), QStringLiteral("Not checked"));
        QCOMPARE(cardText("breached"), QStringLiteral("Not checked"));
        click(part("audit_leaked"));
        QTRY_VERIFY(part("emptyAuditButton"));
        QVERIFY(part("emptyHeadline")->property("text").toString().startsWith(QStringLiteral("Not checked")));
        server.broken = false;
        click(part("emptyAuditButton"));
        QTRY_COMPARE(number("leaked"), 1);
        QTRY_COMPARE(state("breachState"), QStringLiteral("done"));
        QTRY_COMPARE(controller->items()->rowCount(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }

    void the_setting_is_off_until_it_is_ticked_and_kept_with_ok()
    {
        useServer();
        QVERIFY(!controller->checkLeaks());
        roomy();
        QTRY_VERIFY(part("auditCard_leaked"));
        add(QStringLiteral("One"), strong);
        QTRY_COMPARE(controller->audit().value(QStringLiteral("passwords")).toInt(), 1);
        click(part("auditCard_leaked")); // off: to the setting
        QQuickWindow* settings = dialog("settingsWindow");
        QVERIFY(comesUp(settings));
        QQuickItem* box = seen(settings->contentItem(), QStringLiteral("checkLeaksBox"));
        QVERIFY(box && !box->property("checked").toBool());
        click(box);
        QVERIFY(!controller->checkLeaks());
        QVERIFY(QMetaObject::invokeMethod(settings, "accept"));
        QTRY_VERIFY(controller->checkLeaks());
        QTRY_COMPARE(state("leakState"), QStringLiteral("done"));
        controller->setCheckLeaks(false);
        QTRY_VERIFY_WITH_TIMEOUT(controller->isIdle(), 10000);
    }
};

QTEST_MAIN(TestAuditWindows)
#include "test_audit_windows.moc"
