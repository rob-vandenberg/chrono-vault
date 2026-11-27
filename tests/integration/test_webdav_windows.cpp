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
#include "google_drive_storage.h"
#include "google_drive_test_server.h"
#include "settings_files.h"
#include "vault_controller.h"
#include "webdav_sync.h"
#include "webdav_test_server.h"

#include <QDir>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtQml/QQmlExtensionPlugin>
#include <QUrlQuery>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_webdav_windows.cpp 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: Create and Move fit their tiles, Open keeps its size; WebDAV and Google Drive straight to the file dialog; Disconnect in paragraphs (D-082)
// v0.8.119: The Disconnect dialog names the storage and the vault (D-060)
// v0.6.106: Close and Lock per vault (D-075)
// v0.6.88: The settings' topics in their new order (D-066)
// v0.3.67: A refused New folder and a cancelled Browse… keep Back and the choice right
// v0.5.77: Import… and Export… in the vault menu (D-060)
// v0.5.75: Settings > Vaults: the menu per vault, Disconnect without deleting, Lock (D-060)
// v0.3.64: The unlock page's vault list and field
// v0.3.63: One button Connect then Next…; Browse… and the WebDAV file dialog; the window's height
// v0.3.62: The Select Storage tiles, the server list, the folder browser, the unlock page's address
// v0.3.60: Initial release: the WebDAV windows and Settings > Vaults, driven as a person would (D-057)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

class MemoryStore : public SecretStore
{
public:
    bool keep(const QString& name, const QByteArray& secret) override
    {
        const QMutexLocker locker(&lock);
        kept.insert(name, secret);
        return true;
    }
    std::pair<Result, QByteArray> find(const QString& name) override
    {
        const QMutexLocker locker(&lock);
        return kept.contains(name) ? std::pair{Result::Done, kept.value(name)} : std::pair{Result::NoSecret, QByteArray()};
    }
    bool remove(const QString& name) override
    {
        const QMutexLocker locker(&lock);
        kept.remove(name);
        return true;
    }

private:
    QMutex lock;
    QHash<QString, QByteArray> kept;
};

QQuickItem* visualItem(QQuickItem* root, const QString& name)
{
    if (!root) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = visualItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

QQuickItem* inWindow(QObject* window, const QString& name)
{
    auto* quick = qobject_cast<QQuickWindow*>(window);
    return quick ? visualItem(quick->contentItem(), name) : nullptr;
}

// Google's page in the browser, answered at once with a sign-in.
void signInInBrowser(const QUrl& address)
{
    static QNetworkAccessManager browser;
    const QUrlQuery asked(address);
    QUrl back(asked.queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded));
    QUrlQuery answer;
    answer.addQueryItem(QStringLiteral("code"), QStringLiteral("the-code"));
    answer.addQueryItem(QStringLiteral("state"), asked.queryItemValue(QStringLiteral("state"), QUrl::FullyDecoded));
    back.setQuery(answer);
    QNetworkReply* reply = browser.get(QNetworkRequest(back));
    QObject::connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void click(QObject* button)
{
    QVERIFY(button);
    QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
}

} // namespace

class TestWebDavWindows : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_webdav_windows"));
        applyStyle();
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        QDir(webDavDataFolder()).removeRecursively();
    }
    void cleanupTestCase()
    {
        init();
        QFile::remove(SettingsFile().fileName());
        QFile::remove(VaultsFile().fileName());
    }

    void a_new_vault_on_a_webdav_server_through_the_windows();
    void create_and_move_fit_their_tiles_and_open_keeps_its_size();
    void google_drive_opens_its_file_dialog_once_signed_in();
};

