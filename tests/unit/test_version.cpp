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

#include "version.h"

#include <QRegularExpression>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_version.cpp 0.0.1

// --- Version history ----------------------------------------------------
// v0.0.1: Initial release

class TestVersion : public QObject
{
    Q_OBJECT

private slots:
    void the_version_is_the_project_version();
    void the_version_is_three_numbers_with_dots_only();
};

void TestVersion::the_version_is_the_project_version()
{
    QCOMPARE(chrono_vault::programVersion(), QStringLiteral(CHRONO_VAULT_PROJECT_VERSION));
}

void TestVersion::the_version_is_three_numbers_with_dots_only()
{
    // CPP-1: x.y.z, with dots only, no "v".
    static const QRegularExpression threeNumbers(QStringLiteral(R"(^\d+\.\d+\.\d+$)"));
    QVERIFY2(threeNumbers.match(chrono_vault::programVersion()).hasMatch(),
             qPrintable(chrono_vault::programVersion()));
}

QTEST_GUILESS_MAIN(TestVersion)
#include "test_version.moc"
