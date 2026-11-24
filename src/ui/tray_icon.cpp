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

#include "tray_icon.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QStyleHints>

#ifdef Q_OS_WIN
#include <QSettings>
#include <qt_windows.h>
#endif

// --- Version ------------------------------------------------------------
// tray_icon.cpp 0.5.85

// --- Version history ----------------------------------------------------
// v0.5.85: Initial release (D-064)

namespace chrono_vault {

namespace {

std::optional<TrayColour>& colourForTests()
{
    static std::optional<TrayColour> colour;
    return colour;
}

} // namespace

TrayColour trayColour()
{
    if (colourForTests()) {
        return *colourForTests();
    }
#ifdef Q_OS_WIN
    const QSettings settings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    bool isNumber = false;
    const int light = settings.value(QStringLiteral("SystemUsesLightTheme")).toInt(&isNumber);
    return !isNumber ? TrayColour::Unknown : light != 0 ? TrayColour::Light : TrayColour::Dark;
#else
    if (const QStyleHints* hints = QGuiApplication::styleHints()) {
        switch (hints->colorScheme()) {
        case Qt::ColorScheme::Dark: return TrayColour::Dark;
        case Qt::ColorScheme::Light: return TrayColour::Light;
        case Qt::ColorScheme::Unknown: break;
        }
    }
    return TrayColour::Unknown;
#endif
}

QString trayIconFile(TrayColour colour)
{
    return colour == TrayColour::Dark ? QStringLiteral(":/icons/chrono-vault-tray.svg")
                                      : QStringLiteral(":/icons/chrono-vault-tray-light.svg");
}

void setTrayColourForTests(std::optional<TrayColour> colour)
{
    colourForTests() = colour;
}

TrayColourWatch::TrayColourWatch(QObject* parent) : QObject(parent)
{
    if (const QStyleHints* hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, &TrayColourWatch::changed);
    }
#ifdef Q_OS_WIN
    QCoreApplication::instance()->installNativeEventFilter(this);
#endif
}

TrayColourWatch::~TrayColourWatch()
{
    if (QCoreApplication* app = QCoreApplication::instance()) {
        app->removeNativeEventFilter(this);
    }
}

// Windows tells every window when its colours change; the taskbar's alone
// changes no colour of a program, so Qt says nothing of it.
bool TrayColourWatch::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(eventType)
    Q_UNUSED(result)
#ifdef Q_OS_WIN
    const MSG* told = static_cast<const MSG*>(message);
    if (told->message == WM_SETTINGCHANGE && told->lParam != 0
        && lstrcmpiW(reinterpret_cast<const wchar_t*>(told->lParam), L"ImmersiveColorSet") == 0) {
        QMetaObject::invokeMethod(this, &TrayColourWatch::changed, Qt::QueuedConnection);
    }
#else
    Q_UNUSED(message)
#endif
    return false;
}

} // namespace chrono_vault
