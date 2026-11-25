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

#include "audit.h"
#include "password_strength.h"
#include "vault_merge.h"

#include <QCryptographicHash>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_audit.cpp 0.6.86

// --- Version history ----------------------------------------------------
// v0.6.86: Initial release (D-065)

using namespace chrono_vault;

namespace {

QDateTime day(int year, int month, int dayOfMonth)
{
    return QDateTime(QDate(year, month, dayOfMonth), QTime(12, 0), QTimeZone::UTC);
}

Item login(const QString& title, const QString& password, const QDateTime& changed = day(2026, 1, 1), const QString& site = {})
{
    Item item;
    item.id = QUuid::createUuid();
    item.title = title;
    Field field;
    field.role = QStringLiteral("password");
    field.kind = FieldKind::Password;
    field.value = password;
    field.uid = 7;
    field.changed = changed;
    item.fields.append(field);
    item.website = site;
    return item;
}

const QDateTime now = day(2026, 11, 25);
const QString strong = QStringLiteral("vK9#mQ2x!Lp7@zRt4&Wb");

} // namespace

// What the audit finds, and what it reads of Have I Been Pwned (D-065).
class TestAudit : public QObject
{
    Q_OBJECT

private slots:
    void strength_data()
    {
        QTest::addColumn<QString>("password");
        QTest::addColumn<bool>("weak");
        QTest::newRow("empty") << QString() << true;
        QTest::newRow("common") << QStringLiteral("password") << true;
        QTest::newRow("common dressed up") << QStringLiteral("P@ssw0rd2024!") << true;
        QTest::newRow("word and year") << QStringLiteral("Zomer2025!") << true;
        QTest::newRow("keyboard") << QStringLiteral("qwertyuiopasdfghjkl") << true;
        QTest::newRow("sequence") << QStringLiteral("abcdefghijklmnop1234567890") << true;
        QTest::newRow("one character") << QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaa") << true;
        QTest::newRow("a block repeated") << QStringLiteral("x7#qx7#qx7#qx7#qx7#q") << true;
        QTest::newRow("the title") << QStringLiteral("Bluesky123456") << true;
        QTest::newRow("a date") << QStringLiteral("25-11-2026") << true;
        QTest::newRow("four digits") << QStringLiteral("8317") << true;
        QTest::newRow("a short random one") << QStringLiteral("k9#T") << true;
        QTest::newRow("twenty random") << strong << false;
        QTest::newRow("twelve random") << QStringLiteral("h7Rq#2vXp9Lz") << false;
        QTest::newRow("five words") << QStringLiteral("correct horse battery staple lantern") << false;
        QTest::newRow("random letters") << QStringLiteral("qzvtkwhrpnxdmglsbfcj") << false;
        QTest::newRow("other letters") << QStringLiteral("ΩλжЖ漢字ъщ") << false;
    }
    void strength()
    {
        QFETCH(QString, password);
        QFETCH(bool, weak);
        const double bits = passwordBits(password, {QStringLiteral("Bluesky"), QStringLiteral("https://bsky.app")});
        QVERIFY2((bits < weakBelowBits) == weak, qPrintable(QString::number(bits)));
    }

    void a_very_long_password_is_rated_at_once()
    {
        QElapsedTimer timer;
        timer.start();
        QVERIFY(passwordBits(QString(200000, QLatin1Char('a')) + strong) >= 0);
        QVERIFY(timer.elapsed() < 2000);
    }

