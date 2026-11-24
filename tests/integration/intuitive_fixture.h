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

#pragma once

#include "app_setup.h"
#include "item_list_model.h"
#include "settings_files.h"
#include "vault_controller.h"
#include "vault_file.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// intuitive_fixture.h 0.5.83

// --- Version history ----------------------------------------------------
// v0.5.83: Initial release, out of test_intuitive.cpp (D-063)

using namespace chrono_vault;
using State = VaultController::State;



inline QQuickItem* seen(QQuickItem* root, const QString& name)
{
    if (!root) {
        return nullptr;
    }
    if (root->objectName() == name && root->isVisible()) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = seen(child, name)) {
            return found;
        }
    }
    return nullptr;
}

// The program with an unlocked vault of 31 items, by mouse and keys.
class IntuitiveFixture : public QObject
{
    Q_OBJECT

protected:
    QTemporaryDir folder;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow* window = nullptr;
    VaultController* controller = nullptr;
    QObject* page = nullptr;
    QQuickItem* list = nullptr;
    QList<QQmlError> problems;

    QQuickItem* row(int index)
    {
        QQuickItem* found = nullptr;
        QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem*, found), Q_ARG(int, index));
        return found;
    }
    void click(QQuickItem* item, Qt::KeyboardModifiers held = {}, Qt::MouseButton with = Qt::LeftButton)
    {
        QVERIFY(item);
        QTest::mouseClick(item->window(), with, held, item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
    }
    void key(int code, Qt::KeyboardModifiers held = {}) { QTest::keyClick(window, Qt::Key(code), held); }
    void type(const QString& text)
    {
        for (const QChar letter : text) {
            QTest::keyClick(window, letter.toLatin1());
        }
    }
    int current() const { return list->property("currentIndex").toInt(); }
    QStringList chosen() const { return page->property("selectedIds").toStringList(); }
    QQuickItem* part(const char* name) { return seen(window->contentItem(), QString::fromLatin1(name)); }
    QQuickWindow* dialog(const char* name) const
    {
        return qobject_cast<QQuickWindow*>(window->findChild<QObject*>(QString::fromLatin1(name)));
    }
    bool comesUp(QQuickWindow* one) const
    {
        return one && QTest::qWaitFor([one]() { return one->isVisible() && QGuiApplication::focusWindow() == one; }, 5000);
    }
    bool inView(int index)
    {
        QQuickItem* one = row(index);
        if (!one) {
            return false;
        }
        const double y = one->mapToItem(list, QPointF(0, 0)).y();
        return y >= -1 && y + one->height() <= list->height() + 1;
    }
    QString idAt(int index) const { return controller->items()->idAt(index); }

protected slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
        QCoreApplication::setApplicationName(QString::fromLatin1(metaObject()->className()));
        applyStyle();
    }
    void init()
    {
        SettingsFile().clear();
        VaultsFile().clear();
        engine = std::make_unique<QQmlApplicationEngine>();
        problems.clear();
        connect(engine.get(), &QQmlEngine::warnings, this, [this](const QList<QQmlError>& some) { problems += some; });
        engine->loadFromModule("ChronoVault", "Main");
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst());
        showMainWindow(window, false, false);
        controller = engine->singletonInstance<VaultController*>("ChronoVault", "VaultController");
        controller->setKdfParameters(crypto::KdfParameters{1024, 1, 1});
        const QString file = folder.filePath(QString::fromLatin1(QTest::currentTestFunction()) + QStringLiteral(".chronovault"));
        controller->createVault(QUrl::fromLocalFile(file), QStringLiteral("pass"), QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        QTRY_VERIFY(controller->isIdle());
        const OpenResult opened = openVaultFile(file, "pass");
        VaultContents contents = contentsFromJson(opened.contents).contents;
        for (int number = 0; number <= 30; ++number) {
            Item item;
            item.id = QUuid::createUuid();
            item.title = number == 30 ? QStringLiteral("Mill") : QStringLiteral("Item %1").arg(number, 2, 10, QLatin1Char('0'));
            item.created = item.modified = QDateTime::currentDateTimeUtc();
            contents.items.append(item);
        }
        QCOMPARE(saveVaultFile(file, contentsToJson(contents), opened.key), SaveStatus::Saved);
        controller->lock();
        controller->unlock(QStringLiteral("pass"));
        QTRY_COMPARE(controller->state(), State::Unlocked);
        page = window->findChild<QObject*>(QStringLiteral("vaultPage"));
        list = window->findChild<QQuickItem*>(QStringLiteral("itemList"));
        window->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), window);
        QTRY_VERIFY(row(3));
    }
    void cleanup()
    {
        QString said;
        for (const QQmlError& problem : std::as_const(problems)) {
            said += problem.toString() + QLatin1Char('\n');
        }
        engine.reset();
        SettingsFile().clear();
        VaultsFile().clear();
        QVERIFY2(said.isEmpty(), qPrintable(said));
    }
};
