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
#include "icon_library.h"
#include "settings_files.h"
#include "vault_controller.h"

#include <QFile>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_icon_picker.cpp 0.2.56

// --- Version history ----------------------------------------------------
// v0.2.56: Folders inside folders, "-" lines, shipped lines added (the owner, D-050)
// v0.0.40: Initial release (D-050)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;
using State = VaultController::State;

namespace {
// A small picture file of one colour.
QString picture(const QString& path, QColor colour)
{
    QImage image(32, 32, QImage::Format_ARGB32);
    image.fill(colour);
    return image.save(path) ? path : QString();
}
void writeText(const QString& path, const QByteArray& text)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text);
}
} // namespace

// The icon picker and the icons that ship (D-050).
class TestIconPicker : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QStringLiteral("test_icon_picker"));
        applyStyle();
        SettingsFile().clear();
        VaultsFile().clear();
    }
    void cleanupTestCase()
    {
        setIconsFoldersForTests(QString(), QString());
        SettingsFile().clear();
        VaultsFile().clear();
        QFile::remove(SettingsFile().fileName());
        QFile::remove(VaultsFile().fileName());
    }

    void icons_md_sets_the_sections_and_their_order()
    {
        QTemporaryDir icons;
        for (const char* name : {"web", "creditcard", "unlisted"}) {
            QVERIFY(QDir(icons.path()).mkdir(QString::fromLatin1(name)));
        }
        QVERIFY(!picture(icons.filePath(QStringLiteral("web/b.png")), Qt::red).isEmpty());
        QVERIFY(!picture(icons.filePath(QStringLiteral("web/a.png")), Qt::blue).isEmpty());
        writeText(icons.filePath(QStringLiteral("web/notes.txt")), "not a picture");
        writeText(icons.filePath(QStringLiteral("icons.md")),
                  "# The sections\n\ncreditcard: Credit Card\nweb:  Web \nmissing: Missing\n../up: Up\nno colon here\n");
        const QList<IconSection> sections = readIconSections(icons.path());
        QCOMPARE(sections.size(), 2); // a missing, an outside and an unlisted folder are left out
        QCOMPARE(sections[0].heading, QStringLiteral("Credit Card"));
        QCOMPARE(sections[1].folder, QStringLiteral("web"));
        QCOMPARE(sections[1].heading, QStringLiteral("Web"));
        QCOMPARE(sections[1].files.size(), 2);
        QVERIFY(sections[1].files[0].endsWith(QStringLiteral("a.png"))); // by name
        // No icons.md: no sections, nothing fails.
        QTemporaryDir empty;
        QVERIFY(readIconSections(empty.path()).isEmpty());
    }

    // A folder inside a folder is a section of its own, in the order of
    // icons.md; a line starting with "-" is a folder taken out (the owner).
    void folders_inside_folders_and_lines_taken_out()
    {
        QTemporaryDir icons;
        for (const char* name : {"symbols/uppercase", "symbols/digits", "web"}) {
            QVERIFY(QDir(icons.path()).mkpath(QString::fromLatin1(name)));
        }
        QVERIFY(!picture(icons.filePath(QStringLiteral("symbols/uppercase/A.png")), Qt::red).isEmpty());
        QVERIFY(!picture(icons.filePath(QStringLiteral("symbols/digits/0.png")), Qt::blue).isEmpty());
        QVERIFY(!picture(icons.filePath(QStringLiteral("web/w.png")), Qt::green).isEmpty());
        writeText(icons.filePath(QStringLiteral("icons.md")),
                  "symbols/uppercase: Uppercase\nsymbols/../web: Sneaky\n/web: Rooted\n- web: Web\nsymbols/digits: Digits\n");
        const QList<IconSection> sections = readIconSections(icons.path());
        QCOMPARE(sections.size(), 2);
        QCOMPARE(sections[0].folder, QStringLiteral("symbols/uppercase"));
        QCOMPARE(sections[1].heading, QStringLiteral("Digits"));
        QCOMPARE(sections[1].files.size(), 1);
    }

    // After an update, a folder that ships and that the person's icons.md
    // does not name is added at its end; their own lines, and a line they
    // took out, stay as they are (the owner).
    void shipped_lines_are_added_and_lines_taken_out_stay_out()
    {
        QTemporaryDir installed;
        QTemporaryDir home;
        const QString personal = home.filePath(QStringLiteral("chrono-vault/icons"));
        setIconsFoldersForTests(installed.path(), personal);
        for (const char* name : {"web", "creditcard", "symbols/digits"}) {
            QVERIFY(QDir(installed.path()).mkpath(QString::fromLatin1(name)));
        }
        writeText(installed.filePath(QStringLiteral("icons.md")),
                  "web: Web\ncreditcard: Credit Card\nsymbols/digits: Digits\n");
        QDir().mkpath(personal);
        writeText(personal + QStringLiteral("/icons.md"), "web: My sites\n- creditcard: Credit Card\n\n");
        QVERIFY(copyMissingIcons());
        QFile kept(personal + QStringLiteral("/icons.md"));
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), QByteArray("web: My sites\n- creditcard: Credit Card\nsymbols/digits: Digits\n"));
        kept.close();
        // A second start adds nothing more.
        QVERIFY(copyMissingIcons());
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), QByteArray("web: My sites\n- creditcard: Credit Card\nsymbols/digits: Digits\n"));
        setIconsFoldersForTests(QString(), QString());
    }

    void the_shipped_icons_are_copied_and_nothing_is_written_over()
    {
        QTemporaryDir installed;
        QTemporaryDir home;
        const QString personal = home.filePath(QStringLiteral("chrono-vault/icons"));
        setIconsFoldersForTests(installed.path(), personal);
        QVERIFY(QDir(installed.path()).mkdir(QStringLiteral("web")));
        writeText(installed.filePath(QStringLiteral("icons.md")), "web: Web\n");
        QVERIFY(!picture(installed.filePath(QStringLiteral("web/one.png")), Qt::red).isEmpty());
        QVERIFY(copyMissingIcons());
        QVERIFY(QFile::exists(personal + QStringLiteral("/web/one.png")));
        // The person changes the heading; a newer version adds an icon.
        writeText(personal + QStringLiteral("/icons.md"), "web: My sites\n");
        QVERIFY(!picture(installed.filePath(QStringLiteral("web/two.png")), Qt::blue).isEmpty());
        QVERIFY(copyMissingIcons());
        QVERIFY(QFile::exists(personal + QStringLiteral("/web/two.png")));
        QFile kept(personal + QStringLiteral("/icons.md"));
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), QByteArray("web: My sites\n"));
        // A heading without a translation is shown as written.
        VaultController controller;
        const QVariantList sections = controller.iconSections();
        QCOMPARE(sections.size(), 1);
        QCOMPARE(sections[0].toMap().value(QStringLiteral("heading")).toString(), QStringLiteral("My sites"));
        QCOMPARE(sections[0].toMap().value(QStringLiteral("icons")).toList().size(), 2);
        setIconsFoldersForTests(QString(), QString());
    }

    void the_vaults_own_icons_are_added_kept_and_deleted()
    {
        QTemporaryDir folder;
        VaultController controller;
        controller.setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        const QString vault = folder.filePath(QStringLiteral("icons.chronovault"));
        controller.createVault(QUrl::fromLocalFile(vault), QStringLiteral("a"), QStringLiteral("a"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        const QString red = controller.addCustomIcon(
            QUrl::fromLocalFile(picture(folder.filePath(QStringLiteral("red.png")), Qt::red)));
        const QString blue = controller.addCustomIcon(
            QUrl::fromLocalFile(picture(folder.filePath(QStringLiteral("blue.png")), Qt::blue)));
        QVERIFY(!red.isEmpty() && !blue.isEmpty());
        QCOMPARE(controller.customIcons(), (QStringList{red, blue}));
        // Not a picture: nothing added, and said.
        writeText(folder.filePath(QStringLiteral("text.png")), "no picture");
        QVERIFY(controller.addCustomIcon(QUrl::fromLocalFile(folder.filePath(QStringLiteral("text.png")))).isEmpty());
        // An item carries the red one.
        QVariantMap item = controller.newItemFrom(QStringLiteral("login"));
        item.insert(QStringLiteral("title"), QStringLiteral("Bank"));
        item.insert(QStringLiteral("icon"), red);
        const QString id = item.value(QStringLiteral("id")).toString();
        QVERIFY(controller.saveItem(item));
        QCOMPARE(controller.customIconUses(red), 1);
        QCOMPARE(controller.customIconUses(blue), 0);
        // Kept in the vault: there after a lock and an unlock.
        QTRY_VERIFY(!controller.unsavedChanges());
        controller.lock();
        controller.unlock(QStringLiteral("a"));
        QTRY_COMPARE(controller.state(), State::Unlocked);
        QCOMPARE(controller.customIcons(), (QStringList{red, blue}));
        // Deleted: gone from the list, and the item has its category's icon.
        controller.deleteCustomIcon(red);
        QCOMPARE(controller.customIcons(), QStringList{blue});
        QVERIFY(controller.item(id).value(QStringLiteral("icon")).toString().isEmpty());
        QTRY_VERIFY(!controller.unsavedChanges());
        controller.lock();
    }

    void the_editor_opens_the_picker_and_takes_its_choice()
    {
        QTemporaryDir folder;
        QList<QQmlError> qmlWarnings;
        QQmlApplicationEngine engine;
        QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                         [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
        engine.loadFromModule("ChronoVault", "Main");
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        showMainWindow(window, false, false);
        QVERIFY(QTest::qWaitForWindowExposed(window));
        auto* controller = engine.singletonInstance<VaultController*>("ChronoVault", "VaultController");
        controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        controller->createVault(QUrl::fromLocalFile(folder.filePath(QStringLiteral("editor.chronovault"))),
                                QStringLiteral("a"), QStringLiteral("a"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QObject* page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
        QObject* editor = window->findChild<QObject*>(QStringLiteral("itemEditor"));
        QVERIFY(page && editor);
        QVERIFY(QMetaObject::invokeMethod(page, "newItem", Q_ARG(QVariant, QStringLiteral("login"))));
        QTRY_VERIFY(page->property("editing").toBool());

        // Add at the left; Save, then Cancel, at the right (the owner).
        const auto left = [editor](const char* name) {
            auto* button = editor->findChild<QQuickItem*>(QString::fromLatin1(name));
            return button ? button->mapToScene(QPointF(0, 0)).x() : -1.0;
        };
        QTRY_VERIFY(left("addButton") >= 0);
        QVERIFY(left("addButton") < left("saveButton"));
        QVERIFY(left("saveButton") < left("cancelButton"));

        // A click anywhere on the icon, not only on the pencil, opens it.
        auto* icon = editor->findChild<QQuickItem*>(QStringLiteral("itemIcon"));
        auto* picker = qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QStringLiteral("iconPicker")));
        QVERIFY(icon && picker);
        const QPoint topLeft = icon->mapToScene(QPointF(icon->width() / 4, icon->height() / 4)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, topLeft);
        QTRY_VERIFY(picker->isVisible());

        // OK is greyed until an icon is chosen; the chosen one has the check.
        QObject* ok = picker->findChild<QObject*>(QStringLiteral("okButton"));
        QVERIFY(ok);
        QVERIFY(!ok->property("enabled").toBool());
        const QString blue = controller->addCustomIcon(
            QUrl::fromLocalFile(picture(folder.filePath(QStringLiteral("blue.png")), Qt::blue)));
        QVERIFY(QMetaObject::invokeMethod(picker, "reload"));
        picker->setProperty("chosen", QStringLiteral("custom:") + blue);
        QVERIFY(ok->property("enabled").toBool());
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QTRY_VERIFY(!picker->isVisible());
        QCOMPARE(editor->property("icon").toString(), blue);

        // "Default icon" gives back the category's own.
        QVERIFY(QMetaObject::invokeMethod(editor, "chooseIcon"));
        QTRY_VERIFY(picker->isVisible());
        QVERIFY(picker->findChild<QObject*>(QStringLiteral("defaultIconTile")));
        picker->setProperty("chosen", QStringLiteral("default"));
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QTRY_VERIFY(!picker->isVisible());
        QVERIFY(editor->property("icon").toString().isEmpty());

        // A deleted icon leaves the item being edited too, so a Save cannot
        // bring it back.
        QVERIFY(QMetaObject::invokeMethod(editor, "chooseIcon"));
        QTRY_VERIFY(picker->isVisible());
        picker->setProperty("chosen", QStringLiteral("custom:") + blue);
        QVERIFY(QMetaObject::invokeMethod(picker, "accept"));
        QTRY_COMPARE(editor->property("icon").toString(), blue);
        QVERIFY(QMetaObject::invokeMethod(picker, "deleteIcon", Q_ARG(QVariant, blue)));
        QVERIFY(editor->property("icon").toString().isEmpty());
        QVERIFY(!controller->customIcons().contains(blue));

        // Closed without a choice: the item keeps its icon.
        QVERIFY(QMetaObject::invokeMethod(editor, "chooseIcon"));
        QTRY_VERIFY(picker->isVisible());
        QVERIFY(QMetaObject::invokeMethod(picker, "close"));
        QTRY_VERIFY(!picker->isVisible());

        QString warningText;
        for (const QQmlError& warning : std::as_const(qmlWarnings)) {
            warningText += warning.toString() + QLatin1Char('\n');
        }
        QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
        controller->lock();
    }
};

QTEST_MAIN(TestIconPicker)
#include "test_icon_picker.moc"
