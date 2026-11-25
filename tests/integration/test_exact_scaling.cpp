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

#include "intuitive_fixture.h"
#include "intuitive_fixture.h"

#include "program_scale.h"

#include <QScopeGuard>

// --- Version ------------------------------------------------------------
// test_exact_scaling.cpp 0.6.94

// --- Version history ----------------------------------------------------
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.6.92: Initial release (D-069)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// Every size is its size at 100% times the UI scaling (D-069).
class TestExactScaling : public IntuitiveFixture
{
    Q_OBJECT

    QQuickWindow* settings() const { return window->findChild<QQuickWindow*>(QStringLiteral("settingsWindow")); }
    // Once everything that follows the scaling has followed it.
    static void settle(QQuickWindow* one)
    {
        QCoreApplication::processEvents();
        one->grabWindow();
        QCoreApplication::processEvents();
    }
    // Qt leaves the field's padding out of a letter's place.
    QPoint placeOf(QQuickItem* field, int letter) const
    {
        QQuickItem* typed = field->window()->activeFocusItem();
        QRectF at;
        QMetaObject::invokeMethod(typed, "positionToRectangle", Q_RETURN_ARG(QRectF, at), Q_ARG(int, letter));
        return typed->mapToScene(QPointF(at.x() + typed->property("leftPadding").toDouble(), at.center().y())).toPoint();
    }

private slots:
    void general_is_its_height_at_100_times_the_scaling()
    {
        const auto back = qScopeGuard([]() { applyProgramScale(100); });
        controller->setTrayAvailable(true);
        QMetaObject::invokeMethod(settings(), "showWindow");
        QTRY_VERIFY(settings()->isExposed());
        QQuickItem* scroll = settings()->findChild<QQuickItem*>(QStringLiteral("pageScroll"));
        QQuickItem* title = settings()->findChild<QQuickItem*>(QStringLiteral("topicNames"));
        QVERIFY(scroll && title);
        double height = 0;
        double width = 0;
        int lines = 0;
        // 100% first: the one measured.
        for (const int scale : {100, 50, 105, 110, 125, 200}) {
            applyProgramScale(scale);
            // Whatever the test's screen: the window the person has.
            settings()->setMinimumSize({1, 1});
            settings()->resize(qRound(7.2 * scale), qRound(6.4 * scale));
            settle(settings());
            const double is = scroll->property("contentHeight").toDouble();
            QQuickItem* remark = settings()->findChild<QQuickItem*>(QStringLiteral("websiteIconsRemark"));
            QVERIFY(remark);
            if (height == 0) {
                height = is;
                width = title->implicitWidth();
                lines = remark->property("lineCount").toInt();
                continue;
            }
            const QByteArray at = QByteArray::number(scale) + "%: " + QByteArray::number(is) + " for "
                                  + QByteArray::number(height * scale / 100);
            QVERIFY2(qAbs(is - height * scale / 100) <= 0.01, at.constData());
            QVERIFY2(is <= scroll->height(), at.constData());
            QVERIFY2(qAbs(title->implicitWidth() - width * scale / 100) <= 1, at.constData());
            QVERIFY2(remark->property("lineCount").toInt() == lines, at.constData());
        }
    }

    void a_field_is_typed_in_at_every_scaling()
    {
        const auto back = qScopeGuard([]() { applyProgramScale(100); });
        QQuickItem* search = part("searchField");
        QVERIFY(search);
        for (const int scale : {50, 105, 110, 125, 200}) {
            applyProgramScale(scale);
            window->setMinimumSize({1, 1});
            window->resize(qRound(9.6 * scale), qRound(6.4 * scale));
            settle(window);
            const QByteArray at = QByteArray::number(scale) + "%";
            QMetaObject::invokeMethod(search, "forceActiveFocus");
            QMetaObject::invokeMethod(search, "clear");
            QTRY_VERIFY2(search->hasActiveFocus(), at.constData());
            type(QStringLiteral("mill item"));
            QTRY_COMPARE(search->property("text").toString(), QStringLiteral("mill item"));
            settle(window);
            // A click lands on the letter under it; a drag takes the letters between.
            for (const int letter : {2, 5, 7}) {
                QTest::mouseClick(window, Qt::LeftButton, {}, placeOf(search, letter), 700);
                QVERIFY2(search->property("cursorPosition").toInt() == letter, at.constData());
            }
            const QPoint from = placeOf(search, 0) + QPoint(1, 0);
            const QPoint to = placeOf(search, 4);
            QTest::mousePress(window, Qt::LeftButton, {}, from, 700);
            for (int step = 1; step <= 5; ++step) {
                QTest::mouseMove(window, from + (to - from) * step / 5);
            }
            QTest::mouseRelease(window, Qt::LeftButton, {}, to);
            QVERIFY2(search->property("selectedText").toString() == QLatin1String("mill"), at.constData());
        }
        QMetaObject::invokeMethod(search, "clear");
    }

