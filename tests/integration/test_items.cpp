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
#include "vault_controller.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_items.cpp 0.0.17

// --- Version history ----------------------------------------------------
// v0.0.17: Initial release: field history, sections and hidden fields kept, Trash and Archive, two settings (D-033)

using namespace chrono_vault;
using State = VaultController::State;

namespace {

QVariantMap fieldWithRole(const QVariantMap& item, const QString& role)
{
    for (const QVariant& field : item.value(QStringLiteral("fields")).toList()) {
        if (field.toMap().value(QStringLiteral("role")).toString() == role) {
            return field.toMap();
        }
    }
    return {};
}

QVariantMap withValue(QVariantMap item, const QString& role, const QString& value)
{
    QVariantList fields = item.value(QStringLiteral("fields")).toList();
    for (QVariant& field : fields) {
        QVariantMap map = field.toMap();
        if (map.value(QStringLiteral("role")).toString() == role) {
            map.insert(QStringLiteral("value"), value);
            field = map;
        }
    }
    item.insert(QStringLiteral("fields"), fields);
    return item;
}

QStringList titles(VaultController& controller)
{
    QStringList list;
    for (int row = 0; row < controller.items()->rowCount(); ++row) {
        list << controller.items()->data(controller.items()->index(row), ItemListModel::TitleRole).toString();
    }
    return list;
}

} // namespace

// What an item holds since 0.0.17, kept through saves and locks.
class TestItems : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void a_changed_value_goes_into_the_field_history();
    void sections_hidden_deleted_fields_and_the_items_own_parts_are_kept();
    void the_trash_and_the_archive_keep_their_items_apart();
    void hide_sensitive_fields_and_show_subtitles_are_kept();

private:
    std::unique_ptr<VaultController> unlocked();
    void lockAndUnlock(VaultController& controller);
    QTemporaryDir* folder = nullptr;
};

void TestItems::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_items"));
}

void TestItems::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestItems::init()
{
    QSettings().clear();
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
}

void TestItems::cleanup()
{
    QSettings().clear();
    delete folder;
    folder = nullptr;
}

std::unique_ptr<VaultController> TestItems::unlocked()
{
    auto controller = std::make_unique<VaultController>();
    controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
    controller->createVault(QUrl::fromLocalFile(folder->filePath(QStringLiteral("items.chronovault"))),
                            QStringLiteral("pass"), QStringLiteral("pass"));
    if (!QTest::qWaitFor([&controller]() { return controller->state() == State::Unlocked; }, 10000)) {
        return nullptr;
    }
    return controller;
}

void TestItems::lockAndUnlock(VaultController& controller)
{
    QTRY_VERIFY(controller.isIdle());
    controller.lock();
    controller.unlock(QStringLiteral("pass"));
    QTRY_COMPARE(controller.state(), State::Unlocked);
}

void TestItems::a_changed_value_goes_into_the_field_history()
{
    auto controller = unlocked();
    QVERIFY(controller);
    QVariantMap login = withValue(controller->newLogin(), QStringLiteral("password"), QStringLiteral("first"));
    const QString id = login.value(QStringLiteral("id")).toString();
    QVERIFY(controller->saveItem(login));
    QVERIFY(controller->saveItem(withValue(controller->item(id), QStringLiteral("password"), QStringLiteral("second"))));
    QVERIFY(controller->saveItem(controller->item(id))); // unchanged: nothing more in the history
    QVERIFY(controller->saveItem(withValue(controller->item(id), QStringLiteral("password"), QStringLiteral("third"))));
    lockAndUnlock(*controller);

    const QVariantMap password = fieldWithRole(controller->item(id), QStringLiteral("password"));
    QCOMPARE(password.value(QStringLiteral("value")).toString(), QStringLiteral("third"));
    QVERIFY(password.value(QStringLiteral("sensitive")).toBool());
    const QVariantList history = password.value(QStringLiteral("history")).toList();
    QCOMPARE(history.size(), 2);
    QCOMPARE(history.at(0).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("second")); // newest first
    QCOMPARE(history.at(1).toMap().value(QStringLiteral("value")).toString(), QStringLiteral("first"));
    QVERIFY(!history.at(0).toMap().value(QStringLiteral("changed")).toString().isEmpty());

    // A new item's fields have their ids before its first save.
    for (const QVariant& field : controller->newLogin().value(QStringLiteral("fields")).toList()) {
        QVERIFY(field.toMap().value(QStringLiteral("uid")).toLongLong() > 0);
    }
}

