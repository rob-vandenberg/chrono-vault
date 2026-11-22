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

#include "single_instance.h"
#include "window_memory.h"

#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include "settings_files.h"
#include <QSignalSpy>
#include <QStandardPaths>
#include <QThread>
#include <QWindow>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_windows.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.11: A window past its screen's edge is moved back inside
// v0.0.4: Initial release

using namespace chrono_vault;

// One Chrono Vault per person, and windows that remember (D-020).
class TestWindows : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void a_second_start_is_turned_away_and_asks_the_first_to_come_forward();
    void a_name_left_behind_does_not_block_a_new_start();
    void a_window_comes_back_at_its_size_and_place();
    void a_maximized_window_is_remembered_as_maximized();
    void a_place_on_no_screen_is_moved_onto_one();
    void a_window_past_its_screens_edge_is_moved_back_inside();

private:
    const QString name = QStringLiteral("chrono-vault-test-windows");
};

void TestWindows::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_windows"));
    SettingsFile().clear();
    VaultsFile().clear();
}

void TestWindows::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestWindows::a_second_start_is_turned_away_and_asks_the_first_to_come_forward()
{
    // The running Chrono Vault answers on a thread of its own, as a separate
    // program would, so the later start's waiting cannot block it.
    QThread running;
    auto* first = new SingleInstance(name);
    QVERIFY(first->becomeTheOnlyOne());
    int asked = 0;
    connect(first, &SingleInstance::showRequested, this, [&asked]() { ++asked; }, Qt::QueuedConnection);
    first->moveToThread(&running);
    running.start();

    SingleInstance second(name);
    QVERIFY(!second.becomeTheOnlyOne());
    QTRY_COMPARE(asked, 1);

    // Gone before this test ends, its connection closed (OR-11).
    QMetaObject::invokeMethod(first, [first]() { delete first; }, Qt::BlockingQueuedConnection);
    running.quit();
    QVERIFY(running.wait(5000));
}

void TestWindows::a_name_left_behind_does_not_block_a_new_start()
{
    {
        SingleInstance gone(name);
        QVERIFY(gone.becomeTheOnlyOne());
    }
    SingleInstance next(name);
    QVERIFY(next.becomeTheOnlyOne());
}

void TestWindows::a_window_comes_back_at_its_size_and_place()
{
    const QRect place(60, 50, 420, 300);
    {
        QWindow window;
        WindowMemory memory;
        memory.setName(QStringLiteral("test"));
        memory.setWindow(&window);
        memory.restore();
        window.setGeometry(place);
        window.show();
        QTRY_VERIFY(window.isVisible());
        window.hide(); // saves at once
    }
    QWindow again;
    WindowMemory memory;
    memory.setName(QStringLiteral("test"));
    memory.setWindow(&again);
    QVERIFY(memory.hasSaved());
    memory.restore();
    QCOMPARE(again.geometry().size(), place.size());
    QCOMPARE(again.geometry().topLeft(), place.topLeft());
}

void TestWindows::a_maximized_window_is_remembered_as_maximized()
{
    QWindow window;
    WindowMemory memory;
    memory.setName(QStringLiteral("maximized"));
    memory.setWindow(&window);
    memory.restore();
    window.showMaximized();
    QTRY_VERIFY(window.isVisible());
    window.hide();
    QCOMPARE(WindowMemory::savedState(QStringLiteral("maximized")), Qt::WindowMaximized);
    QCOMPARE(WindowMemory::savedState(QStringLiteral("never seen")), Qt::WindowNoState);
}

void TestWindows::a_place_on_no_screen_is_moved_onto_one()
{
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    const QRect moved = WindowMemory::onAScreen(QRect(screen.right() + 5000, screen.bottom() + 5000, 300, 200));
    QVERIFY(screen.contains(moved.center()));
    QCOMPARE(moved.size(), QSize(300, 200));

    const QRect huge = WindowMemory::onAScreen(QRect(screen.right() + 10, 0, screen.width() * 3, screen.height() * 3));
    QVERIFY(huge.width() <= screen.width() && huge.height() <= screen.height());

    const QRect fine(screen.topLeft() + QPoint(10, 10), QSize(200, 150));
    QCOMPARE(WindowMemory::onAScreen(fine), fine);
}

void TestWindows::a_window_past_its_screens_edge_is_moved_back_inside()
{
    // Remembered near the bottom right, then made larger: moved back inside,
    // its size kept.
    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    const QRect past(screen.right() - 300, screen.bottom() - 200, 555, 440);
    const QRect inside = WindowMemory::insideItsScreen(past);
    QCOMPARE(inside.size(), past.size());
    QVERIFY(screen.contains(inside));
    // A place already inside stays where it is.
    const QRect fine(screen.topLeft() + QPoint(10, 10), QSize(200, 150));
    QCOMPARE(WindowMemory::insideItsScreen(fine), fine);
}

QTEST_MAIN(TestWindows)
#include "test_windows.moc"
