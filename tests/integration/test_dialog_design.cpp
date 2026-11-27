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
#include <QSet>
#include <algorithm>

// --- Version ------------------------------------------------------------
// test_dialog_design.cpp 0.8.121

// --- Version history ----------------------------------------------------
// v0.8.121: 16 edge, 8 in a group, 28 high, groups, Discard left, Yes and No words, growth at the top (D-082)
// v0.8.120: Initial release: every dialog in the one design, at 100% and 200% (D-082)

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

// Every dialog the program has, opened from a clean start: a dialog that fits
// its content is 440 wide (WebDAV 580, wider by hand) and exactly as tall as its
// content; the edge 16 all round; buttons and fields 28 high, buttons at least
// 88 wide, 8 apart and alike inside a group; Discard at the far left; Yes and No
// show their words; a resizable one made taller keeps its content at the top
// and its buttons at the bottom (the owner, D-082). Built twice: at 100%, and
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

    // What is wrong with one window, or nothing. The settings window keeps
    // its own edge; its bottom row is checked as every dialog's.
    QString faultsOf(QQuickWindow* one, bool withEdge = true)
    {
        QStringList faults;
        const double factor = scale / 100.0;
        const double edge = 16 * factor;
        const double gap = 8 * factor;
        const double high = std::round(28 * factor);
        const bool fits = one->property("fitsContent").toBool();
        const bool wider = one->property("widthResizable").toBool();
        if (fits) {
            const double fitWidth = one->property("fitWidth").toDouble();
            if (one->minimumWidth() != qRound(fitWidth * factor)) {
                faults << QStringLiteral("width %1").arg(one->minimumWidth());
            }
            if (one->minimumHeight() != one->maximumHeight() || one->height() != one->minimumHeight()) {
                faults << QStringLiteral("not as tall as its content");
            }
            if (!wider && one->minimumWidth() != one->maximumWidth()) {
                faults << QStringLiteral("not fixed in width");
            }
            if (wider && one->maximumWidth() <= one->minimumWidth()) {
                faults << QStringLiteral("not wider by hand");
            }
        }
        // Fields and choices as high as the buttons.
        QList<QQuickItem*> all{one->contentItem()};
        for (int at = 0; at < all.size(); ++at) {
            for (QQuickItem* child : all[at]->childItems()) {
                if (child->isVisible()) {
                    all << child;
                }
            }
        }
        for (QQuickItem* item : std::as_const(all)) {
            // A field drawn as one: one inside another, without a
            // background of its own, takes the size of the one around it.
            if ((item->inherits("QQuickTextField") || item->inherits("QQuickComboBox")) && withEdge
                && item->property("background").value<QQuickItem*>() && std::abs(item->height() - high) > 0.5) {
                faults << QStringLiteral("a field %1 high").arg(item->height());
                break;
            }
        }
        // The bottom row: the buttons whose bottom is the lowest, left to right.
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
        std::sort(row.begin(), row.end(), [](QQuickItem* a, QQuickItem* b) { return placeOf(a).left() < placeOf(b).left(); });
        if (row.isEmpty()) {
            return faults.join(QStringLiteral(", "));
        }
        double right = 0;
        for (int i = 0; i < row.size(); ++i) {
            QQuickItem* button = row[i];
            right = std::max(right, placeOf(button).right());
            if (std::abs(button->height() - high) > 0.5 || button->width() < std::round(88 * factor) - 0.5) {
                faults << QStringLiteral("a button %1 by %2").arg(button->width()).arg(button->height());
            }
            if (button->property("text").toString().contains(QLatin1Char('&'))) {
                faults << QStringLiteral("an & shown");
            }
            // 8 between the buttons of a group, alike in width; more between
            // groups.
            if (i > 0) {
                const double apart = placeOf(button).left() - placeOf(row[i - 1]).right();
                if (apart < gap - 0.5) {
                    faults << QStringLiteral("buttons %1 apart").arg(apart);
                } else if (apart < gap + 0.5 && std::abs(button->width() - row[i - 1]->width()) > 0.5) {
                    faults << QStringLiteral("a group not alike");
                }
            }
        }
        // Discard apart at the far left (the owner, D-082).
        for (QQuickItem* button : std::as_const(row)) {
            if (button->objectName() == QLatin1String("discardButton") && std::abs(placeOf(button).left() - edge) > 1) {
                faults << QStringLiteral("Discard not at the left");
            }
        }
        if (!withEdge) {
            return faults.join(QStringLiteral(", "));
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

    // The top of what a window shows: the highest visible part inside it.
    static double topOf(QQuickWindow* one)
    {
        double top = 0;
        bool found = false;
        QList<QQuickItem*> all{seen(one->contentItem(), QStringLiteral("dialogContent"))};
        if (!all.constFirst()) {
            return 0;
        }
        for (int at = 0; at < all.size(); ++at) {
            for (QQuickItem* child : all[at]->childItems()) {
                if (child->isVisible() && child->width() > 0 && child->height() > 0) {
                    all << child;
                    if (child->childItems().isEmpty()) {
                        top = found ? std::min(top, placeOf(child).top()) : placeOf(child).top();
                        found = true;
                    }
                }
            }
        }
        return top;
    }

    // A resizable window made taller: its content stays at the top, the
    // buttons at the bottom; the room goes to the part that grows (D-082).
    QString growthFaultsOf(QQuickWindow* one)
    {
        if (one->property("fitsContent").toBool() || one->height() + 100 > one->screen()->availableGeometry().height()) {
            return {};
        }
        const double top = topOf(one);
        one->resize(one->width(), one->height() + 100);
        QString faults;
        (void)QTest::qWaitFor([&]() {
            faults = std::abs(topOf(one) - top) > 1 ? QStringLiteral("content not at the top") : faultsOf(one);
            return faults.isEmpty();
        }, 2000);
        one->resize(one->width(), one->height() - 100);
        return faults;
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
        const QString settingsFaults = faultsOf(settingsWindow, false);
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
            if (faults.isEmpty()) {
                faults = growthFaultsOf(one);
            }
            // Yes and No show their words (0.8.121: a "&" given while made
            // left them empty).
            if (faults.isEmpty() && one->property("asksYesNo").toBool()) {
                const QImage picture = one->grabWindow();
                for (const char* name : {"buttonBox_yes", "buttonBox_no"}) {
                    QVariant answer;
                    QMetaObject::invokeMethod(one, name, Q_RETURN_ARG(QVariant, answer));
                    QQuickItem* button = answer.value<QQuickItem*>();
                    if (!button) {
                        faults = QStringLiteral("no Yes or No");
                        break;
                    }
                    const QRect inside = placeOf(button).toRect().adjusted(4, 4, -4, -4);
                    QSet<QRgb> colours;
                    for (int y = inside.top(); y <= inside.bottom(); ++y) {
                        for (int x = inside.left(); x <= inside.right(); ++x) {
                            colours.insert(picture.pixel(x * picture.width() / one->width(), y * picture.height() / one->height()));
                        }
                    }
                    if (colours.size() < 4) {
                        faults = QStringLiteral("a Yes or No without words");
                    }
                }
            }
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
