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
#include "vault_controller.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include "settings_files.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_unlock_waits.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.26: Initial release (D-041)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

// The waits after wrong master passwords (D-041), with the tests' clock:
// no test waits for real.
class TestUnlockWaits : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void the_rule_is_the_owners();
    void wrong_passwords_wait_as_the_rule_says_and_a_right_one_ends_it();
    void a_wait_survives_a_restart_and_a_clock_set_back();
    void the_unlock_page_greys_and_counts_down();

private:
    // A vault made and locked; its file.
    QString lockedVault(VaultController& controller, const QString& name);
    // A wrong master password typed, and its answer awaited.
    static void typeWrong(VaultController& controller);
    QTemporaryDir* folder = nullptr;
};

void TestUnlockWaits::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_unlock_waits"));
    applyStyle();
}

void TestUnlockWaits::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestUnlockWaits::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
}

void TestUnlockWaits::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
    delete folder;
    folder = nullptr;
}

QString TestUnlockWaits::lockedVault(VaultController& controller, const QString& name)
{
    controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    const QString file = folder->filePath(name + QStringLiteral(".chronovault"));
    controller.createVault(QUrl::fromLocalFile(file), QStringLiteral("right"), QStringLiteral("right"));
    if (!QTest::qWaitFor([&controller]() { return controller.state() == State::Unlocked; }, 10000)) {
        return {};
    }
    controller.lock();
    return file;
}

void TestUnlockWaits::typeWrong(VaultController& controller)
{
    controller.unlock(QStringLiteral("wrong"));
    QVERIFY(QTest::qWaitFor([&controller]() { return controller.state() == State::Locked; }, 10000));
}

void TestUnlockWaits::the_rule_is_the_owners()
{
    const QList<std::pair<int, int>> rule{{1, 0},   {3, 0},   {4, 10},    {5, 10},  {6, 60},  {10, 60},
                                          {11, 600}, {25, 600}, {26, 3600}, {27, 600}, {51, 3600}, {76, 3600}};
    for (const auto& [attempt, seconds] : rule) {
        QCOMPARE(VaultController::waitBeforeAttempt(attempt), seconds);
    }
}

void TestUnlockWaits::wrong_passwords_wait_as_the_rule_says_and_a_right_one_ends_it()
{
    VaultController controller;
    QVERIFY(!lockedVault(controller, QStringLiteral("waits")).isEmpty());
    // Three wrong ones at once, then 10 seconds before the fourth.
    for (int attempt = 1; attempt <= 3; ++attempt) {
        QCOMPARE(controller.unlockWaitSeconds(), 0);
        typeWrong(controller);
    }
    QCOMPARE(controller.unlockWaitSeconds(), 10);
    // Tried during the wait: nothing is tried, not even the right one.
    controller.unlock(QStringLiteral("right"));
    QCOMPARE(controller.state(), State::Locked);
    controller.moveClockForTests(10);
    QCOMPARE(controller.unlockWaitSeconds(), 0);
    typeWrong(controller); // the fourth
    QCOMPARE(controller.unlockWaitSeconds(), 10);
    controller.moveClockForTests(10);
    typeWrong(controller); // the fifth: a minute before the sixth
    QCOMPARE(controller.unlockWaitSeconds(), 60);
    for (int attempt = 6; attempt <= 10; ++attempt) {
        controller.moveClockForTests(600);
        typeWrong(controller);
    }
    QCOMPARE(controller.unlockWaitSeconds(), 600); // before the eleventh
    for (int attempt = 11; attempt <= 25; ++attempt) {
        controller.moveClockForTests(600);
        typeWrong(controller);
    }
    QCOMPARE(controller.unlockWaitSeconds(), 3600); // before the 26th, once
    controller.moveClockForTests(3600);
    typeWrong(controller);
    QCOMPARE(controller.unlockWaitSeconds(), 600);

    // The right one, once allowed, opens it and starts the count again.
    controller.moveClockForTests(600);
    controller.unlock(QStringLiteral("right"));
    QTRY_COMPARE(controller.state(), State::Unlocked);
    QCOMPARE(controller.unlockWaitSeconds(), 0);
    controller.lock();
    for (int attempt = 1; attempt <= 3; ++attempt) {
        QCOMPARE(controller.unlockWaitSeconds(), 0);
        typeWrong(controller);
    }
    QCOMPARE(controller.unlockWaitSeconds(), 10);
}

void TestUnlockWaits::a_wait_survives_a_restart_and_a_clock_set_back()
{
    QString file;
    {
        VaultController controller;
        file = lockedVault(controller, QStringLiteral("restart"));
        QVERIFY(!file.isEmpty());
        for (int attempt = 1; attempt <= 5; ++attempt) {
            controller.moveClockForTests(60);
            typeWrong(controller);
        }
        QCOMPARE(controller.unlockWaitSeconds(), 60);
    }
    // Started again: the same vault still waits.
    VaultController again;
    again.chooseVault(QUrl::fromLocalFile(file));
    QVERIFY(again.unlockWaitSeconds() > 0);
    QVERIFY(again.unlockWaitSeconds() <= 60);
    // A clock set a day back never makes it longer than its minute.
    again.moveClockForTests(-24 * 3600);
    QVERIFY(again.unlockWaitSeconds() <= 60);
    // Another vault does not wait for this one.
    VaultController other;
    const QString otherFile = lockedVault(other, QStringLiteral("other"));
    QVERIFY(!otherFile.isEmpty());
    other.chooseVault(QUrl::fromLocalFile(otherFile)); // a lock shows the primary again
    QCOMPARE(other.unlockWaitSeconds(), 0);
}

void TestUnlockWaits::the_unlock_page_greys_and_counts_down()
{
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    QVERIFY(!lockedVault(*controller, QStringLiteral("page")).isEmpty());
    QObject* field = window->findChild<QObject*>(QStringLiteral("masterPasswordField"));
    QObject* unlockButton = window->findChild<QObject*>(QStringLiteral("unlockButton"));
    QObject* waitText = window->findChild<QObject*>(QStringLiteral("unlockWaitText"));
    QVERIFY(field && unlockButton && waitText);
    QVERIFY(unlockButton->property("enabled").toBool());
    for (int attempt = 1; attempt <= 5; ++attempt) {
        controller->moveClockForTests(60);
        typeWrong(*controller);
    }
    QTRY_VERIFY(!unlockButton->property("enabled").toBool());
    QVERIFY(!field->property("enabled").toBool());
    QVERIFY(waitText->property("visible").toBool());
    QCOMPARE(waitText->property("text").toString(), QStringLiteral("Try again in 1:00"));
    controller->moveClockForTests(8);
    QCOMPARE(waitText->property("text").toString(), QStringLiteral("Try again in 0:52"));
    controller->moveClockForTests(52);
    QTRY_VERIFY(unlockButton->property("enabled").toBool());
    QVERIFY(field->property("enabled").toBool());
    QVERIFY(!waitText->property("visible").toBool());

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

QTEST_MAIN(TestUnlockWaits)
#include "test_unlock_waits.moc"
