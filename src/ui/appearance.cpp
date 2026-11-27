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
#include "appearance.h"

#include "program_scale.h"
#include "theme.h"
#include "scaling.h"

#include <QCursor>
#include <QEvent>
#include <QQuickItem>
#include <QFontInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QWidget>
#include "settings_files.h"

#include <algorithm>

// --- Version ------------------------------------------------------------
// appearance.cpp 0.8.119

// --- Version history ----------------------------------------------------
// v0.8.119: questionFlags: a utility window on Linux, where KDE minimizes every dialog (D-079)
// v0.6.92: The font at 100% for the windows (D-069)
// v0.5.82: focusWindow (D-062)
// v0.2.47: pointerIn: where the pointer is, for the sidebar snap (the owner)
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.13: The size of a window's title bar and frame
// v0.0.12: The free room of a window's screen, which a minimum gives way to; --reset at the start (D-026)
// v0.0.10: The program's colours for every window (D-023)
// v0.0.7: Initial release (D-023)

namespace chrono_vault {

namespace {

const QString themeKey = QStringLiteral("view/theme");
const QString scaleKey = QStringLiteral("view/scale");

Theme themeFromNumber(int number)
{
    return static_cast<Theme>(std::clamp(number, 0, 2));
}

Theme keptTheme()
{
    return themeFromName(SettingsFile().value(themeKey).toString());
}

int keptScale()
{
    bool isNumber = false;
    const int percent = SettingsFile().value(scaleKey, programScaleToBeginWith).toInt(&isNumber);
    return isNumber ? programScaleHeldToItsLimits(percent) : programScaleToBeginWith;
}

} // namespace

int Appearance::questionFlags()
{
    const Qt::WindowFlags titleAndClose = Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    // KDE's window manager offers minimize on every dialog window, whatever
    // the flags ask; a utility window it never minimizes (D-079).
    return static_cast<int>(Qt::Tool | titleAndClose);
#else
    return static_cast<int>(Qt::Dialog | titleAndClose);
#endif
}

Appearance::Appearance(QObject* parent) : QObject(parent)
{
    if (QCoreApplication* application = QCoreApplication::instance()) {
        application->installEventFilter(this);
    }
    connect(UiScale::instance(), &UiScale::factorChanged, this, &Appearance::factorChanged);
    if (auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        connect(application, &QGuiApplication::focusWindowChanged, this, &Appearance::focusWindowChanged);
    }
}

int Appearance::theme() const
{
    return static_cast<int>(keptTheme());
}

int Appearance::scale() const
{
    return keptScale();
}

int Appearance::smallestScale()
{
    return smallestProgramScale;
}

int Appearance::largestScale()
{
    return largestProgramScale;
}

int Appearance::scaleMarkEvery()
{
    return programScaleMarkEvery;
}

int Appearance::scaleStep()
{
    return programScaleStep;
}

int Appearance::defaultTheme()
{
    return static_cast<int>(Theme::FollowTheSystem);
}

int Appearance::defaultScale()
{
    return programScaleToBeginWith;
}

double Appearance::fontPointSize() const
{
    return QFontInfo(programFont()).pointSizeF();
}

double Appearance::factor()
{
    return UiScale::instance()->factor();
}

QObject* Appearance::focusWindow()
{
    return QGuiApplication::focusWindow();
}

QFont Appearance::font()
{
    return programFont();
}

namespace {

// The names Qt Quick gives the colour roles of a window's palette.
const QList<std::pair<const char*, QPalette::ColorRole>> paletteRoles{
    {"alternateBase", QPalette::AlternateBase}, {"base", QPalette::Base},
    {"brightText", QPalette::BrightText},       {"button", QPalette::Button},
    {"buttonText", QPalette::ButtonText},       {"dark", QPalette::Dark},
    {"highlight", QPalette::Highlight},         {"highlightedText", QPalette::HighlightedText},
    {"light", QPalette::Light},                 {"link", QPalette::Link},
    {"linkVisited", QPalette::LinkVisited},     {"mid", QPalette::Mid},
    {"midlight", QPalette::Midlight},           {"shadow", QPalette::Shadow},
    {"text", QPalette::Text},                   {"toolTipBase", QPalette::ToolTipBase},
    {"toolTipText", QPalette::ToolTipText},     {"window", QPalette::Window},
    {"windowText", QPalette::WindowText},       {"placeholderText", QPalette::PlaceholderText},
    {"accent", QPalette::Accent},
};

void copyPalette(QObject* window)
{
    QObject* palette = window->property("palette").value<QObject*>();
    if (!palette) {
        return;
    }
    const QPalette ours = QGuiApplication::palette();
    for (const auto& [groupName, group] : {std::pair{"active", QPalette::Active}, std::pair{"inactive", QPalette::Inactive},
                                           std::pair{"disabled", QPalette::Disabled}}) {
        QObject* colours = palette->property(groupName).value<QObject*>();
        if (!colours) {
            continue;
        }
        for (const auto& [roleName, role] : paletteRoles) {
            colours->setProperty(roleName, ours.color(group, role));
        }
    }
}

} // namespace

void Appearance::dress(QObject* window)
{
    if (!window) {
        return;
    }
    copyPalette(window);
    connect(this, &Appearance::paletteChanged, window, [window]() { copyPalette(window); });
}

QSize Appearance::screenRoom(QObject* window) const
{
    const auto* shown = qobject_cast<QWindow*>(window);
    const QScreen* screen = shown && shown->screen() ? shown->screen() : QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry().size() : QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
}

QPointF Appearance::pointerIn(QObject* item) const
{
    const auto* shown = qobject_cast<QQuickItem*>(item);
    return shown ? shown->mapFromGlobal(QCursor::pos()) : QPointF();
}

QSize Appearance::frameOf(QObject* window) const
{
    const auto* shown = qobject_cast<QWindow*>(window);
    if (!shown) {
        return {};
    }
    const QMargins frame = shown->frameMargins();
    return {frame.left() + frame.right(), frame.top() + frame.bottom()};
}

void Appearance::show(int theme, int scale)
{
    applyTheme(themeFromNumber(theme));
    applyProgramScale(scale);
}

void Appearance::keep(int theme, int scale)
{
    SettingsFile settings;
    settings.setValue(themeKey, nameOfTheme(themeFromNumber(theme)));
    settings.setValue(scaleKey, programScaleHeldToItsLimits(scale));
    show(theme, scale);
    emit keptChanged();
}

void Appearance::showKept()
{
    applyKept();
}

void Appearance::applyKept()
{
    applyTheme(keptTheme());
    applyProgramScale(keptScale());
}

void Appearance::applyAtStart()
{
    applyKept();
}

bool Appearance::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == QCoreApplication::instance() && event->type() == QEvent::ApplicationFontChange) {
        emit fontChanged();
    }
    if (watched == QCoreApplication::instance() && event->type() == QEvent::ApplicationPaletteChange) {
        emit paletteChanged();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace chrono_vault