    void the_lists_hold_what_they_say()
    {
        VaultContents contents;
        contents.items = {login("A", strong), login("B", strong), login("C", QStringLiteral("welkom")),
                          login("D", QStringLiteral("vK9#mQ2x!Lp7@zRt4&Wc"), day(2024, 11, 24)),
                          login("E", QStringLiteral("vK9#mQ2x!Lp7@zRt4&Wd"), QDateTime()),
                          login("F", QStringLiteral("vK9#mQ2x!Lp7@zRt4&We"), day(2024, 11, 26)),
                          login("Trash", QStringLiteral("welkom")), login("Archive", QStringLiteral("welkom")),
                          login("Out", QStringLiteral("welkom")), login("Empty", QString())};
        contents.items[6].trashed = true;
        contents.items[7].archived = true;
        contents.items[8].noAudit = true;
        const auto found = auditPasswords(contents, {}, {}, now);
        QCOMPARE(found.size(), 6);
        QCOMPARE(found.at(0).sameIn, QList<QUuid>{contents.items.at(1).id});
        QCOMPARE(found.at(0).group, 1);
        QCOMPARE(found.at(1).group, 1);
        QCOMPARE(found.at(2).group, 0); // the same word only where it does not count
        QVERIFY(found.at(2).weak && !found.at(0).weak);
        QVERIFY(found.at(3).old);
        QVERIFY(!found.at(4).old && !found.at(5).old); // not known; not yet
        QCOMPARE(found.at(0).leaked, -1);
        QCOMPARE(found.at(2).sha1, QCryptographicHash::hash("welkom", QCryptographicHash::Sha1).toHex().toUpper());
        QCOMPARE(auditPasswords(contents, {{found.at(2).sha1, 12}}, {}, now).at(2).leaked, 12);
    }

    void breached_websites()
    {
        VaultContents contents;
        contents.items = {login("Before", strong, day(2025, 1, 1), QStringLiteral("https://www.shop.example/a")),
                          login("After", strong, day(2026, 2, 1), QStringLiteral("shop.example")),
                          login("Unknown", strong, QDateTime(), QStringLiteral("shop.example")),
                          login("Other", strong, day(2025, 1, 1), QStringLiteral("https://myshop.example"))};
        const QByteArray answer = R"([{"Title":"Shop","Domain":"Shop.example","BreachDate":"2026-01-15","IsVerified":true,"DataClasses":["Email addresses","Passwords"]},
            {"Title":"Unverified","Domain":"shop.example","BreachDate":"2026-03-01","IsVerified":false,"DataClasses":["Passwords"]},
            {"Title":"No passwords","Domain":"shop.example","BreachDate":"2026-03-01","IsVerified":true,"DataClasses":["Names"]},
            {"Title":"No site","Domain":"","BreachDate":"2026-03-01","IsVerified":true,"DataClasses":["Passwords"]}, 7])";
        const auto breaches = readBreaches(answer);
        QVERIFY(breaches);
        QCOMPARE(breaches->size(), 1);
        const auto found = auditPasswords(contents, {}, *breaches, now);
        QCOMPARE(found.at(0).breaches.size(), 1);
        QCOMPARE(found.at(0).breaches.at(0).title, QStringLiteral("Shop"));
        QCOMPARE(found.at(1).breaches.size(), 0);
        QCOMPARE(found.at(2).breaches.size(), 1);
        QCOMPARE(found.at(3).breaches.size(), 0);
        QVERIFY(!readBreaches("<html>Sign in</html>"));
        QVERIFY(!readBreaches(""));
        QVERIFY(!readBreaches("{}"));
    }

    void a_leak_answer_is_read_and_its_padding_passed_over()
    {
        const QByteArray one(35, 'A');
        const QByteArray two(35, 'B');
        const auto found = readLeakAnswer(one + ":3\r\n" + two + ":0\r\nnonsense\r\n" + QByteArray(34, 'C') + ":9\r\n"
                                          + QByteArray(35, 'D') + ":x");
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.value(one), 3);
        QVERIFY(readLeakAnswer("<html></html>").isEmpty());
    }

    void left_out_is_kept_and_merged()
    {
        VaultContents contents;
        contents.items = {login("A", strong)};
        contents.items[0].noAudit = true;
        const auto read = contentsFromJson(contentsToJson(contents));
        QCOMPARE(read.status, ContentsStatus::Read);
        QVERIFY(read.contents.items.at(0).noAudit);
        VaultContents base = contents;
        base.items[0].noAudit = false;
        VaultContents mine = base;
        mine.items[0].favourite = true;
        const auto merged = mergeContents(&base, mine, contents, now, QStringLiteral("Here"));
        QVERIFY(merged.contents.items.at(0).noAudit);
        QVERIFY(merged.contents.items.at(0).favourite);
    }
};

QTEST_APPLESS_MAIN(TestAudit)
#include "test_audit.moc"
