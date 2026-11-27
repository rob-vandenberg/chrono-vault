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
#include "program_scale.h"

#include <QQuickItem>
#include <QScreen>
#include <algorithm>

// --- Version ------------------------------------------------------------
// test_dialog_design.cpp 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: Initial release: every dialog in the one design, at 100% and 200% (D-082)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// Every dialog the program has, opened from a clean start: a dialog that fits
// its content is 440 wide and exactly as tall as its content; in every one the
// buttons of the bottom row are alike, and the last one is as far from the
// right edge as from the bottom (the owner, D-082). Built twice: at 100%, and
// with CHRONO_VAULT_DESIGN_200 at 200%, so each stays under 20 seconds (OR-11).

namespace {

#if defined(CHRONO_VAULT_DESIGN_200)
constexpr int scale = 200;
#else
constexpr int scale = 100;
#endif

bool isButton(const QQuickItem* item)
{
    return item->inherits("QQuickAbstractButton") && !item->inherits("QQuickCheckBox")
           && !item->inherits("QQuickRadioButton") && !item->inherits("QQuickSwitch");
}

void buttonsIn(QQuickItem* item, QList<QQuickItem*>& found)
{
    if (!item || !item->isVisible()) {
        return;
    }
    if (isButton(item) && item->width() > 0) {
        found.append(item);
        return;
    }
    for (QQuickItem* child : item->childItems()) {
        buttonsIn(child, found);
    }
}

QRectF placeOf(QQuickItem* item)
{
    return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
}

} // namespace

class TestDialogDesign : public IntuitiveFixture
{
    Q_OBJECT

    // What is wrong with one window, or nothing.
    QString faultsOf(QQuickWindow* one)
    {
        QStringList faults;
        const double factor = scale / 100.0;
        const double edge = 12 * factor;
        const bool fits = one->property("fitsContent").toBool();
        if (fits) {
            if (one->width() != qRound(440 * factor)) {
                faults << QStringLiteral("width %1").arg(one->width());
            }
            if (one->minimumSize() != one->maximumSize() || one->size() != one->minimumSize()) {
                faults << QStringLiteral("not fixed to its content");
            }
        }
        // The bottom row: the buttons whose bottom is the lowest.
        QList<QQuickItem*> buttons;
        buttonsIn(one->contentItem(), buttons);
        double lowest = 0;
        for (QQuickItem* button : std::as_const(buttons)) {
            lowest = std::max(lowest, placeOf(button).bottom());
        }
        QList<QQuickItem*> row;
        for (QQuickItem* button : std::as_const(buttons)) {
            if (std::abs(placeOf(button).bottom() - lowest) < 1) {
                row << button;
            }
        }
        if (row.isEmpty()) {
            return faults.join(QStringLiteral(", "));
        }
        double right = 0;
        for (QQuickItem* button : std::as_const(row)) {
            right = std::max(right, placeOf(button).right());
            if (std::abs(button->width() - row.constFirst()->width()) > 0.5
                || std::abs(button->height() - row.constFirst()->height()) > 0.5) {
                faults << QStringLiteral("buttons not alike");
                break;
            }
        }
        // A window the screen keeps smaller than its content is left aside.
        if (one->height() >= one->minimumHeight() && std::abs(one->height() - lowest - edge) > 1) {
            faults << QStringLiteral("bottom %1").arg(one->height() - lowest);
        }
        // A window the screen keeps narrower than its row is left aside.
        if (one->width() >= one->minimumWidth() && std::abs(one->width() - right - edge) > 1) {
            faults << QStringLiteral("right %1").arg(one->width() - right);
        }
        return faults.join(QStringLiteral(", "));
    }

private slots:
    void every_dialog_has_the_one_design()
    {
        applyProgramScale(scale);
        const auto back = qScopeGuard([]() { applyProgramScale(100); });
        QObject* settings = window->findChild<QObject*>(QStringLiteral("settingsWindow"));
        QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
        // The settings window itself, in the same edge and buttons.
        auto* settingsWindow = qobject_cast<QQuickWindow*>(settings);
        QTRY_VERIFY(settingsWindow->isExposed());
        const QString settingsFaults = faultsOf(settingsWindow);
        QVERIFY2(settingsFaults.isEmpty(), qPrintable(settingsFaults));
        QMetaObject::invokeMethod(settings, "close");
        QTRY_VERIFY(!settingsWindow->isVisible());
        applyProgramScale(scale); // Cancel shows the kept scale again
        QStringList wrong;
        int checked = 0;
        const QList<QQuickWindow*> windows = window->findChildren<QQuickWindow*>();
        for (QQuickWindow* one : windows) {
            if (one == window || one == settingsWindow || !one->property("fitsContent").isValid() || one->isVisible()) {
                continue;
            }
            QVERIFY(QMetaObject::invokeMethod(one, "open"));
            if (!QTest::qWaitFor([one]() { return one->isExposed(); }, 3000)) {
                wrong << one->objectName() + QStringLiteral(": not shown");
                continue;
            }
            // Laid out once shown: what is still wrong then is a fault.
            QString faults;
            (void)QTest::qWaitFor([&]() { faults = faultsOf(one); return faults.isEmpty(); }, 2000);
            if (!faults.isEmpty()) {
                wrong << one->objectName() + QStringLiteral(": ") + faults;
            }
            ++checked;
            QMetaObject::invokeMethod(one, "reject");
            one->hide();
            QTRY_VERIFY(!one->isVisible());
        }
        QVERIFY2(checked >= 45, qPrintable(QString::number(checked)));
        QVERIFY2(wrong.isEmpty(), qPrintable(wrong.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(TestDialogDesign)
#include "test_dialog_design.moc"
