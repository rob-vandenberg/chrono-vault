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
#include "vault_controller.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include "settings_files.h"
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_reader.cpp 0.8.126

// --- Version history ----------------------------------------------------
// v0.8.126: The standard master password dialog, a second vault open (D-043)
// v0.8.125: Ticking the box asks the open vault's master password before the touch (D-043)
// v0.4.73: The finger asked only once the window is active
// v0.4.73: The width compared to within a pixel, as the layout rounds
// v0.4.72: Unlock with Windows Hello as wide as its text, centred
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.27: Initial release (D-042)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

// Stands in for fprintd: it holds the reader from the moment it is asked
// until it is answered or cancelled, as the real one does.
class Reader : public QuickUnlock
{
public:
    QHash<QString, QByteArray> kept;
    bool holding = false;
    int asked = 0;
    bool storing = false;
    QString vault;
    QByteArray secret;

    [[nodiscard]] bool available() const override { return true; }
    [[nodiscard]] QString name() const override { return QStringLiteral("fingerprint"); }
    [[nodiscard]] QString touchPrompt() const override { return QStringLiteral("Touch the fingerprint reader…"); }
    void store(const QString& forVault, const QByteArray& forSecret) override
    {
        ask(true, forVault);
        secret = forSecret;
    }
    void retrieve(const QString& forVault) override { ask(false, forVault); }
    // As on Linux: a key kept without asking for a finger.
    void replace(const QString& forVault, const QByteArray& forSecret) override
    {
        kept.insert(forVault, forSecret);
        remember(forVault, true);
        emit stored(Result::Done);
    }
    void remove(const QString& forVault) override
    {
        kept.remove(forVault);
        remember(forVault, false);
    }
    void cancel() override
    {
        if (holding) {
            answer(Result::Cancelled);
        }
    }
    // The person's finger: recognized, or not (the reader stays held).
    void finger(bool recognized)
    {
        if (!recognized) {
            emit notRecognized();
            return;
        }
        answer(Result::Done);
    }
    void answer(Result result)
    {
        holding = false;
        if (storing) {
            if (result == Result::Done) {
                kept.insert(vault, secret);
                remember(vault, true);
            }
            emit stored(result);
        } else {
            emit retrieved(result, result == Result::Done ? kept.value(vault) : QByteArray());
        }
    }

private:
    void ask(bool forStoring, const QString& forVault)
    {
        cancel(); // the one still asked for gives the reader back first
        ++asked;
        holding = true;
        storing = forStoring;
        vault = forVault;
        emit fingerReady();
    }
};

// Turned on as the settings window does: the master password, the primary
// vault's, typed and proven first (D-043).
bool turnOn(VaultController& controller, const QString& password = QStringLiteral("pass"))
{
    QSignalSpy proven(&controller, &VaultController::proven);
    controller.askFor(controller.primaryVault());
    controller.proveWithPassword(password);
    if (!proven.wait(10000) && proven.isEmpty()) {
        return false;
    }
    controller.setQuickUnlock(true);
    controller.stopAsking();
    return true;
}

} // namespace

// The fingerprint reader is held only while a finger is asked, and given
// back the moment that is settled (the owner, D-042).
class TestReader : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();
    void turning_it_on_asks_one_touch_and_tells_what_came_of_it();
    void a_cancel_is_no_failure_and_a_key_not_kept_is_told();
    void unlocking_holds_the_reader_until_it_is_settled();
    void the_unlock_page_asks_whenever_it_is_on_screen();
    void one_setting_for_all_vaults_keeps_each_key_at_its_unlock();
    void ticking_the_box_asks_the_master_password_before_the_touch();

private:
    std::unique_ptr<VaultController> unlocked(Reader*& reader, const QString& name);
    QTemporaryDir* folder = nullptr;
};

void TestReader::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_reader"));
    applyStyle();
}

void TestReader::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestReader::init()
{
    SettingsFile().clear();
    VaultsFile().clear();
    folder = new QTemporaryDir;
}

void TestReader::cleanup()
{
    SettingsFile().clear();
    VaultsFile().clear();
    delete folder;
    folder = nullptr;
}

std::unique_ptr<VaultController> TestReader::unlocked(Reader*& reader, const QString& name)
{
    auto controller = std::make_unique<VaultController>();
    auto owned = std::make_unique<Reader>();
    reader = owned.get();
    controller->setQuickUnlockForTests(std::move(owned));
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder->filePath(name + QStringLiteral(".chronovault"))),
                            QStringLiteral("pass"), QStringLiteral("pass"));
    if (!QTest::qWaitFor([&controller]() { return controller->state() == State::Unlocked; }, 10000)) {
        return nullptr;
    }
    return controller;
}

