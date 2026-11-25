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

#include "scaling.h"

#include <QGuiApplication>
#include <QScreen>
#include "settings_files.h"

#include <algorithm>
#include <cmath>
#include <limits>

// --- Version ------------------------------------------------------------
// window_memory.cpp 0.6.94

// --- Version history ----------------------------------------------------
// v0.6.94: The main window keeps its size, the others scale along (D-070)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
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
    if (!scales && kept.width() < 0) {
        kept = asked; // the size it was made with
    }
    restore();
}

void WindowMemory::restore()
{
    if (!target || key.isEmpty()) {
        return;
    }
    const QRect saved = SettingsFile().value(settingsKey(key, "geometry")).toRect();
    if (saved.isValid()) {
        // Its size at 100%: it was remembered at the scaling of that time, the
        // one now when that was not written down (before 0.6.94).
        const double then = SettingsFile().value(settingsKey(key, "scale"), 100 * scaleNow()).toDouble() / 100;
        ownSize = QSizeF(saved.size()) / (scales && then > 0 ? then : 1.0);
        emit ownChanged();
        // Never smaller than the window's minimum now: a size remembered from
        // before a window got a larger minimum does not bring it back.
        QRect atLeast = saved;
        atLeast.setSize((ownSize * scaleNow()).toSize().expandedTo(target->minimumSize()));
        asked = atLeast.size();
        target->setGeometry(insideItsScreen(onAScreen(atLeast)));
    }
    restored = true;
}

bool WindowMemory::hasSaved() const
{
    return !key.isEmpty() && SettingsFile().value(settingsKey(key, "geometry")).toRect().isValid();
}

void WindowMemory::watch()
{
    if (!target) {
        return;
    }
    const auto later = [this]() {
        notice();
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

double WindowMemory::scaleNow() const
{
    return scales ? UiScale::instance()->factor() : 1.0;
}

bool WindowMemory::isNormal() const
{
    return target && target->windowState() == Qt::WindowNoState;
}

int WindowMemory::wide(double first, double least, const QSizeF& own)
{
    if (target && target->isVisible() && !isNormal()) {
        return target->width(); // maximized: as wide as the screen gives it
    }
    if (!scales && kept.width() < 0) {
        kept.setWidth(first);
    }
    double width = own.isValid() ? own.width() * scaleNow() : scales ? first : kept.width();
    width = std::max(width, std::ceil(least));
    if (const QScreen* screen = target ? target->screen() : nullptr) {
        // With its frame, or the desktop would make it smaller itself.
        const QMargins frame = target->frameMargins();
        width = std::min<double>(width, screen->availableGeometry().width() - frame.left() - frame.right());
    }
    asked.setWidth(static_cast<int>(std::lround(width)));
    return asked.width();
}

int WindowMemory::high(double first, double least, const QSizeF& own)
{
    if (target && target->isVisible() && !isNormal()) {
        return target->height();
    }
    if (!scales && kept.height() < 0) {
        kept.setHeight(first);
    }
    double height = own.isValid() ? own.height() * scaleNow() : scales ? first : kept.height();
    height = std::max(height, std::ceil(least));
    if (const QScreen* screen = target ? target->screen() : nullptr) {
        const QMargins frame = target->frameMargins();
        height = std::min<double>(height, screen->availableGeometry().height() - frame.top() - frame.bottom());
    }
    asked.setHeight(static_cast<int>(std::lround(height)));
    return asked.height();
}

void WindowMemory::notice()
{
    if (!restored || !target->isVisible() || !isNormal() || target->size() == asked) {
        return;
    }
    const QSizeF given = QSizeF(target->size()) / scaleNow();
    if (given != ownSize) {
        ownSize = given;
        asked = target->size();
        emit ownChanged();
    }
}

void WindowMemory::save()
{
    if (!target || key.isEmpty()) {
        return;
    }
    SettingsFile settings;
    const Qt::WindowState state = target->windowState();
    settings.setValue(settingsKey(key, "state"), static_cast<int>(state));
    // The size and place of the normal window: a maximized or minimized one
    // comes back to that when it is made normal again.
    if (state == Qt::WindowNoState && (target->isVisible() || shownOnce)) {
        settings.setValue(settingsKey(key, "geometry"), target->geometry());
        settings.setValue(settingsKey(key, "scale"), qRound(100 * scaleNow()));
    }
}

Qt::WindowState WindowMemory::savedState(const QString& name)
{
    const int state = SettingsFile().value(settingsKey(name, "state"), static_cast<int>(Qt::WindowNoState)).toInt();
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
