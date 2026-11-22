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
#include "settings_files.h"
#include "vault_controller.h"
#include "window_memory.h"

#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_settings_place.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The two files, chrono-vault.ini and chrono-vault.conf; --reset deletes the first (D-026)
// v0.0.30: Initial release (D-044)

using namespace chrono_vault;
using State = VaultController::State;

// Where Chrono Vault keeps what it stores, and --reset (D-026); --debug's log
// (D-044).
class TestSettingsPlace : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("chrono-vault-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_settings_place"));
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
    }
    void cleanupTestCase()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        QFile::remove(SettingsFile().fileName());
        QFile::remove(VaultsFile().fileName());
    }
    void the_two_files_sit_side_by_side_in_the_program_folder()
    {
        const QFileInfo settings(SettingsFile().fileName());
        const QFileInfo vaults(VaultsFile().fileName());
        QCOMPARE(settings.fileName(), QStringLiteral("test_settings_place.ini"));
        QCOMPARE(vaults.fileName(), QStringLiteral("test_settings_place.conf"));
        QCOMPARE(settings.absolutePath(), vaults.absolutePath());
        QCOMPARE(settings.dir().dirName(), QStringLiteral("chrono-vault-tests"));
    }
    void each_value_goes_into_its_own_file()
    {
        QTemporaryDir folder;
        {
            VaultController controller;
            const QString vault = lockedVault(controller, folder);
            QVERIFY(!vault.isEmpty());
            controller.setAutoLockMinutes(17);
            controller.setEscapeExits(true);
            controller.setKeys(QStringLiteral("lock"), {QStringLiteral("Ctrl+K")});
            typeWrong(controller);
        }
        QSettings settings(SettingsFile().fileName(), QSettings::IniFormat);
        QSettings vaults(VaultsFile().fileName(), QSettings::IniFormat);
        for (const char* key : {"security/autoLockMinutes", "start/escapeExits", "shortcuts/lock"}) {
            QVERIFY2(settings.contains(QString::fromLatin1(key)), key);
            QVERIFY2(!vaults.contains(QString::fromLatin1(key)), key);
        }
        for (const char* key : {"vault/lastPath", "vaults/list"}) {
            QVERIFY2(vaults.contains(QString::fromLatin1(key)), key);
            QVERIFY2(!settings.contains(QString::fromLatin1(key)), key);
        }
        // The login tries are kept with the vaults.
        QVERIFY(!vaults.childGroups().isEmpty());
        vaults.beginGroup(QStringLiteral("security"));
        QVERIFY(vaults.childGroups().contains(QStringLiteral("wrongAttempts")));
        vaults.endGroup();
        settings.beginGroup(QStringLiteral("security"));
        QVERIFY(!settings.childGroups().contains(QStringLiteral("wrongAttempts")));
    }
    void reset_deletes_the_settings_file_and_keeps_the_vaults()
    {
        QTemporaryDir folder;
        QString vault;
        {
            VaultController controller;
            vault = lockedVault(controller, folder);
            QVERIFY(!vault.isEmpty());
            controller.setAutoLockMinutes(17);
            controller.setEscapeExits(true);
            controller.setKeys(QStringLiteral("lock"), {QStringLiteral("Ctrl+K")});
            for (int attempt = 0; attempt < 3; ++attempt) {
                typeWrong(controller);
            }
            QVERIFY(controller.unlockWaitSeconds() > 0);
        }
        SettingsFile().setValue(QStringLiteral("quickUnlock/enabled"), true);
        SettingsFile().setValue(QStringLiteral("windows/main/state"), static_cast<int>(Qt::WindowMaximized));
        SettingsFile().sync();
        QVERIFY(QFile::exists(SettingsFile().fileName()));

        QVERIFY(deleteSettingsFile()); // --reset
        QVERIFY(!QFile::exists(SettingsFile().fileName()));
        QVERIFY(QFile::exists(VaultsFile().fileName()));

        VaultController controller;
        // Every setting at its default, the windows as never placed.
        QCOMPARE(controller.autoLockMinutes(), 5);
        QVERIFY(!controller.escapeExits());
        QCOMPARE(controller.keysOf(QStringLiteral("lock")), QStringList{QStringLiteral("Ctrl+L")});
        QVERIFY(!controller.quickUnlockEnabled());
        QCOMPARE(WindowMemory::savedState(QStringLiteral("main")), Qt::WindowNoState);
        // The vault as it was configured, and its wait, are still there.
        QCOMPARE(controller.vaults(), QStringList{vault});
        QCOMPARE(controller.vaultPath(), vault);
        QCOMPARE(controller.state(), State::Locked);
        QVERIFY(controller.unlockWaitSeconds() > 0);
        // Nothing there to delete is no failure.
        QVERIFY(deleteSettingsFile());
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

private:
    // A vault made and locked; its file.
    static QString lockedVault(VaultController& controller, const QTemporaryDir& folder)
    {
        controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        const QString file = folder.filePath(QStringLiteral("place.chronovault"));
        controller.createVault(QUrl::fromLocalFile(file), QStringLiteral("right"), QStringLiteral("right"));
        if (!QTest::qWaitFor([&controller]() { return controller.state() == State::Unlocked; }, 10000)) {
            return {};
        }
        controller.lock();
        return file;
    }
    // A wrong master password typed, and its answer awaited.
    static void typeWrong(VaultController& controller)
    {
        controller.unlock(QStringLiteral("wrong"));
        QVERIFY(QTest::qWaitFor([&controller]() { return controller.state() == State::Locked; }, 10000));
    }
};

QTEST_MAIN(TestSettingsPlace)
#include "test_settings_place.moc"
