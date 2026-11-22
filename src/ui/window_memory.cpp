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

#include "window_memory.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

#include <algorithm>
#include <limits>

// --- Version ------------------------------------------------------------
// window_memory.cpp 0.0.11

// --- Version history ----------------------------------------------------
// v0.0.11: A remembered size never smaller than the window's minimum, and moved back inside its screen
// v0.0.4: Initial release

namespace chrono_vault {

namespace {

QString settingsKey(const QString& name, const char* what)
{
    return QStringLiteral("windows/%1/%2").arg(name, QLatin1String(what));
}

} // namespace

WindowMemory::WindowMemory(QObject* parent) : QObject(parent)
{
    // Many changes come while a window is dragged; one write when it rests.
    saveTimer.setSingleShot(true);
    saveTimer.setInterval(400);
    connect(&saveTimer, &QTimer::timeout, this, &WindowMemory::save);
}

void WindowMemory::setWindow(QWindow* window)
{
    if (target != window) {
        if (target) {
            disconnect(target, nullptr, this, nullptr);
        }
        target = window;
        watch();
        emit windowChanged();
    }
}

void WindowMemory::setName(const QString& name)
{
    if (key != name) {
        key = name;
        emit nameChanged();
    }
}

void WindowMemory::componentComplete()
{
    restore();
}

void WindowMemory::restore()
{
    if (!target || key.isEmpty()) {
        return;
    }
    const QRect saved = QSettings().value(settingsKey(key, "geometry")).toRect();
    if (saved.isValid()) {
        // Never smaller than the window's minimum now: a size remembered from
        // before a window got a larger minimum does not bring it back.
        QRect atLeast = saved;
        atLeast.setSize(saved.size().expandedTo(target->minimumSize()));
        target->setGeometry(insideItsScreen(onAScreen(atLeast)));
    }
    restored = true;
}

bool WindowMemory::hasSaved() const
{
    return !key.isEmpty() && QSettings().value(settingsKey(key, "geometry")).toRect().isValid();
}

void WindowMemory::watch()
{
    if (!target) {
        return;
    }
    const auto later = [this]() {
        if (restored) {
            saveTimer.start();
        }
    };
    connect(target, &QWindow::xChanged, this, later);
    connect(target, &QWindow::yChanged, this, later);
    connect(target, &QWindow::widthChanged, this, later);
    connect(target, &QWindow::heightChanged, this, later);
    connect(target, &QWindow::windowStateChanged, this, later);
    connect(target, &QWindow::visibleChanged, this, [this](bool visible) {
        if (visible) {
            shownOnce = true;
        } else if (restored) {
            save(); // closing or hiding: the last place, at once
        }
    });
}

void WindowMemory::save()
{
    if (!target || key.isEmpty()) {
        return;
    }
    QSettings settings;
    const Qt::WindowState state = target->windowState();
    settings.setValue(settingsKey(key, "state"), static_cast<int>(state));
    // The size and place of the normal window: a maximized or minimized one
    // comes back to that when it is made normal again.
    if (state == Qt::WindowNoState && (target->isVisible() || shownOnce)) {
        settings.setValue(settingsKey(key, "geometry"), target->geometry());
    }
}

Qt::WindowState WindowMemory::savedState(const QString& name)
{
    const int state = QSettings().value(settingsKey(name, "state"), static_cast<int>(Qt::WindowNoState)).toInt();
    if (state == Qt::WindowMaximized || state == Qt::WindowMinimized || state == Qt::WindowFullScreen) {
        return static_cast<Qt::WindowState>(state);
    }
    return Qt::WindowNoState;
}

QRect WindowMemory::insideItsScreen(const QRect& place)
{
    // A window made larger than it was remembered (its minimum grew) can reach
    // past the edge of its screen: it is moved back inside, its size kept.
    // The screen it lies on most.
    const QScreen* mostOn = nullptr;
    long long largest = 0;
    for (const QScreen* screen : QGuiApplication::screens()) {
        const QRect overlap = screen->availableGeometry().intersected(place);
        const long long size = static_cast<long long>(overlap.width()) * overlap.height();
        if (overlap.isValid() && size > largest) {
            largest = size;
            mostOn = screen;
        }
    }
    if (mostOn) {
        const QRect area = mostOn->availableGeometry();
        QRect moved = place;
        if (moved.right() > area.right()) {
            moved.moveRight(area.right());
        }
        if (moved.bottom() > area.bottom()) {
            moved.moveBottom(area.bottom());
        }
        if (moved.left() < area.left()) {
            moved.moveLeft(area.left());
        }
        if (moved.top() < area.top()) {
            moved.moveTop(area.top());
        }
        return moved;
    }
    return place;
}

QRect WindowMemory::onAScreen(const QRect& place)
{
    const QList<QScreen*> screens = QGuiApplication::screens();
    if (screens.isEmpty()) {
        return place;
    }
    for (QScreen* screen : screens) {
        if (screen->availableGeometry().contains(place.center())) {
            return place;
        }
    }
    // On no screen any more (a monitor was unplugged): onto the nearest one.
    QScreen* nearest = screens.constFirst();
    int shortest = std::numeric_limits<int>::max();
    for (QScreen* screen : screens) {
        const int distance = (screen->availableGeometry().center() - place.center()).manhattanLength();
        if (distance < shortest) {
            shortest = distance;
            nearest = screen;
        }
    }
    const QRect area = nearest->availableGeometry();
    QRect moved(place.topLeft(), place.size().boundedTo(area.size()));
    moved.moveCenter(area.center());
    return moved;
}

} // namespace chrono_vault