// Signed in: Google Drive's file dialog opens by itself, the only window
// seen; Cancel there ends it all, a name goes on to the master password (D-082).
void TestWebDavWindows::google_drive_opens_its_file_dialog_once_signed_in()
{
    GoogleDriveTestServer server;
    const QUrl base = server.base();
    setGoogleForTests({base.resolved(QUrl(QStringLiteral("auth"))), base.resolved(QUrl(QStringLiteral("token"))),
                       base.resolved(QUrl(QStringLiteral("drive/v3/"))), base.resolved(QUrl(QStringLiteral("upload/drive/v3/")))},
                      QStringLiteral("test-client"), QStringLiteral("test-secret"));
    const auto done = qScopeGuard([]() { setGoogleForTests({}, QString(), QString()); });
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->setSecretStoreForTests(std::make_unique<MemoryStore>());
    controller->setGoogleBrowserForTests(signInInBrowser);
    QObject* choice = window->findChild<QObject*>(QStringLiteral("storageWindow"));
    QObject* drive = window->findChild<QObject*>(QStringLiteral("googleDriveWindow"));
    QVERIFY(choice && drive);
    QObject* files = drive->findChild<QObject*>(QStringLiteral("googleDriveFileDialog"));
    QVERIFY(files);
    for (const bool cancelled : {true, false}) {
        QVERIFY(QMetaObject::invokeMethod(window, "newVault"));
        QTRY_VERIFY(choice->property("visible").toBool());
        click(inWindow(choice, QStringLiteral("googleDriveTile")));
        QTRY_VERIFY(drive->property("visible").toBool());
        click(inWindow(drive, QStringLiteral("googleDriveGoButton")));
        QTRY_VERIFY_WITH_TIMEOUT(files->property("visible").toBool(), 10000);
        QVERIFY(!inWindow(drive, QStringLiteral("googleDriveChosenField")));
        // Signed in: the sign-in window is gone, only the file dialog is on
        // screen, over the main window (the owner, D-082).
        QVERIFY(!drive->property("visible").toBool());
        QCOMPARE(qobject_cast<QQuickWindow*>(files)->transientParent(), window);
        if (cancelled) {
            QVERIFY(QMetaObject::invokeMethod(files, "reject"));
            QTRY_VERIFY(!files->property("visible").toBool());
            QTRY_VERIFY(!drive->property("visible").toBool());
            continue;
        }
        inWindow(files, QStringLiteral("fileDialogNameField"))->setProperty("text", QStringLiteral("Drive"));
        click(inWindow(files, QStringLiteral("fileDialogChooseButton")));
        QTRY_VERIFY(!files->property("visible").toBool());
        QObject* masterPassword = window->findChild<QObject*>(QStringLiteral("newVaultDialog"));
        QTRY_VERIFY(masterPassword->property("visible").toBool());
        QCOMPARE(masterPassword->property("webDavName").toString(), QStringLiteral("Drive"));
        QCOMPARE(masterPassword->property("cloudKind").toString(), QStringLiteral("googledrive"));
        QTRY_VERIFY(!drive->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(masterPassword, "reject"));
    }
}