    // The main window keeps its size, every other window scales along (D-070).
    void the_main_window_keeps_its_size_and_another_scales_along()
    {
        const auto back = qScopeGuard([]() { applyProgramScale(100); });
        window->resize(700, 500);
        settle(window);
        QMetaObject::invokeMethod(settings(), "showWindow");
        QTRY_VERIFY(settings()->isExposed());
        settings()->resize(740, 700);
        settle(settings());
        applyProgramScale(105);
        settle(settings());
        QCOMPARE(window->size(), QSize(700, 500));
        QCOMPARE(settings()->size(), QSize(777, 735));
        applyProgramScale(50);
        settle(settings());
        QCOMPARE(window->size(), QSize(700, 500));
        QCOMPARE(settings()->size(), QSize(370, 350));
        QMetaObject::invokeMethod(settings(), "close");
        // Closed too: scaled at its next opening, and in the next run.
        applyProgramScale(105);
        QMetaObject::invokeMethod(settings(), "showWindow");
        QTRY_VERIFY(settings()->isExposed());
        QCOMPARE(settings()->size(), QSize(777, 735));
        QMetaObject::invokeMethod(settings(), "close");
        engine.reset();
        applyProgramScale(100);
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->loadFromModule("ChronoVault", "Main");
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst());
        QCOMPARE(settings()->size(), QSize(740, 700));
        QCOMPARE(window->size(), QSize(700, 500));
    }

    void a_number_is_typed_at_another_scaling()
    {
        const auto back = qScopeGuard([]() { applyProgramScale(100); });
        applyProgramScale(110);
        QMetaObject::invokeMethod(settings(), "showWindow");
        QTRY_VERIFY(settings()->isExposed());
        settings()->requestActivate();
        QTRY_COMPARE(QGuiApplication::focusWindow(), settings());
        settle(settings());
        QQuickItem* box = settings()->findChild<QQuickItem*>(QStringLiteral("clipboardBox"));
        QVERIFY(box);
        QTest::mouseClick(settings(), Qt::LeftButton, {}, box->mapToScene(QPointF(box->width() / 3, box->height() / 2)).toPoint());
        QTest::keyClick(settings(), Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(settings(), Qt::Key_4);
        QTest::keyClick(settings(), Qt::Key_5);
        QTest::keyClick(settings(), Qt::Key_Tab);
        QTRY_COMPARE(box->property("value").toInt(), 45);
        // Emptied, it keeps the number it had.
        QTest::mouseClick(settings(), Qt::LeftButton, {}, box->mapToScene(QPointF(box->width() / 3, box->height() / 2)).toPoint());
        QTest::keyClick(settings(), Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(settings(), Qt::Key_Delete);
        QTest::keyClick(settings(), Qt::Key_Tab);
        QTRY_COMPARE(box->property("value").toInt(), 45);
        // The arrows.
        QTest::mouseClick(settings(), Qt::LeftButton, {}, box->mapToScene(QPointF(box->width() - 4, 5)).toPoint());
        QTRY_COMPARE(box->property("value").toInt(), 46);
        QMetaObject::invokeMethod(settings(), "close");
    }
};

QTEST_MAIN(TestExactScaling)
#include "test_exact_scaling.moc"
