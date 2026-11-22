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
#include "app_setup.h"
#include "quick_unlock.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_settings_place.cpp 0.0.30

// --- Version history ----------------------------------------------------
// v0.0.30: Initial release (D-044)

using namespace chrono_vault;

// The settings under the identifier, carried over; --debug's log (D-044).
class TestSettingsPlace : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("chrono-vault-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_settings_place"));
        QSettings().clear();
    }
    void cleanupTestCase()
    {
        QSettings settings;
        settings.clear();
        settings.sync();
        QFile::remove(settings.fileName());
    }
    void the_old_settings_are_carried_over_once_and_nothing_is_lost()
    {
        {
            QSettings old(QStringLiteral("Chrono Vault Tests Old"), QStringLiteral("Old App"));
            old.setValue(QStringLiteral("vault/lastPath"), QStringLiteral("/home/someone/rob.chronovault"));
            old.setValue(QStringLiteral("security/autoLockMinutes"), 7);
        }
        QVERIFY(carryOverOldSettings(QStringLiteral("Chrono Vault Tests Old"), QStringLiteral("Old App")));
        QCOMPARE(QSettings().value(QStringLiteral("vault/lastPath")).toString(),
                 QStringLiteral("/home/someone/rob.chronovault"));
        QCOMPARE(QSettings().value(QStringLiteral("security/autoLockMinutes")).toInt(), 7);
        QVERIFY(QSettings(QStringLiteral("Chrono Vault Tests Old"), QStringLiteral("Old App")).allKeys().isEmpty());
        // Run again: what is now kept is never written over.
        QSettings().setValue(QStringLiteral("security/autoLockMinutes"), 9);
        QVERIFY(carryOverOldSettings(QStringLiteral("Chrono Vault Tests Old"), QStringLiteral("Old App")));
        QCOMPARE(QSettings().value(QStringLiteral("security/autoLockMinutes")).toInt(), 9);
    }
    void the_fingerprint_log_is_written_only_with_debug()
    {
        const QString log = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                            QStringLiteral("/chrono-vault/fingerprint.log");
        QFile::remove(log);
        setFingerLogOn(false);
        fingerLog(QStringLiteral("not written"));
        QVERIFY(!QFile::exists(log));
        setFingerLogOn(true);
        fingerLog(QStringLiteral("written"));
        QFile file(log);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("written"));
        file.close();
        QFile::remove(log);
        setFingerLogOn(false);
    }
};

QTEST_MAIN(TestSettingsPlace)
#include "test_settings_place.moc"