// Open a vault remembers the size the person gave it; Create a vault, in the
// same window, and Move a vault are exactly as large as their tiles (D-079).
void TestWebDavWindows::create_and_move_fit_their_tiles_and_open_keeps_its_size()
{
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* choice = qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QStringLiteral("storageWindow")));
    QVERIFY(choice);
    const auto fits = [](QQuickWindow* shown) {
        return shown->width() == shown->minimumWidth() && shown->height() == shown->minimumHeight()
               && shown->maximumWidth() == shown->minimumWidth() && shown->maximumHeight() == shown->minimumHeight();
    };

    // Open a vault, made large by the person.
    QVERIFY(QMetaObject::invokeMethod(window, "openVault"));
    QTRY_VERIFY(choice->isVisible());
    const QSize large(choice->minimumWidth() + 200, choice->minimumHeight() + 300);
    choice->resize(large);
    QTRY_COMPARE(choice->size(), large);
    QVERIFY(QMetaObject::invokeMethod(choice, "reject"));
    QTRY_VERIFY(!choice->isVisible());

    // Create a vault: its tiles, no room around them.
    QVERIFY(QMetaObject::invokeMethod(window, "newVault"));
    QTRY_VERIFY(choice->isVisible());
    QTRY_VERIFY(fits(choice));
    QVERIFY(choice->height() < large.height());
    QVERIFY(QMetaObject::invokeMethod(choice, "reject"));
    QTRY_VERIFY(!choice->isVisible());

    // Open a vault again: the size it was given.
    QVERIFY(QMetaObject::invokeMethod(window, "openVault"));
    QTRY_VERIFY(choice->isVisible());
    QTRY_COMPARE(choice->size(), large);
    QVERIFY(QMetaObject::invokeMethod(choice, "reject"));
    QTRY_VERIFY(!choice->isVisible());

    // Move a vault, from Settings > Vaults: its tiles too.
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    auto* move = qobject_cast<QQuickWindow*>(settings->findChild<QObject*>(QStringLiteral("moveStorageWindow")));
    QVERIFY(move);
    QVERIFY(QMetaObject::invokeMethod(move, "open"));
    QTRY_VERIFY(move->isVisible());
    QTRY_VERIFY(fits(move));
    QVERIFY(QMetaObject::invokeMethod(move, "reject"));
    QTRY_VERIFY(!move->isVisible());
    click(settings->findChild<QObject*>(QStringLiteral("cancelButton")));
}

