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

#include "totp.h"

#include <QtTest>

// --- Version ------------------------------------------------------------
// test_totp.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

using namespace chrono_vault;

// The secrets of RFC 6238, appendix B, in base32.
namespace {
const QString sha1Secret = QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
const QString sha256Secret = QStringLiteral("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZA");
const QString sha512Secret = QStringLiteral(
    "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQGEZDGNA");
} // namespace

class TestTotp : public QObject
{
    Q_OBJECT

private slots:
    void the_codes_of_rfc_6238_data();
    void the_codes_of_rfc_6238();
    void a_secret_with_spaces_and_lowercase_is_read();
    void an_otpauth_address_is_read_with_its_settings();
    void what_is_not_a_secret_is_refused();
    void the_seconds_left_count_down_within_the_period();
};

void TestTotp::the_codes_of_rfc_6238_data()
{
    QTest::addColumn<QString>("address");
    QTest::addColumn<qint64>("time");
    QTest::addColumn<QString>("code");
    const QString sha1 = QStringLiteral("otpauth://totp/x?secret=%1&digits=8&algorithm=SHA1").arg(sha1Secret);
    const QString sha256 = QStringLiteral("otpauth://totp/x?secret=%1&digits=8&algorithm=SHA256").arg(sha256Secret);
    const QString sha512 = QStringLiteral("otpauth://totp/x?secret=%1&digits=8&algorithm=SHA512").arg(sha512Secret);
    QTest::newRow("sha1 59") << sha1 << qint64(59) << QStringLiteral("94287082");
    QTest::newRow("sha256 59") << sha256 << qint64(59) << QStringLiteral("46119246");
    QTest::newRow("sha512 59") << sha512 << qint64(59) << QStringLiteral("90693936");
    QTest::newRow("sha1 1111111109") << sha1 << qint64(1111111109) << QStringLiteral("07081804");
    QTest::newRow("sha1 1234567890") << sha1 << qint64(1234567890) << QStringLiteral("89005924");
    QTest::newRow("sha256 2000000000") << sha256 << qint64(2000000000) << QStringLiteral("90698825");
    QTest::newRow("sha512 20000000000") << sha512 << qint64(20000000000) << QStringLiteral("47863826");
}

void TestTotp::the_codes_of_rfc_6238()
{
    QFETCH(QString, address);
    QFETCH(qint64, time);
    QFETCH(QString, code);
    const auto settings = parseTotp(address);
    QVERIFY(settings);
    QCOMPARE(totpCode(*settings, time), code);
}

void TestTotp::a_secret_with_spaces_and_lowercase_is_read()
{
    const auto settings = parseTotp(QStringLiteral(" gezd gnbv gy3t qojq gezd gnbv gy3t qojq "));
    QVERIFY(settings);
    QCOMPARE(settings->digits, 6);
    QCOMPARE(settings->period, 30);
    QCOMPARE(totpCode(*settings, 59), QStringLiteral("287082"));
}

void TestTotp::an_otpauth_address_is_read_with_its_settings()
{
    const auto settings = parseTotp(
        QStringLiteral("otpauth://totp/Example:anna@example.org?secret=JBSWY3DPEHPK3PXP&issuer=Example&period=60&digits=8"));
    QVERIFY(settings);
    QCOMPARE(settings->digits, 8);
    QCOMPARE(settings->period, 60);
    QCOMPARE(settings->algorithm, TotpAlgorithm::Sha1);
}

void TestTotp::what_is_not_a_secret_is_refused()
{
    QVERIFY(!parseTotp(QString()));
    QVERIFY(!parseTotp(QStringLiteral("   ")));
    QVERIFY(!parseTotp(QStringLiteral("not base32 at all! 0189")));
    QVERIFY(!parseTotp(QStringLiteral("otpauth://hotp/x?secret=JBSWY3DPEHPK3PXP")));
    QVERIFY(!parseTotp(QStringLiteral("otpauth://totp/x?issuer=nobody")));
    QVERIFY(!parseTotp(QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&digits=99")));
    QVERIFY(!parseTotp(QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&period=0")));
    QVERIFY(!parseTotp(QStringLiteral("otpauth://totp/x?secret=JBSWY3DPEHPK3PXP&algorithm=MD5")));
    QVERIFY(!parseTotp(QString(5000, QLatin1Char('A'))));
}

void TestTotp::the_seconds_left_count_down_within_the_period()
{
    const TotpSettings settings{QByteArray("12345678901234567890"), 6, 30, TotpAlgorithm::Sha1};
    QCOMPARE(totpSecondsLeft(settings, 0), 30);
    QCOMPARE(totpSecondsLeft(settings, 29), 1);
    QCOMPARE(totpSecondsLeft(settings, 30), 30);
}

QTEST_GUILESS_MAIN(TestTotp)
#include "test_totp.moc"