void TestReader::turning_it_on_asks_one_touch_and_tells_what_came_of_it()
{
    Reader* reader = nullptr;
    auto controller = unlocked(reader, QStringLiteral("on"));
    QVERIFY(controller);
    QVERIFY(turnOn(*controller));
    QVERIFY(controller->quickUnlockBusy());
    QVERIFY(reader->holding);
    QCOMPARE(controller->quickUnlockMessage(), QStringLiteral("Touch the fingerprint reader…"));
    // Pressed again while waiting: no second check.
    controller->setQuickUnlock(true);
    QCOMPARE(reader->asked, 1);
    // A finger not recognized: still held, asked again.
    reader->finger(false);
    QVERIFY(reader->holding);
    QVERIFY(controller->quickUnlockTrouble());
    QCOMPARE(controller->quickUnlockMessage(), QStringLiteral("Not recognized. Try again."));
    // Recognized: settled, the reader given back, said with a capital.
    reader->finger(true);
    QVERIFY(!reader->holding);
    QVERIFY(!controller->quickUnlockBusy());
    QVERIFY(controller->quickUnlockOn());
    QVERIFY(!controller->quickUnlockTrouble());
    QVERIFY(controller->quickUnlockMessage().isEmpty()); // the ticked box says it
    QVERIFY(controller->quickUnlockEnabled());
}

void TestReader::a_cancel_is_no_failure_and_a_key_not_kept_is_told()
{
    Reader* reader = nullptr;
    auto controller = unlocked(reader, QStringLiteral("cancel"));
    QVERIFY(controller);
    QVERIFY(turnOn(*controller));
    controller->cancelQuickUnlockSetup();
    QVERIFY(!reader->holding);
    QVERIFY(!controller->quickUnlockBusy());
    QVERIFY(!controller->quickUnlockOn());
    QVERIFY(controller->quickUnlockMessage().isEmpty());
    QVERIFY(!controller->messageIsTrouble());
    // Recognized, but the keyring would not keep the key.
    QVERIFY(turnOn(*controller));
    reader->answer(QuickUnlock::Result::NotKept);
    QVERIFY(!controller->quickUnlockOn());
    QVERIFY(controller->quickUnlockTrouble());
    QCOMPARE(controller->quickUnlockMessage(), QStringLiteral("The key could not be kept for this vault."));
}

void TestReader::unlocking_holds_the_reader_until_it_is_settled()
{
    Reader* reader = nullptr;
    auto controller = unlocked(reader, QStringLiteral("held"));
    QVERIFY(controller);
    QVERIFY(turnOn(*controller));
    reader->finger(true);
    QVERIFY(controller->quickUnlockOn());
    controller->lock();

    // Asked: held; asked again meanwhile: the same question, no second one.
    reader->asked = 0;
    controller->unlockWithQuickUnlock();
    QVERIFY(reader->holding);
    controller->unlockWithQuickUnlock();
    QCOMPARE(reader->asked, 1);
    // A finger not recognized keeps it held.
    reader->finger(false);
    QVERIFY(reader->holding);
    QCOMPARE(controller->message(), QStringLiteral("Not recognized. Try again."));
    // Settled by the master password: given back.
    controller->unlock(QStringLiteral("pass"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QVERIFY(!reader->holding);
    controller->lock();

    // Settled by the page leaving the screen: given back.
    controller->unlockWithQuickUnlock();
    QVERIFY(reader->holding);
    controller->stopQuickUnlock();
    QVERIFY(!reader->holding);

    // Another vault chosen: this one's question ends.
    controller->unlockWithQuickUnlock();
    QVERIFY(reader->holding);
    controller->chooseVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("other.chronovault"))));
    QVERIFY(!reader->holding);
    controller->chooseVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("held.chronovault"))));

    // Settled by the finger: given back, and the vault opens.
    controller->unlockWithQuickUnlock();
    QVERIFY(reader->holding);
    reader->finger(true);
    QVERIFY(!reader->holding);
    QTRY_COMPARE(controller->state(), State::Unlocked);
}