void TestWebDavWindows::a_new_vault_on_a_webdav_server_through_the_windows()
{
    WebDavTestServer server(true);
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject* window = engine.rootObjects().constFirst();
    showMainWindow(qobject_cast<QWindow*>(window), false, false);
    auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
    QVERIFY(controller);
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->setSecretStoreForTests(std::make_unique<MemoryStore>());

    // File > New vault… asks where first.
    QVERIFY(QMetaObject::invokeMethod(window, "newVault"));
    // A tile per storage (0.3.62).
    QObject* choice = window->findChild<QObject*>(QStringLiteral("storageWindow"));
    QVERIFY(choice);
    QTRY_VERIFY(choice->property("visible").toBool());
    QVERIFY(inWindow(choice, QStringLiteral("deviceTile")));
    click(inWindow(choice, QStringLiteral("webDavTile")));
    QObject* webDav = window->findChild<QObject*>(QStringLiteral("webDavWindow"));
    QVERIFY(webDav);
    QTRY_VERIFY(webDav->property("visible").toBool());
    QCOMPARE(webDav->property("mode").toString(), QStringLiteral("new"));

    QQuickItem* address = inWindow(webDav, QStringLiteral("webDavAddressField"));
    // The address starts empty; pCloud's is filled in only when chosen.
    QQuickItem* serverBox = inWindow(webDav, QStringLiteral("webDavServerBox"));
    QVERIFY(address && serverBox);
    QCOMPARE(address->property("text").toString(), QString());
    QVERIFY(QMetaObject::invokeMethod(serverBox, "activated", Q_ARG(int, 0)));
    QCOMPARE(address->property("text").toString(), QStringLiteral("https://ewebdav.pcloud.com"));
    QVERIFY(QMetaObject::invokeMethod(serverBox, "activated", Q_ARG(int, 2)));
    QCOMPARE(address->property("text").toString(), QString());
    QQuickItem* warning = inWindow(webDav, QStringLiteral("plainHttpWarning"));
    QVERIFY(address && warning);
    address->setProperty("text", QStringLiteral("http://example.org/dav"));
    QVERIFY(warning->isVisible());
    address->setProperty("text", server.folder().toString());
    QVERIFY(!warning->isVisible());
    inWindow(webDav, QStringLiteral("webDavUserField"))->setProperty("text", server.user);
    inWindow(webDav, QStringLiteral("webDavPasswordField"))->setProperty("text", server.password);

    // The server's certificate is not trusted here: its fingerprint is shown,
    // and trusting it connects.
    // One button: Connect until connected (the owner, 0.3.63).
    QQuickItem* goButton = inWindow(webDav, QStringLiteral("webDavGoButton"));
    QVERIFY(goButton);
    QCOMPARE(goButton->property("text").toString(), QStringLiteral("Connect"));
    click(goButton);
    QTRY_VERIFY_WITH_TIMEOUT(!webDav->property("certificate").toString().isEmpty(), 10000);
    QCOMPARE(webDav->property("certificate").toString().remove(QLatin1Char(':')).toLower().toLatin1(),
             WebDavTestServer::certificateFingerprint());
    click(inWindow(webDav, QStringLiteral("trustCertificateButton")));
    QTRY_VERIFY_WITH_TIMEOUT(webDav->property("connected").toBool(), 10000);
    // Connected: the WebDAV file dialog opens by itself, the only window seen
    // (the owner, D-082): a new folder, shown at once, Back, Up.
    QObject* files = webDav->findChild<QObject*>(QStringLiteral("webDavFileDialog"));
    QVERIFY(files);
    QTRY_VERIFY_WITH_TIMEOUT(files->property("visible").toBool(), 10000);
    QCOMPARE(goButton->property("text").toString(), QStringLiteral("Browse…"));
    QVERIFY(!inWindow(webDav, QStringLiteral("webDavChosenField")));
    // As tall as its content, whatever height it had.
    auto* webDavWindow = qobject_cast<QQuickWindow*>(webDav);
    QCOMPARE(webDavWindow->height(), webDavWindow->minimumHeight());
    click(inWindow(files, QStringLiteral("fileDialogNewFolderButton")));
    inWindow(files, QStringLiteral("fileDialogFolderNameField"))->setProperty("text", QStringLiteral("Work vaults"));
    click(inWindow(files, QStringLiteral("fileDialogCreateFolderButton")));
    QTRY_VERIFY_WITH_TIMEOUT(files->property("folderShown").toString().endsWith(QStringLiteral("/Work vaults/")), 10000);
    QVERIFY(inWindow(files, QStringLiteral("fileDialogUpButton"))->isEnabled());
    click(inWindow(files, QStringLiteral("fileDialogBackButton")));
    QTRY_VERIFY_WITH_TIMEOUT(files->property("folderShown").toString() == files->property("serverRoot").toString(), 10000);
    QVERIFY(!inWindow(files, QStringLiteral("fileDialogUpButton"))->isEnabled());
    // A folder the server refuses: Back gains no step for the folder that
    // was not made.
    const qsizetype steps = files->property("history").toList().size();
    click(inWindow(files, QStringLiteral("fileDialogNewFolderButton")));
    inWindow(files, QStringLiteral("fileDialogFolderNameField"))->setProperty("text", QStringLiteral("Work vaults"));
    click(inWindow(files, QStringLiteral("fileDialogCreateFolderButton")));
    QTRY_VERIFY_WITH_TIMEOUT(!files->property("problem").toString().isEmpty(), 10000);
    QCOMPARE(files->property("history").toList().size(), steps);
    // Cancel in the file dialog ends it all (the owner, D-082).
    QVERIFY(QMetaObject::invokeMethod(files, "reject"));
    QTRY_VERIFY(!files->property("visible").toBool());
    QTRY_VERIFY(!webDav->property("visible").toBool());

    // Again: the vault named in the file dialog goes on at once to its master
    // password, as for a file here.
    QVERIFY(QMetaObject::invokeMethod(window, "newVault"));
    QTRY_VERIFY(choice->property("visible").toBool());
    click(inWindow(choice, QStringLiteral("webDavTile")));
    QTRY_VERIFY(webDav->property("visible").toBool());
    address->setProperty("text", server.folder().toString());
    inWindow(webDav, QStringLiteral("webDavUserField"))->setProperty("text", server.user);
    inWindow(webDav, QStringLiteral("webDavPasswordField"))->setProperty("text", server.password);
    click(goButton);
    QTRY_VERIFY_WITH_TIMEOUT(!webDav->property("certificate").toString().isEmpty(), 10000);
    click(inWindow(webDav, QStringLiteral("trustCertificateButton")));
    QTRY_VERIFY_WITH_TIMEOUT(files->property("visible").toBool(), 10000);
    QVERIFY(QMetaObject::invokeMethod(files, "go", Q_ARG(QVariant, QStringLiteral("Work vaults")), Q_ARG(QVariant, true)));
    QTRY_VERIFY_WITH_TIMEOUT(files->property("folderShown").toString().endsWith(QStringLiteral("/Work vaults/")), 10000);
    inWindow(files, QStringLiteral("fileDialogNameField"))->setProperty("text", QStringLiteral("Home"));
    click(inWindow(files, QStringLiteral("fileDialogChooseButton")));
    QTRY_VERIFY(!files->property("visible").toBool());
    QObject* masterPassword = window->findChild<QObject*>(QStringLiteral("newVaultDialog"));
    QVERIFY(masterPassword);
    QTRY_VERIFY(masterPassword->property("visible").toBool());
    QCOMPARE(masterPassword->property("webDavName").toString(), QStringLiteral("Home"));
    inWindow(masterPassword, QStringLiteral("newVaultPassword"))->setProperty("text", QStringLiteral("pass"));
    inWindow(masterPassword, QStringLiteral("newVaultPasswordAgain"))->setProperty("text", QStringLiteral("pass"));
    QVERIFY(QMetaObject::invokeMethod(masterPassword, "accept"));
    QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!server.fileData(QStringLiteral("Work vaults/Home.chronovault")).isEmpty(), 10000);
    QVERIFY(controller->onWebDav());

    // The left column says where it is kept; Tools > Sync Now is there.
    // Locked, the unlock page offers this vault, chosen, and asks its own
    // master password (0.3.64).
    controller->lock();
    QQuickItem* vaultBox = inWindow(window, QStringLiteral("unlockVaultBox"));
    QQuickItem* masterField = inWindow(window, QStringLiteral("masterPasswordField"));
    QVERIFY(vaultBox && masterField);
    QTRY_VERIFY(vaultBox->isVisible());
    QTRY_COMPARE(vaultBox->property("currentIndex").toInt(), 0);
    QCOMPARE(masterField->property("placeholderText").toString(), QStringLiteral("Master Password"));
    controller->unlock(QStringLiteral("pass"));
    QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);
    QQuickItem* place = inWindow(window, QStringLiteral("vaultPlace"));
    QVERIFY(place);
    QCOMPARE(place->property("text").toString(), QStringLiteral("WebDAV"));
    QObject* syncNow = window->findChild<QObject*>(QStringLiteral("syncNowAction"));
    QVERIFY(syncNow && syncNow->property("enabled").toBool());

    // Settings > Vaults: kept on WebDAV, last synchronized; its menu shows
    // only what can be done now; Disconnect… keeps it here and deletes
    // nothing (D-060).
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    settings->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 2);
    QTRY_VERIFY(inWindow(settings, QStringLiteral("vaultMenuButton_Home")));
    const auto entryOf = [settings](const QString& name) -> QObject* {
        QQuickItem* button = inWindow(settings, QStringLiteral("vaultMenuButton_Home"));
        QObject* menu = button ? button->findChild<QObject*>(QStringLiteral("vaultMenu_Home")) : nullptr;
        return menu ? menu->findChild<QObject*>(name) : nullptr;
    };
    // An entry is looked at with the menu open, as a person sees it.
    const auto shown = [settings](const QString& name) {
        QQuickItem* button = inWindow(settings, QStringLiteral("vaultMenuButton_Home"));
        QObject* menu = button ? button->findChild<QObject*>(QStringLiteral("vaultMenu_Home")) : nullptr;
        QObject* entry = menu ? menu->findChild<QObject*>(name) : nullptr;
        if (!menu || !entry) {
            return false;
        }
        if (!menu->property("opened").toBool()) {
            (void)QMetaObject::invokeMethod(inWindow(settings, QStringLiteral("vaultMenuButton_Home")), "clicked");
            (void)QTest::qWaitFor([menu]() { return menu->property("opened").toBool(); }, 5000);
        }
        const bool visible = entry->property("visible").toBool();
        (void)QMetaObject::invokeMethod(menu, "close");
        (void)QTest::qWaitFor([menu]() { return !menu->property("visible").toBool(); }, 5000);
        return visible;
    };
    QVERIFY(shown(QStringLiteral("closeVault_Home")));
    QVERIFY(!shown(QStringLiteral("lockVault_Home")));
    QVERIFY(shown(QStringLiteral("moveVault_Home")));
    QVERIFY(!shown(QStringLiteral("forgetVault_Home"))); // the primary
    QVERIFY(shown(QStringLiteral("disconnect_Home")));
    QVERIFY(shown(QStringLiteral("importVault_Home")));
    QVERIFY(shown(QStringLiteral("exportVault_Home")));
    QVERIFY(QMetaObject::invokeMethod(entryOf(QStringLiteral("disconnect_Home")), "triggered"));
    QObject* disconnectWindow = settings->findChild<QObject*>(QStringLiteral("disconnectWindow"));
    QVERIFY(disconnectWindow);
    QTRY_VERIFY(disconnectWindow->property("visible").toBool());
    QVERIFY(!settings->findChild<QObject*>(QStringLiteral("alsoDeleteBox")));
    // The storage and the vault are named; nothing is deleted (D-060).
    const QString said = inWindow(disconnectWindow, QStringLiteral("dialogText"))->property("text").toString();
    QVERIFY2(said.contains(QStringLiteral("stored in a remote location, like WebDAV, so you")), qPrintable(said));
    QVERIFY2(said.contains(QStringLiteral("nothing is deleted")), qPrintable(said));
    // Three paragraphs, the last one the question (D-082).
    QCOMPARE(said.count(QStringLiteral("<p ")), 3);
    QVERIFY2(said.endsWith(QStringLiteral("This action cannot be undone. Are you sure you want to disconnect from “Home”?</p>")),
             qPrintable(said));
    click(inWindow(disconnectWindow, QStringLiteral("disconnectButton")));
    QTRY_VERIFY_WITH_TIMEOUT(!controller->onWebDav(), 10000);
    QTRY_VERIFY(!disconnectWindow->property("visible").toBool());
    QVERIFY(server.files.contains(QStringLiteral("Work vaults/Home.chronovault")));
    QTRY_VERIFY(!shown(QStringLiteral("disconnect_Home")));
    QVERIFY(shown(QStringLiteral("moveVault_Home")));
    QTRY_COMPARE(place->property("text").toString(), QStringLiteral("Local file"));

    // Close from the menu, then Lock; then Lock is no longer offered.
    QVERIFY(QMetaObject::invokeMethod(entryOf(QStringLiteral("closeVault_Home")), "triggered"));
    QTRY_COMPARE(controller->state(), State::Locked);
    QTRY_VERIFY(shown(QStringLiteral("lockVault_Home")));
    QVERIFY(QMetaObject::invokeMethod(entryOf(QStringLiteral("lockVault_Home")), "triggered"));
    QTRY_COMPARE(controller->state(), State::Locked);
    QTRY_VERIFY(!shown(QStringLiteral("lockVault_Home")));
    click(settings->findChild<QObject*>(QStringLiteral("cancelButton")));

    QString warningText;
    for (const QQmlError& one : std::as_const(qmlWarnings)) {
        warningText += one.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

QTEST_MAIN(TestWebDavWindows)
#include "test_webdav_windows.moc"
