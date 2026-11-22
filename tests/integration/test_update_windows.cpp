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
#include "updater.h"
#include "vault_controller.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include "settings_files.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_update_windows.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.9: Initial release

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = Updater::State;

namespace {

QByteArray releaseWith(const QString& tag)
{
    return QString::fromUtf8(R"([{"tag_name":"%1","html_url":"https://github.com/rob-vandenberg/chrono-vault/releases/tag/%1","assets":[{"name":"chrono-vault_%1_amd64.deb","browser_download_url":"https://github.com/x/chrono-vault_%1_amd64.deb","size":10,"digest":"sha256:%2"}]}])")
        .arg(tag, QString(64, QLatin1Char('c')))
        .toUtf8();
}

} // namespace

// The update's windows, as in Chrono Mail (D-024).
class TestUpdateWindows : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void a_found_update_opens_the_popup_and_skip_this_update_skips_it();
    void downloaded_then_installed_asks_restart_or_close();

private:
    struct Loaded
    {
        std::unique_ptr<QQmlApplicationEngine> engine;
        QWindow* window = nullptr;
        Updater* updater = nullptr;
    };
    Loaded load();
};

void TestUpdateWindows::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_update_windows"));
    SettingsFile().clear();
    VaultsFile().clear();
    applyStyle();
}

void TestUpdateWindows::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

TestUpdateWindows::Loaded TestUpdateWindows::load()
{
    Loaded loaded;
    loaded.engine = std::make_unique<QQmlApplicationEngine>();
    loaded.engine->loadFromModule("ChronoVault", "Main");
    if (!loaded.engine->rootObjects().isEmpty()) {
        loaded.window = qobject_cast<QWindow*>(loaded.engine->rootObjects().constFirst());
    }
    loaded.updater = loaded.engine->singletonInstance<Updater*>("ChronoVault", "Updater");
    if (loaded.updater) {
        loaded.updater->setInstallKindForTests(InstallKind::Deb);
    }
    return loaded;
}

void TestUpdateWindows::a_found_update_opens_the_popup_and_skip_this_update_skips_it()
{
    Loaded loaded = load();
    QVERIFY(loaded.window && loaded.updater);
    QObject* popup = loaded.window->findChild<QObject*>(QStringLiteral("updateAvailableWindow"));
    QVERIFY(popup);

    // Found by itself while the main window is hidden (in the tray): it waits.
    loaded.updater->answerForTests(releaseWith(QStringLiteral("99.0.0")), false);
    QCOMPARE(loaded.updater->state(), State::Available);
    QVERIFY(!popup->property("visible").toBool());

    // The main window comes on screen: the popup opens.
    showMainWindow(loaded.window, false, false);
    QTRY_VERIFY(popup->property("visible").toBool());
    QObject* headline = popup->findChild<QObject*>(QStringLiteral("headline"));
    QVERIFY(headline && headline->property("text").toString().contains(QStringLiteral("99.0.0")));
    QObject* pageLink = popup->findChild<QObject*>(QStringLiteral("pageLink"));
    QVERIFY(pageLink && pageLink->property("visible").toBool());

    // Skip this update: the window closes and that version is not offered again.
    QObject* skip = popup->findChild<QObject*>(QStringLiteral("skipButton"));
    QVERIFY(skip && skip->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(skip, "clicked"));
    QTRY_VERIFY(!popup->property("visible").toBool());
    QCOMPARE(SettingsFile().value(QStringLiteral("updates/skipped")).toString(), QStringLiteral("99.0.0"));
    loaded.updater->answerForTests(releaseWith(QStringLiteral("99.0.0")), false);
    QVERIFY(loaded.updater->state() != State::Available);
    QVERIFY(!popup->property("visible").toBool());

    // Asked for in About: About closes and the popup comes forward.
    QObject* about = loaded.window->findChild<QObject*>(QStringLiteral("aboutWindow"));
    QVERIFY(about && QMetaObject::invokeMethod(about, "open"));
    loaded.updater->answerForTests(releaseWith(QStringLiteral("99.0.0")), true);
    QTRY_VERIFY(popup->property("visible").toBool());
    QVERIFY(!about->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(popup, "reject"));
    QTRY_VERIFY(!popup->property("visible").toBool());
    SettingsFile().remove(QStringLiteral("updates/skipped"));
}

void TestUpdateWindows::downloaded_then_installed_asks_restart_or_close()
{
    Loaded loaded = load();
    QVERIFY(loaded.window && loaded.updater);
    showMainWindow(loaded.window, false, false);
    QObject* popup = loaded.window->findChild<QObject*>(QStringLiteral("updateAvailableWindow"));
    QObject* downloaded = loaded.window->findChild<QObject*>(QStringLiteral("updateDownloadedWindow"));
    QObject* updated = loaded.window->findChild<QObject*>(QStringLiteral("updatedWindow"));
    QVERIFY(popup && downloaded && updated);

    loaded.updater->answerForTests(releaseWith(QStringLiteral("98.0.0")), true);
    QTRY_VERIFY(popup->property("visible").toBool());

    // Downloaded: the popup closes as Update downloaded opens, with its folder.
    QTemporaryDir folder;
    const QString file = folder.filePath(QStringLiteral("chrono-vault_98.0.0_amd64.deb"));
    loaded.updater->downloadedForTests(file);
    QTRY_VERIFY(downloaded->property("visible").toBool());
    QVERIFY(!popup->property("visible").toBool());
    QObject* folderLink = downloaded->findChild<QObject*>(QStringLiteral("folderLink"));
    QVERIFY(folderLink && folderLink->property("text").toString().contains(QStringLiteral("Saved in")));
    QObject* install = downloaded->findChild<QObject*>(QStringLiteral("installButton"));
    QVERIFY(install && install->property("enabled").toBool());

    // The system refuses (a cancelled password): the window says so, Install again.
    loaded.updater->installAnsweredForTests(false, QStringLiteral("Not authorized"));
    QObject* note = downloaded->findChild<QObject*>(QStringLiteral("note"));
    QVERIFY(note && note->property("text").toString().contains(QStringLiteral("still in its folder")));
    QVERIFY(install->property("enabled").toBool());
    QVERIFY(downloaded->property("visible").toBool());

    // Installed: Update downloaded closes, Restart or Close is asked. Close
    // quits without a restart.
    QSignalSpy quit(loaded.engine.get(), &QQmlEngine::quit);
    loaded.updater->installAnsweredForTests(true, QString());
    QTRY_VERIFY(updated->property("visible").toBool());
    QVERIFY(!downloaded->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(updated, "reject"));
    QTRY_COMPARE(quit.count(), 1);
    QVERIFY(!loaded.updater->armedForTests());

    // The quit is held up (unsaved work, then Cancel): Restart or Close is
    // asked again, never left without a word.
    loaded.updater->quitHeldUp();
    QTRY_VERIFY(updated->property("visible").toBool());
    QVERIFY(loaded.updater->waitsForRestartChoice());
    QObject* restart = updated->findChild<QObject*>(QStringLiteral("restartButton"));
    QVERIFY(restart && QMetaObject::invokeMethod(restart, "clicked"));
    QTRY_COMPARE(quit.count(), 2);
    QVERIFY(loaded.updater->armedForTests());
    loaded.updater->quitHeldUp(); // nothing restarts when the test ends
}

QTEST_MAIN(TestUpdateWindows)
#include "test_update_windows.moc"
