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

#include <QFont>
#include <QPalette>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class QEvent;

// --- Version ------------------------------------------------------------
// appearance.h 0.0.10

// --- Version history ----------------------------------------------------
// v0.0.10: The program's colours for every window (D-023)
// v0.0.7: Initial release (D-023)

namespace chrono_vault {

// The theme and the UI scaling, as the windows see them (D-023): what is
// kept, what is shown while the settings window is open, and the text size
// fixed sizes are worked out from.
class Appearance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // What is kept: 0 light, 1 dark, 2 follow the system; and the percent.
    Q_PROPERTY(int theme READ theme NOTIFY keptChanged)
    Q_PROPERTY(int scale READ scale NOTIFY keptChanged)
    Q_PROPERTY(int smallestScale READ smallestScale CONSTANT)
    Q_PROPERTY(int largestScale READ largestScale CONSTANT)
    Q_PROPERTY(int scaleMarkEvery READ scaleMarkEvery CONSTANT)
    Q_PROPERTY(int scaleStep READ scaleStep CONSTANT)
    Q_PROPERTY(int defaultTheme READ defaultTheme CONSTANT)
    Q_PROPERTY(int defaultScale READ defaultScale CONSTANT)
    // The program's text size in points now: a heading is a multiple of it,
    // so it grows and shrinks with the scale.
    Q_PROPERTY(double fontPointSize READ fontPointSize NOTIFY fontChanged)
    // The UI scaling as a factor (1.0 is the desktop's own size): every size
    // a window sets itself is a multiple of it (D-023).
    Q_PROPERTY(double factor READ factor NOTIFY factorChanged)
    // The program's font now, for every window: Qt Quick's controls do not
    // take a changed program font by themselves.
    Q_PROPERTY(QFont font READ font NOTIFY fontChanged)

public:
    explicit Appearance(QObject* parent = nullptr);

    [[nodiscard]] int theme() const;
    [[nodiscard]] int scale() const;
    [[nodiscard]] static int smallestScale();
    [[nodiscard]] static int largestScale();
    [[nodiscard]] static int scaleMarkEvery();
    [[nodiscard]] static int scaleStep();
    [[nodiscard]] static int defaultTheme();
    [[nodiscard]] static int defaultScale();
    [[nodiscard]] double fontPointSize() const;
    [[nodiscard]] static double factor();
    [[nodiscard]] static QFont font();

    // Shows a theme and a scale at once, without keeping them.
    Q_INVOKABLE void show(int theme, int scale);
    // Gives a window the program's colours, now and at every later change: a
    // Qt Quick window takes the desktop theme's colours when it is made, not
    // the program's (D-023). Called once by each window as it is made.
    Q_INVOKABLE void dress(QObject* window);

    // Keeps them, and shows them.
    Q_INVOKABLE void keep(int theme, int scale);
    // Shows what is kept again (Cancel).
    Q_INVOKABLE void showKept();

    // At the start, before the first window: what is kept, put in use.
    static void applyKept();

signals:
    void keptChanged();
    void fontChanged();
    void paletteChanged();
    void factorChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

} // namespace chrono_vault
