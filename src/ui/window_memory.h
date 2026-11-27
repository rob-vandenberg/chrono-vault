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

#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <QRect>
#include <QSizeF>
#include <QTimer>
#include <QWindow>
#include <QtQml/qqmlregistration.h>

// --- Version ------------------------------------------------------------
// window_memory.h 0.8.120

// --- Version history ----------------------------------------------------
// v0.8.120: completed: a name set later reads its own size (D-082)
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.0.11: A place moved back inside its screen
// v0.0.4: Initial release

namespace chrono_vault {

// Every window remembers its size, its place and its state (D-020): placed in
// a window with a name of its own, it puts the window back as it was left,
// on a screen that still exists, and saves every change.
class WindowMemory : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_ELEMENT

    Q_PROPERTY(QWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    // Whether the window scales along with the UI scaling; the main window
    // keeps its size (D-070).
    Q_PROPERTY(bool scales MEMBER scales)
    // The size the person gave the window: as it is at 100% when it scales,
    // in pixels when it keeps its size; invalid while there is none.
    Q_PROPERTY(QSizeF own READ own NOTIFY ownChanged)

public:
    explicit WindowMemory(QObject* parent = nullptr);

    [[nodiscard]] QWindow* window() const { return target; }
    void setWindow(QWindow* window);
    [[nodiscard]] QString name() const { return key; }
    void setName(const QString& name);

    [[nodiscard]] QSizeF own() const { return ownSize; }
    // The window's width and height now: the person's own at the scaling now,
    // the first one until there is one; at least what it needs, within its
    // screen. Given "own", so that a window follows it (D-070).
    Q_INVOKABLE int wide(double first, double least, const QSizeF& own);
    Q_INVOKABLE int high(double first, double least, const QSizeF& own);

    // Puts the window's size and place back, kept on a screen.
    Q_INVOKABLE void restore();
    // Whether this window has been placed before.
    [[nodiscard]] Q_INVOKABLE bool hasSaved() const;

    void classBegin() override {}
    void componentComplete() override;

    // The state the window named so was left in (normal, maximized or
    // minimized), for the program as it starts.
    [[nodiscard]] static Qt::WindowState savedState(const QString& name);
    // A place kept on a screen: moved onto the nearest one when it is on
    // none, and made smaller when it is larger than that screen.
    [[nodiscard]] static QRect onAScreen(const QRect& place);
    // The place moved back inside the screen it is on, its size kept.
    [[nodiscard]] static QRect insideItsScreen(const QRect& place);

signals:
    void windowChanged();
    void nameChanged();
    void ownChanged();

private:
    void watch();
    void save();
    // A size the window has that was not asked for here is the person's.
    void notice();
    [[nodiscard]] double scaleNow() const;
    [[nodiscard]] bool isNormal() const;

    QPointer<QWindow> target;
    QString key;
    bool restored = false;
    bool completed = false; // a name set after this reads its own size
    bool scales = true;
    QSizeF ownSize;
    QSize asked{-1, -1}; // what the window was last told to be
    QSizeF kept{-1, -1}; // the first size of a window that keeps its size
    bool shownOnce = false; // the place means something only once it was shown
    QTimer saveTimer;
};

} // namespace chrono_vault