void TestItems::sections_hidden_deleted_fields_and_the_items_own_parts_are_kept()
{
    auto controller = unlocked();
    QVERIFY(controller);
    QVariantMap item = controller->newItemFrom(QStringLiteral("note"));
    QVariantList fields = item.value(QStringLiteral("fields")).toList();
    fields << QVariantMap{{"role", ""}, {"label", "Recovery"}, {"kind", "section"}, {"value", ""}}
           << QVariantMap{{"role", ""}, {"label", "Token"}, {"kind", "text"}, {"value", "t"}, {"sensitive", true}}
           << QVariantMap{{"role", ""}, {"label", "2FA"}, {"kind", "hidden"}, {"value", "x"}, {"hiddenType", ".twoFA"}}
           << QVariantMap{{"role", ""}, {"label", "Old"}, {"kind", "date"}, {"value", "1-1-2020"}, {"deleted", true}};
    item.insert(QStringLiteral("fields"), fields);
    item.insert(QStringLiteral("subtitle"), QStringLiteral("my own line"));
    item.insert(QStringLiteral("note"), QStringLiteral("a note"));
    item.insert(QStringLiteral("autoSubmit"), true);
    const QString id = item.value(QStringLiteral("id")).toString();
    QVERIFY(controller->saveItem(item));
    lockAndUnlock(*controller);

    const QVariantMap kept = controller->item(id);
    QCOMPARE(kept.value(QStringLiteral("subtitle")).toString(), QStringLiteral("my own line"));
    QCOMPARE(kept.value(QStringLiteral("note")).toString(), QStringLiteral("a note"));
    QVERIFY(kept.value(QStringLiteral("autoSubmit")).toBool());
    QStringList kinds;
    for (const QVariant& field : kept.value(QStringLiteral("fields")).toList()) {
        kinds << field.toMap().value(QStringLiteral("kind")).toString();
    }
    QCOMPARE(kinds, QStringList({"multiline", "section", "text", "hidden", "date"}));
    const QVariantList keptFields = kept.value(QStringLiteral("fields")).toList();
    QVERIFY(keptFields.at(2).toMap().value(QStringLiteral("sensitive")).toBool());
    QCOMPARE(keptFields.at(3).toMap().value(QStringLiteral("hiddenType")).toString(), QStringLiteral(".twoFA"));
    QVERIFY(keptFields.at(4).toMap().value(QStringLiteral("deleted")).toBool());
    QCOMPARE(controller->items()->data(controller->items()->index(0), ItemListModel::SubtitleRole).toString(),
             QStringLiteral("my own line"));
}

void TestItems::the_trash_and_the_archive_keep_their_items_apart()
{
    auto controller = unlocked();
    QVERIFY(controller);
    QVariantMap first = controller->newLogin();
    first.insert(QStringLiteral("title"), QStringLiteral("Kept"));
    QVariantMap second = controller->newLogin();
    second.insert(QStringLiteral("title"), QStringLiteral("Old"));
    QVariantMap third = controller->newLogin();
    third.insert(QStringLiteral("title"), QStringLiteral("Gone"));
    QVERIFY(controller->saveItem(first) && controller->saveItem(second) && controller->saveItem(third));
    controller->archiveItem(second.value(QStringLiteral("id")).toString(), true);
    controller->moveToTrash(third.value(QStringLiteral("id")).toString(), true);
    lockAndUnlock(*controller);

    QCOMPARE(titles(*controller), QStringList{QStringLiteral("Kept")});
    controller->setFilter(QStringLiteral("archive"));
    QCOMPARE(titles(*controller), QStringList{QStringLiteral("Old")});
    controller->setFilter(QStringLiteral("trash"));
    QCOMPARE(titles(*controller), QStringList{QStringLiteral("Gone")});
    controller->moveToTrash(third.value(QStringLiteral("id")).toString(), false); // restored
    QCOMPARE(titles(*controller), QStringList());
    controller->setFilter(QString());
    QCOMPARE(titles(*controller), QStringList({"Gone", "Kept"}));
    controller->deleteItem(third.value(QStringLiteral("id")).toString()); // for good
    QVERIFY(controller->item(third.value(QStringLiteral("id")).toString()).isEmpty());
}

void TestItems::hide_sensitive_fields_and_show_subtitles_are_kept()
{
    {
        VaultController controller;
        QVERIFY(controller.hideSensitive());
        QVERIFY(controller.showSubtitles());
        controller.setHideSensitive(false);
        controller.setShowSubtitles(false);
    }
    VaultController again;
    QVERIFY(!again.hideSensitive());
    QVERIFY(!again.showSubtitles());
    QVERIFY(again.settingsDefaults().value(QStringLiteral("hideSensitive")).toBool());
    QVERIFY(again.settingsDefaults().value(QStringLiteral("showSubtitles")).toBool());
}

QTEST_MAIN(TestItems)
#include "test_items.moc"
