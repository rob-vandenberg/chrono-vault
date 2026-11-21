// SPDX-License-Identifier: AGPL-3.0-only
//
// Chrono Vault, a password manager.
// Copyright (C) 2026 Rob Vandenberg
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

#include "release_check.h"

#include <QtTest>

// --- Version ------------------------------------------------------------
// test_release_check.cpp 0.0.5

// --- Version history ----------------------------------------------------
// v0.0.5: Initial release

using namespace chrono_vault;

namespace {

QByteArray asset(const QString& name, char digit = 'a')
{
    return QString::fromUtf8(R"({"name":"%1","browser_download_url":"https://github.com/x/%1","size":1000,"digest":"sha256:%2"})")
        .arg(name, QString(64, QLatin1Char(digit)))
        .toUtf8();
}

QByteArray release(const QString& tag, const QList<QByteArray>& assets)
{
    QByteArray list;
    for (const QByteArray& one : assets) {
        list += (list.isEmpty() ? "" : ",") + one;
    }
    return QString::fromUtf8(R"({"tag_name":"%1","html_url":"https://github.com/x/releases/tag/%1","assets":[)").arg(tag).toUtf8() +
           list + "]}";
}

QByteArray releases(const QList<QByteArray>& items)
{
    QByteArray list;
    for (const QByteArray& one : items) {
        list += (list.isEmpty() ? "" : ",") + one;
    }
    return "[" + list + "]";
}

} // namespace

class TestReleaseCheck : public QObject
{
    Q_OBJECT

private slots:
    void versions_are_three_numbers();
    void versions_compare_number_by_number();
    void the_newest_release_with_this_computers_installer_is_offered();
    void a_release_without_this_computers_installer_is_passed_over();
    void a_skipped_version_is_not_offered_again();
    void nothing_newer_offers_nothing();
    void files_without_a_fingerprint_and_bad_answers_are_left_out();
    void a_check_is_due_by_the_chosen_time();
};

void TestReleaseCheck::versions_are_three_numbers()
{
    QCOMPARE(parseVersion(QStringLiteral("0.0.5")), std::optional<Version>(Version{0, 0, 5}));
    QCOMPARE(parseVersion(QStringLiteral("v1.2.30")), std::optional<Version>(Version{1, 2, 30}));
    QVERIFY(!parseVersion(QStringLiteral("1.2")));
    QVERIFY(!parseVersion(QStringLiteral("1.2.x")));
    QVERIFY(!parseVersion(QStringLiteral("1.-2.3")));
    QVERIFY(!parseVersion(QString()));
    QCOMPARE(versionText(Version{4, 2, 460}), QStringLiteral("4.2.460"));
}

void TestReleaseCheck::versions_compare_number_by_number()
{
    QVERIFY(Version({0, 0, 10}) > Version({0, 0, 9}));
    QVERIFY(Version({0, 1, 0}) > Version({0, 0, 99}));
}

void TestReleaseCheck::the_newest_release_with_this_computers_installer_is_offered()
{
    const QList<Release> list = parseReleases(releases({
        release(QStringLiteral("0.0.6"), {asset(QStringLiteral("chrono-vault_0.0.6_amd64.deb")),
                                         asset(QStringLiteral("chrono-vault_0.0.6_x86_64.rpm")),
                                         asset(QStringLiteral("chrono-vault-0.0.6-setup.exe"))}),
        release(QStringLiteral("0.0.5"), {asset(QStringLiteral("chrono-vault_0.0.5_amd64.deb"))}),
    }));
    QCOMPARE(list.size(), 2);
    const auto deb = newestUpdate(list, InstallKind::Deb, {0, 0, 4}, {});
    QVERIFY(deb);
    QCOMPARE(deb->file.name, QStringLiteral("chrono-vault_0.0.6_amd64.deb"));
    QCOMPARE(newestUpdate(list, InstallKind::Rpm, {0, 0, 4}, {})->file.name, QStringLiteral("chrono-vault_0.0.6_x86_64.rpm"));
    QCOMPARE(newestUpdate(list, InstallKind::Windows, {0, 0, 4}, {})->file.name, QStringLiteral("chrono-vault-0.0.6-setup.exe"));
    QVERIFY(!newestUpdate(list, InstallKind::None, {0, 0, 4}, {}));
}

void TestReleaseCheck::a_release_without_this_computers_installer_is_passed_over()
{
    const QList<Release> list = parseReleases(releases({
        release(QStringLiteral("0.0.7"), {asset(QStringLiteral("chrono-vault_0.0.7_amd64.deb"))}),
        release(QStringLiteral("0.0.6"), {asset(QStringLiteral("chrono-vault_0.0.6_x86_64.rpm"))}),
    }));
    QCOMPARE(newestUpdate(list, InstallKind::Rpm, {0, 0, 5}, {})->release.version, Version({0, 0, 6}));
    QVERIFY(!newestUpdate(list, InstallKind::Windows, {0, 0, 5}, {}));
}

void TestReleaseCheck::a_skipped_version_is_not_offered_again()
{
    const QList<Release> list = parseReleases(releases({
        release(QStringLiteral("0.0.7"), {asset(QStringLiteral("chrono-vault_0.0.7_amd64.deb"))}),
        release(QStringLiteral("0.0.6"), {asset(QStringLiteral("chrono-vault_0.0.6_amd64.deb"))}),
    }));
    QCOMPARE(newestUpdate(list, InstallKind::Deb, {0, 0, 5}, {0, 0, 7})->release.version, Version({0, 0, 6}));
}

void TestReleaseCheck::nothing_newer_offers_nothing()
{
    const QList<Release> list = parseReleases(releases({
        release(QStringLiteral("0.0.5"), {asset(QStringLiteral("chrono-vault_0.0.5_amd64.deb"))}),
    }));
    QVERIFY(!newestUpdate(list, InstallKind::Deb, {0, 0, 5}, {}));
    QVERIFY(!newestUpdate(list, InstallKind::Deb, {0, 1, 0}, {}));
}

void TestReleaseCheck::files_without_a_fingerprint_and_bad_answers_are_left_out()
{
    const QByteArray noDigest =
        R"({"name":"chrono-vault_0.0.9_amd64.deb","browser_download_url":"https://github.com/x/a","size":10})";
    const QByteArray plainAddress =
        R"({"name":"chrono-vault_0.0.9_amd64.deb","browser_download_url":"http://github.com/x/a","size":10,"digest":"sha256:)" +
        QByteArray(64, 'b') + "\"}";
    const QList<Release> list = parseReleases(releases({release(QStringLiteral("0.0.9"), {noDigest, plainAddress})}));
    QCOMPARE(list.size(), 1);
    QVERIFY(list.constFirst().files.isEmpty());
    QVERIFY(parseReleases("not json").isEmpty());
    QVERIFY(parseReleases(R"({"message":"rate limited"})").isEmpty());
    QVERIFY(parseReleases(R"([{"tag_name":"latest"},5,"x"])").isEmpty());
    QVERIFY(parseReleases(R"([{"tag_name":"0.0.9","draft":true,"assets":[]}])").isEmpty());
}

void TestReleaseCheck::a_check_is_due_by_the_chosen_time()
{
    const QDateTime now(QDate(2026, 11, 21), QTime(12, 0), QTimeZone::UTC);
    QVERIFY(isCheckDue(CheckFrequency::Daily, QDateTime(), now));
    QVERIFY(!isCheckDue(CheckFrequency::Daily, now.addSecs(-3600), now));
    QVERIFY(isCheckDue(CheckFrequency::Daily, now.addDays(-1), now));
    QVERIFY(!isCheckDue(CheckFrequency::Weekly, now.addDays(-6), now));
    QVERIFY(isCheckDue(CheckFrequency::Weekly, now.addDays(-7), now));
    QVERIFY(!isCheckDue(CheckFrequency::Monthly, now.addDays(-20), now));
    QVERIFY(isCheckDue(CheckFrequency::Monthly, now.addMonths(-1), now));
    QVERIFY(!isCheckDue(CheckFrequency::Never, QDateTime(), now));
    QVERIFY(isCheckDue(CheckFrequency::Daily, now.addDays(3), now)); // a clock set back
}

QTEST_GUILESS_MAIN(TestReleaseCheck)
#include "test_release_check.moc"
