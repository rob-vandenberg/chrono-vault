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

#include <QEvent>
#include <QFontInfo>
#include <QGuiApplication>
#include <QSettings>

#include <algorithm>

// --- Version ------------------------------------------------------------
// appearance.cpp 0.0.7

// --- Version history ----------------------------------------------------
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
    return themeFromName(QSettings().value(themeKey).toString());
}

int keptScale()
{
    bool isNumber = false;
    const int percent = QSettings().value(scaleKey, programScaleToBeginWith).toInt(&isNumber);
    return isNumber ? programScaleHeldToItsLimits(percent) : programScaleToBeginWith;
}

} // namespace

Appearance::Appearance(QObject* parent) : QObject(parent)
{
    if (QCoreApplication* application = QCoreApplication::instance()) {
        application->installEventFilter(this);
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
    return QFontInfo(QGuiApplication::font()).pointSizeF();
}

QFont Appearance::font()
{
    return QGuiApplication::font();
}

void Appearance::show(int theme, int scale)
{
    applyTheme(themeFromNumber(theme));
    applyProgramScale(scale);
}

void Appearance::keep(int theme, int scale)
{
    QSettings settings;
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

bool Appearance::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == QCoreApplication::instance() && event->type() == QEvent::ApplicationFontChange) {
        emit fontChanged();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace chrono_vault
