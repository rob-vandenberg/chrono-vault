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


#include "webdav_client.h"
#include "webdav_test_server.h"

#include <QRandomGenerator>
#include <QTcpServer>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_webdav_client.cpp 0.3.60

// --- Version history ----------------------------------------------------
// v0.3.60: Initial release (D-057)

using namespace chrono_vault;

namespace {

WebDavAccount accountOf(const WebDavTestServer& server, const QString& password = QStringLiteral("secret"))
{
    return WebDavAccount{server.folder(), server.user, password, {}};
}

// Waits for one answer of the client, at most 10 seconds.
template <typename Call>
WebDavReply waitFor(Call call)
{
    std::optional<WebDavReply> got;
    call([&got](const WebDavReply& answer) { got = answer; });
    (void)QTest::qWaitFor([&got]() { return got.has_value(); }, 10000);
    WebDavReply none;
    none.error = WebDavError::Server; // no answer in time fails every check
    return got.value_or(none);
}

} // namespace

class TestWebDavClient : public QObject
{
    Q_OBJECT

private slots:
    void listsTheFolderAfterSigningIn()
    {
        WebDavTestServer server;
        server.setFile(QStringLiteral("Home.chronovault"), QByteArrayLiteral("one"));
        server.setFile(QStringLiteral("Work vault.chronovault"), QByteArrayLiteral("two"));
        WebDavClient client(accountOf(server));
        std::optional<QList<WebDavEntry>> listed;
        WebDavReply answer;
        client.list([&](const WebDavReply& reply, const QList<WebDavEntry>& entries) {
            answer = reply;
            listed = entries;
        });
        QTRY_VERIFY_WITH_TIMEOUT(listed.has_value(), 10000);
        QVERIFY(answer.ok());
        QStringList names;
        for (const WebDavEntry& entry : *listed) {
            names << entry.name;
        }
        names.sort();
        QCOMPARE(names, (QStringList{QStringLiteral("Home.chronovault"), QStringLiteral("Work vault.chronovault")}));
    }

    void aWrongPasswordIsSaidSo()
    {
        WebDavTestServer server;
        WebDavClient client(accountOf(server, QStringLiteral("wrong")));
        const WebDavReply answer = waitFor([&](auto done) { client.get(QStringLiteral("x"), {}, done); });
        QCOMPARE(answer.error, WebDavError::Unauthorized);
    }

    void aRefusalIsNotAWrongPassword()
    {
        WebDavTestServer server;
        server.refuseWith = 403; // a read-only share, a full account
        WebDavClient client(accountOf(server));
        QCOMPARE(waitFor([&](auto done) { client.put(QStringLiteral("x"), QByteArrayLiteral("data"), done); }).error,
                 WebDavError::Server);
        // A redirect to a less safe address is not followed, and is no answer.
        server.refuseWith = 301;
        const WebDavReply moved = waitFor([&](auto done) { client.get(QStringLiteral("x"), {}, done); });
        QCOMPARE(moved.error, WebDavError::Server);
        QCOMPARE(moved.status, 301);
    }

    void readsWritesAndTagsAFile()
    {
        WebDavTestServer server;
        WebDavClient client(accountOf(server));
        const QString name = QStringLiteral("Home.chronovault");
        QCOMPARE(waitFor([&](auto done) { client.get(name, {}, done); }).error, WebDavError::NotFound);
        QVERIFY(waitFor([&](auto done) { client.put(name, QByteArrayLiteral("vault bytes"), done); }).ok());
        const WebDavReply read = waitFor([&](auto done) { client.get(name, {}, done); });
        QVERIFY(read.ok());
        QCOMPARE(read.body, QByteArrayLiteral("vault bytes"));
        QVERIFY(!read.tag.isEmpty());
        // The same tag from a PROPFIND as from the GET, however quoted.
        const WebDavReply tagged = waitFor([&](auto done) { client.tagOf(name, done); });
        QCOMPARE(tagged.tag, read.tag);
        QCOMPARE(tagged.size, qint64(11));
        // Not changed since that tag: no body comes again.
        const WebDavReply again = waitFor([&](auto done) { client.get(name, read.tag, done); });
        QCOMPARE(again.status, 304);
        QVERIFY(again.body.isEmpty());
        QVERIFY(waitFor([&](auto done) { client.remove(name, done); }).ok());
        QVERIFY(!server.files.contains(name));
    }

