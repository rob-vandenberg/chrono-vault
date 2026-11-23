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
#include "settings_files.h"
#include "vault_controller.h"
#include "webdav_sync.h"
#include "webdav_test_server.h"

#include <QDir>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_webdav_windows.cpp 0.3.63

// --- Version history ----------------------------------------------------
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
};

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
    QCOMPARE(goButton->property("text").toString(), QStringLiteral("Next…"));
    QVERIFY(!goButton->isEnabled());
    // As tall as its content, whatever height it had.
    auto* webDavWindow = qobject_cast<QQuickWindow*>(webDav);
    QCOMPARE(webDavWindow->height(), webDavWindow->minimumHeight());

    // Browse… opens the WebDAV file dialog: a new folder, shown at once,
    // Back, Up, then the name and Save.
    click(inWindow(webDav, QStringLiteral("webDavBrowseButton")));
    QObject* files = webDav->findChild<QObject*>(QStringLiteral("webDavFileDialog"));
    QVERIFY(files);
    QTRY_VERIFY(files->property("visible").toBool());
    click(inWindow(files, QStringLiteral("fileDialogNewFolderButton")));
    inWindow(files, QStringLiteral("fileDialogFolderNameField"))->setProperty("text", QStringLiteral("Work vaults"));
    click(inWindow(files, QStringLiteral("fileDialogCreateFolderButton")));
    QTRY_VERIFY_WITH_TIMEOUT(files->property("folderShown").toString().endsWith(QStringLiteral("/Work vaults/")), 10000);
    QVERIFY(inWindow(files, QStringLiteral("fileDialogUpButton"))->isEnabled());
    click(inWindow(files, QStringLiteral("fileDialogBackButton")));
    QTRY_VERIFY_WITH_TIMEOUT(files->property("folderShown").toString() == files->property("serverRoot").toString(), 10000);
    QVERIFY(!inWindow(files, QStringLiteral("fileDialogUpButton"))->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(files, "go", Q_ARG(QVariant, QStringLiteral("Work vaults")), Q_ARG(QVariant, true)));
    QTRY_VERIFY_WITH_TIMEOUT(files->property("folderShown").toString().endsWith(QStringLiteral("/Work vaults/")), 10000);
    inWindow(files, QStringLiteral("fileDialogNameField"))->setProperty("text", QStringLiteral("Home"));
    click(inWindow(files, QStringLiteral("fileDialogChooseButton")));
    QTRY_VERIFY(!files->property("visible").toBool());
    QVERIFY(inWindow(webDav, QStringLiteral("webDavChosenField"))->property("text").toString().endsWith(
        QStringLiteral("/Work vaults/Home.chronovault")));
    QVERIFY(goButton->isEnabled());

    // Next…, then the master password, as for a file here.
    click(goButton);
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
    // Locked, the unlock page names the server, not the copy here.
    controller->lock();
    QQuickItem* unlockPlace = inWindow(window, QStringLiteral("unlockVaultPlace"));
    QVERIFY(unlockPlace);
    QTRY_VERIFY(unlockPlace->property("text").toString().endsWith(QStringLiteral("/Work vaults/Home.chronovault")));
    controller->unlock(QStringLiteral("pass"));
    QTRY_VERIFY_WITH_TIMEOUT(controller->state() == State::Unlocked, 10000);
    QQuickItem* place = inWindow(window, QStringLiteral("vaultPlace"));
    QVERIFY(place);
    QCOMPARE(place->property("text").toString(), QStringLiteral("WebDAV"));
    QObject* syncNow = window->findChild<QObject*>(QStringLiteral("syncNowAction"));
    QVERIFY(syncNow && syncNow->property("enabled").toBool());

    // Settings > Vaults: kept on WebDAV, last synchronized; Disconnect… keeps
    // it here.
    QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    settings->findChild<QObject*>(QStringLiteral("topics"))->setProperty("currentIndex", 5);
    QQuickItem* disconnect = inWindow(settings, QStringLiteral("disconnect_Home"));
    QTRY_VERIFY(disconnect && disconnect->isVisible());
    click(disconnect);
    QObject* disconnectWindow = window->findChild<QObject*>(QStringLiteral("disconnectWindow"));
    if (!disconnectWindow) {
        disconnectWindow = settings->findChild<QObject*>(QStringLiteral("disconnectWindow"));
    }
    QVERIFY(disconnectWindow);
    QTRY_VERIFY(disconnectWindow->property("visible").toBool());
    click(inWindow(disconnectWindow, QStringLiteral("disconnectButton")));
    QTRY_VERIFY_WITH_TIMEOUT(!controller->onWebDav(), 10000);
    QTRY_VERIFY(!disconnectWindow->property("visible").toBool());
    QVERIFY(server.files.contains(QStringLiteral("Work vaults/Home.chronovault")));
    QQuickItem* move = inWindow(settings, QStringLiteral("moveToWebDav_Home"));
    QTRY_VERIFY(move && move->isVisible());
    QTRY_COMPARE(place->property("text").toString(), QStringLiteral("Local file"));
    click(settings->findChild<QObject*>(QStringLiteral("cancelButton")));

    QString warningText;
    for (const QQmlError& one : std::as_const(qmlWarnings)) {
        warningText += one.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

QTEST_MAIN(TestWebDavWindows)
#include "test_webdav_windows.moc"