void TestReader::the_unlock_page_asks_whenever_it_is_on_screen()
{
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* window = qobject_cast<QWindow*>(engine.rootObjects().constFirst());
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    auto owned = std::make_unique<Reader>();
    Reader* reader = owned.get();
    controller->setQuickUnlockForTests(std::move(owned));
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("page.chronovault"))),
                            QStringLiteral("pass"), QStringLiteral("pass"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QVERIFY(turnOn(*controller));
    reader->finger(true);
    QVERIFY(controller->quickUnlockOn());

    // Locked while the window is hidden (in the tray): nothing is asked.
    reader->asked = 0;
    controller->lock();
    QCOMPARE(reader->asked, 0);
    // The window shows: asked at once, as at a start.
    showMainWindow(window, false, false);
    QTRY_COMPARE(reader->asked, 1);
    QVERIFY(reader->holding);
    // Closed into the tray: given back.
    controller->setTrayAvailable(true);
    window->close();
    QTRY_VERIFY(!window->isVisible());
    QVERIFY(!reader->holding);
    // Back from the tray: asked again.
    showMainWindow(window, false, false);
    QTRY_COMPARE(reader->asked, 2);
    controller->stopQuickUnlock();
    // Another window active: not asked until this one is active again, so
    // Windows lets Windows Hello come on top (the owner, 0.4.73).
    {
        QWindow other;
        other.resize(100, 100);
        other.show();
        other.requestActivate();
        QTRY_VERIFY(!window->isActive());
        const int before = reader->asked;
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        controller->lock();
        QTest::qWait(50); // nothing may be asked meanwhile
        QCOMPARE(reader->asked, before);
        window->requestActivate();
        QTRY_COMPARE(reader->asked, before + 1);
        controller->stopQuickUnlock();
        // Cancelled, then active again (as when Windows Hello's window closes):
        // not asked again until the page is shown anew.
        other.requestActivate();
        QTRY_VERIFY(!window->isActive());
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QCOMPARE(reader->asked, before + 1);
    }
    // The button is as wide as its text, centred under the field (the owner, 0.4.72).
    auto* button = window->findChild<QQuickItem*>(QStringLiteral("quickUnlockButton"));
    auto* field = window->findChild<QQuickItem*>(QStringLiteral("masterPasswordField"));
    QVERIFY(button && field);
    QTRY_VERIFY(button->isVisible());
    QVERIFY(qAbs(button->width() - button->implicitWidth()) < 1); // the layout rounds to whole pixels
    QVERIFY(button->width() < field->width());
    const auto middleOf = [](QQuickItem* item) { return item->mapToScene(QPointF(item->width() / 2, 0)).x(); };
    QTRY_VERIFY(qAbs(middleOf(button) - middleOf(field)) <= 1);

    // The reminder says how often it asks, as Settings > Security says (D-049).
    {
        VaultsFile settings;
        QVariantMap times = settings.value(QStringLiteral("security/lastMasterPassword")).toMap();
        times.insert(controller->vaultPath(), QDateTime::currentDateTimeUtc().addDays(-400));
        settings.setValue(QStringLiteral("security/lastMasterPassword"), times);
    }
    controller->setMasterPasswordReminder(0);
    QTRY_VERIFY(controller->reminderDue());
    QObject* reminderText = window->findChild<QObject*>(QStringLiteral("reminderText"));
    QVERIFY(reminderText);
    QVERIFY(reminderText->property("text").toString().contains(QStringLiteral("once a day")));
    controller->setMasterPasswordReminder(2);
    QVERIFY(reminderText->property("text").toString().contains(QStringLiteral("once a quarter")));
    controller->stopQuickUnlock();

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestReader::one_setting_for_all_vaults_keeps_each_key_at_its_unlock()
{
    Reader* reader = nullptr;
    auto controller = unlocked(reader, QStringLiteral("first"));
    QVERIFY(controller);
    QVERIFY(turnOn(*controller));
    reader->finger(true);
    QVERIFY(controller->quickUnlockEnabled());
    const QString first = controller->vaultPath();
    QVERIFY(reader->kept.contains(first));

    // Another vault, unlocked with its master password: its key is kept
    // too, without a touch (D-043).
    controller->createVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("second.chronovault"))),
                            QStringLiteral("two"), QStringLiteral("two"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    const QString second = controller->vaultPath();
    controller->lock();
    controller->chooseVault(QUrl::fromLocalFile(second));
    controller->unlock(QStringLiteral("two"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    QTRY_VERIFY(reader->kept.contains(second));
    QVERIFY(controller->quickUnlockOn());

    // Unticked: every kept key removed.
    controller->setQuickUnlock(false);
    QVERIFY(!controller->quickUnlockEnabled());
    QVERIFY(reader->kept.isEmpty());
    QVERIFY(!controller->quickUnlockOn());
}

// Settings > Security, a second vault open: ticking the box first shows the
// standard dialog asking the master password; Cancel, a wrong password and the
// open vault's own password ask no finger and leave it unticked; the master
// password goes on to the finger (the owner, D-043).
void TestReader::ticking_the_box_asks_the_master_password_before_the_touch()
{
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    auto owned = std::make_unique<Reader>();
    Reader* reader = owned.get();
    controller->setQuickUnlockForTests(std::move(owned));
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("box.chronovault"))),
                            QStringLiteral("pass"), QStringLiteral("pass"));
    QTRY_COMPARE(controller->state(), State::Unlocked);
    const QString second = folder->filePath(QStringLiteral("second.chronovault"));
    controller->createVault(QUrl::fromLocalFile(second), QStringLiteral("two"), QStringLiteral("two"));
    QTRY_VERIFY(controller->vaultPath() == second && controller->state() == State::Unlocked);
    QVERIFY(controller->primaryVault() != second);

    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    settings->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 1);
    QObject* box = nullptr;
    QTRY_VERIFY((box = settings->findChild<QObject*>(QStringLiteral("quickUnlockToggle"))));
    auto* asked = settings->findChild<QQuickWindow*>(QStringLiteral("fingerPasswordWindow"));
    QVERIFY(asked);
    QObject* field = asked->findChild<QObject*>(QStringLiteral("askField"));
    QVERIFY(field);

    // Ticked: the standard master password dialog, no finger, the box still unticked.
    QVERIFY(QMetaObject::invokeMethod(box, "click"));
    QTRY_VERIFY(asked->isVisible());
    QCOMPARE(asked->title(), QStringLiteral("Unlock the vault"));
    QCOMPARE(asked->property("vaultFile").toString(), controller->primaryVault());
    QCOMPARE(asked->findChild<QObject*>(QStringLiteral("askHeading"))->property("text").toString(),
             QStringLiteral("Enter the Master Password to unlock"));
    QCOMPARE(asked->findChild<QObject*>(QStringLiteral("askOk"))->property("text").toString(), QStringLiteral("Unlock"));
    QCOMPARE(reader->asked, 0);
    QVERIFY(!box->property("checked").toBool());
    QVERIFY(!asked->findChild<QQuickItem*>(QStringLiteral("askFinger"))->isVisible());

    // Wrong: said in the dialog, which stays open; nothing kept.
    field->setProperty("text", QStringLiteral("no"));
    QVERIFY(QMetaObject::invokeMethod(asked, "go"));
    QTRY_VERIFY(controller->askTrouble());
    QVERIFY(asked->isVisible());
    QVERIFY(asked->findChild<QQuickItem*>(QStringLiteral("askProblem"))->isVisible());
    QCOMPARE(reader->asked, 0);
    QVERIFY(!box->property("checked").toBool());

    // The open vault's own password is not the master password.
    field->setProperty("text", QStringLiteral("two"));
    QVERIFY(QMetaObject::invokeMethod(asked, "go"));
    QTRY_VERIFY(controller->askTrouble());
    QVERIFY(asked->isVisible());
    QCOMPARE(reader->asked, 0);
    QVERIFY(!box->property("checked").toBool());

    // Cancel: nothing kept, still unticked.
    QVERIFY(QMetaObject::invokeMethod(asked, "reject"));
    QTRY_VERIFY(!asked->isVisible());
    QCOMPARE(reader->asked, 0);
    QVERIFY(!box->property("checked").toBool());
    QVERIFY(!controller->quickUnlockEnabled());

    // Right: the dialog closes and the touch is asked, as before.
    QVERIFY(QMetaObject::invokeMethod(box, "click"));
    QTRY_VERIFY(asked->isVisible());
    field->setProperty("text", QStringLiteral("pass"));
    QVERIFY(QMetaObject::invokeMethod(asked, "go"));
    QTRY_COMPARE(reader->asked, 1);
    QTRY_VERIFY(!asked->isVisible());
    QVERIFY(controller->quickUnlockBusy());
    QVERIFY(box->property("checked").toBool());
    reader->finger(true);
    QVERIFY(controller->quickUnlockOn());
    QVERIFY(controller->quickUnlockEnabled());

    // Unticked: every kept key gone, nothing asked.
    QVERIFY(QMetaObject::invokeMethod(box, "click"));
    QVERIFY(!asked->isVisible());
    QVERIFY(!controller->quickUnlockEnabled());
    QVERIFY(reader->kept.isEmpty());
    QCOMPARE(reader->asked, 1);
    settings->setProperty("visible", false);
}

QTEST_MAIN(TestReader)
#include "test_reader.moc"
