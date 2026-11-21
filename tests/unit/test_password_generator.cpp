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

#include "password_generator.h"

#include <QSet>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_password_generator.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

using namespace chrono_vault;

class TestPasswordGenerator : public QObject
{
    Q_OBJECT

private slots:
    void a_password_has_the_asked_length_and_every_chosen_kind();
    void only_the_chosen_kinds_are_used();
    void a_length_outside_the_bounds_is_brought_within_them();
    void no_kind_chosen_gives_lowercase_letters();
    void two_passwords_are_not_the_same();
};

void TestPasswordGenerator::a_password_has_the_asked_length_and_every_chosen_kind()
{
    for (int round = 0; round < 50; ++round) {
        const QString password = generatePassword(GeneratorOptions{});
        QCOMPARE(password.size(), 20);
        QVERIFY(std::any_of(password.cbegin(), password.cend(), [](QChar c) { return c.isUpper(); }));
        QVERIFY(std::any_of(password.cbegin(), password.cend(), [](QChar c) { return c.isLower(); }));
        QVERIFY(std::any_of(password.cbegin(), password.cend(), [](QChar c) { return c.isDigit(); }));
        QVERIFY(std::any_of(password.cbegin(), password.cend(), [](QChar c) { return !c.isLetterOrNumber(); }));
    }
}

void TestPasswordGenerator::only_the_chosen_kinds_are_used()
{
    const QString password = generatePassword(GeneratorOptions{64, false, false, true, false});
    QCOMPARE(password.size(), 64);
    QVERIFY(std::all_of(password.cbegin(), password.cend(), [](QChar c) { return c.isDigit(); }));
}

void TestPasswordGenerator::a_length_outside_the_bounds_is_brought_within_them()
{
    QCOMPARE(generatePassword(GeneratorOptions{1, true, true, true, true}).size(), shortestGeneratedPassword);
    QCOMPARE(generatePassword(GeneratorOptions{-5, true, true, true, true}).size(), shortestGeneratedPassword);
    QCOMPARE(generatePassword(GeneratorOptions{100000, true, true, true, true}).size(), longestGeneratedPassword);
}

void TestPasswordGenerator::no_kind_chosen_gives_lowercase_letters()
{
    const QString password = generatePassword(GeneratorOptions{16, false, false, false, false});
    QCOMPARE(password.size(), 16);
    QVERIFY(std::all_of(password.cbegin(), password.cend(), [](QChar c) { return c.isLower(); }));
}

void TestPasswordGenerator::two_passwords_are_not_the_same()
{
    QSet<QString> seen;
    for (int round = 0; round < 100; ++round) {
        seen.insert(generatePassword(GeneratorOptions{}));
    }
    QCOMPARE(seen.size(), 100);
}

QTEST_GUILESS_MAIN(TestPasswordGenerator)
#include "test_password_generator.moc"