    void aLockKeepsOthersFromMovingOverAFile()
    {
        WebDavTestServer server;
        server.setFile(QStringLiteral("Home.chronovault"), QByteArrayLiteral("old"));
        server.setFile(QStringLiteral("new"), QByteArrayLiteral("new"));
        WebDavClient client(accountOf(server));
        const WebDavReply locked = waitFor([&](auto done) { client.lock(QStringLiteral("Home.chronovault"), done); });
        QVERIFY(locked.ok());
        QVERIFY(!locked.body.isEmpty());
        QCOMPARE(waitFor([&](auto done) { client.lock(QStringLiteral("Home.chronovault"), done); }).error,
                 WebDavError::Locked);
        QCOMPARE(waitFor([&](auto done) {
                     client.move(QStringLiteral("new"), QStringLiteral("Home.chronovault"), {}, done);
                 }).error,
                 WebDavError::Locked);
        QVERIFY(waitFor([&](auto done) {
                    client.move(QStringLiteral("new"), QStringLiteral("Home.chronovault"), locked.body, done);
                }).ok());
        QCOMPARE(server.fileData(QStringLiteral("Home.chronovault")), QByteArrayLiteral("new"));
        QVERIFY(waitFor([&](auto done) {
                    client.unlock(QStringLiteral("Home.chronovault"), locked.body, done);
                }).ok());
    }

    void aServerWithoutLocksSaysSo()
    {
        WebDavTestServer server;
        server.locksOffered = false;
        WebDavClient client(accountOf(server));
        QCOMPARE(waitFor([&](auto done) { client.lock(QStringLiteral("Home.chronovault"), done); }).error,
                 WebDavError::NotSupported);
    }

    void anUntrustedCertificateIsAskedAboutWithItsFingerprint()
    {
        WebDavTestServer server(true);
        WebDavClient untrusted(accountOf(server));
        const WebDavReply refused = waitFor([&](auto done) { untrusted.get(QStringLiteral("x"), {}, done); });
        QCOMPARE(refused.error, WebDavError::Certificate);
        QCOMPARE(refused.certificate, WebDavTestServer::certificateFingerprint());

        WebDavAccount trusting = accountOf(server);
        trusting.trustedCertificate = WebDavTestServer::certificateFingerprint();
        WebDavClient trusted(trusting);
        QCOMPARE(waitFor([&](auto done) { trusted.get(QStringLiteral("x"), {}, done); }).error,
                 WebDavError::NotFound);

        // Another certificate than the one trusted is asked about again.
        trusting.trustedCertificate = QByteArray(64, 'a');
        WebDavClient changed(trusting);
        QCOMPARE(waitFor([&](auto done) { changed.get(QStringLiteral("x"), {}, done); }).error,
                 WebDavError::Certificate);
    }

    void noServerIsNoConnection()
    {
        QTcpServer closed;
        QVERIFY(closed.listen(QHostAddress::LocalHost, 0));
        const quint16 port = closed.serverPort();
        closed.close();
        WebDavClient client(WebDavAccount{QUrl(QStringLiteral("http://127.0.0.1:%1/dav/").arg(port)), {}, {}, {}});
        QCOMPARE(waitFor([&](auto done) { client.get(QStringLiteral("x"), {}, done); }).error,
                 WebDavError::NoConnection);
    }

    void aFolderListIsReadWhateverComes()
    {
        const QUrl folder(QStringLiteral("https://example.org/dav/"));
        QVERIFY(!readFolderList(QByteArrayLiteral("not xml at all"), folder).has_value());
        QVERIFY(!readFolderList(QByteArrayLiteral("<a><b>"), folder).has_value());
        const QByteArray good =
            "<d:multistatus xmlns:d=\"DAV:\"><d:response><d:href>/dav/</d:href></d:response>"
            "<d:response><d:href>/dav/My%20vault.chronovault</d:href><d:propstat><d:prop>"
            "<d:getetag>W/\"abc\"</d:getetag></d:prop></d:propstat></d:response></d:multistatus>";
        const auto entries = readFolderList(good, folder);
        QVERIFY(entries.has_value());
        QCOMPARE(entries->size(), 1);
        QCOMPARE(entries->constFirst().name, QStringLiteral("My vault.chronovault"));
        QCOMPARE(entries->constFirst().tag, QByteArrayLiteral("abc"));
        // Random damage never crashes (OR-4).
        QRandomGenerator random(57);
        for (int round = 0; round < 500; ++round) {
            QByteArray damaged = good;
            for (int change = 0; change < 5; ++change) {
                damaged[random.bounded(damaged.size())] = static_cast<char>(random.bounded(256));
            }
            (void)readFolderList(damaged, folder);
        }
    }
};

QTEST_MAIN(TestWebDavClient)
#include "test_webdav_client.moc"
