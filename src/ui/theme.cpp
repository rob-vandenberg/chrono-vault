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
#include "theme.h"

#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>

// --- Version ------------------------------------------------------------
// theme.cpp 0.0.7

// --- Version history ----------------------------------------------------
// v0.0.7: Initial release, as in Chrono Viewer (D-023)

namespace chrono_vault {

namespace {

struct Look
{
    QColor window;
    QColor windowText;
    QColor base; // where a list or a field stands
    QColor alternateBase;
    QColor text;
    QColor placeholder;
    QColor button;
    QColor buttonText;
    QColor brightText;
    QColor highlight;
    QColor highlightedText;
    QColor link;
    QColor linkVisited;
    QColor tooltipBase;
    QColor tooltipText;
    QColor light;
    QColor midlight;
    QColor mid;
    QColor dark;
    QColor shadow;
    QColor disabledText;
};

// The colours of Chrono Viewer's light and dark looks.
const Look lightLook{
    QColor(0xef, 0xef, 0xef), QColor(0x1c, 0x1c, 0x1c), QColor(0xff, 0xff, 0xff), QColor(0xf6, 0xf6, 0xf6),
    QColor(0x1c, 0x1c, 0x1c), QColor(0x7a, 0x7a, 0x7a), QColor(0xe4, 0xe4, 0xe4), QColor(0x1c, 0x1c, 0x1c),
    QColor(0xff, 0xff, 0xff), QColor(0x2d, 0x6f, 0xb8), QColor(0xff, 0xff, 0xff), QColor(0x1d, 0x5c, 0xa8),
    QColor(0x6b, 0x3f, 0xa0), QColor(0xfd, 0xfd, 0xe0), QColor(0x1c, 0x1c, 0x1c), QColor(0xff, 0xff, 0xff),
    QColor(0xf4, 0xf4, 0xf4), QColor(0xb8, 0xb8, 0xb8), QColor(0x92, 0x92, 0x92), QColor(0x6e, 0x6e, 0x6e),
    QColor(0x9e, 0x9e, 0x9e),
};

const Look darkLook{
    QColor(0x2b, 0x2d, 0x30), QColor(0xe8, 0xe8, 0xe8), QColor(0x1f, 0x21, 0x24), QColor(0x26, 0x28, 0x2b),
    QColor(0xe8, 0xe8, 0xe8), QColor(0x8c, 0x8c, 0x8c), QColor(0x35, 0x38, 0x3b), QColor(0xe8, 0xe8, 0xe8),
    QColor(0xff, 0xff, 0xff), QColor(0x3d, 0x81, 0xc9), QColor(0xff, 0xff, 0xff), QColor(0x6f, 0xb0, 0xf0),
    QColor(0xb0, 0x90, 0xe0), QColor(0x3a, 0x3d, 0x41), QColor(0xe8, 0xe8, 0xe8), QColor(0x44, 0x47, 0x4b),
    QColor(0x37, 0x3a, 0x3e), QColor(0x22, 0x24, 0x27), QColor(0x1a, 0x1c, 0x1e), QColor(0x14, 0x15, 0x17),
    QColor(0x76, 0x76, 0x76),
};

QPalette paletteOf(const Look& look)
{
    QPalette palette;
    palette.setColor(QPalette::Window, look.window);
    palette.setColor(QPalette::WindowText, look.windowText);
    palette.setColor(QPalette::Base, look.base);
    palette.setColor(QPalette::AlternateBase, look.alternateBase);
    palette.setColor(QPalette::Text, look.text);
    palette.setColor(QPalette::PlaceholderText, look.placeholder);
    palette.setColor(QPalette::Button, look.button);
    palette.setColor(QPalette::ButtonText, look.buttonText);
    palette.setColor(QPalette::BrightText, look.brightText);
    palette.setColor(QPalette::Highlight, look.highlight);
    palette.setColor(QPalette::HighlightedText, look.highlightedText);
    palette.setColor(QPalette::Accent, look.highlight);
    palette.setColor(QPalette::Link, look.link);
    palette.setColor(QPalette::LinkVisited, look.linkVisited);
    palette.setColor(QPalette::ToolTipBase, look.tooltipBase);
    palette.setColor(QPalette::ToolTipText, look.tooltipText);
    palette.setColor(QPalette::Light, look.light);
    palette.setColor(QPalette::Midlight, look.midlight);
    palette.setColor(QPalette::Mid, look.mid);
    palette.setColor(QPalette::Dark, look.dark);
    palette.setColor(QPalette::Shadow, look.shadow);
    for (const QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, look.disabledText);
    }
    return palette;
}

Theme& chosenTheme()
{
    static Theme theme = Theme::FollowTheSystem;
    return theme;
}

} // namespace

QString nameOfTheme(Theme theme)
{
    switch (theme) {
    case Theme::Light: return QStringLiteral("light");
    case Theme::Dark: return QStringLiteral("dark");
    case Theme::FollowTheSystem: break;
    }
    return QStringLiteral("follow_the_system");
}

Theme themeFromName(const QString& name)
{
    if (name == QLatin1String("light")) {
        return Theme::Light;
    }
    if (name == QLatin1String("dark")) {
        return Theme::Dark;
    }
    return Theme::FollowTheSystem;
}

bool lookIsDark(Theme chosen)
{
    switch (chosen) {
    case Theme::Light: return false;
    case Theme::Dark: return true;
    case Theme::FollowTheSystem: break;
    }
    if (const QStyleHints* hints = QGuiApplication::styleHints()) {
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    }
    return false;
}

QPalette lightPalette()
{
    return paletteOf(lightLook);
}

QPalette darkPalette()
{
    return paletteOf(darkLook);
}

void applyTheme(Theme chosen)
{
    chosenTheme() = chosen;
    // Once: when the system switches, Follow the system switches with it.
    static bool following = false;
    if (!following) {
        if (QStyleHints* hints = QGuiApplication::styleHints()) {
            QObject::connect(hints, &QStyleHints::colorSchemeChanged, hints, [](Qt::ColorScheme) {
                if (chosenTheme() == Theme::FollowTheSystem) {
                    applyTheme(Theme::FollowTheSystem);
                }
            });
            following = true;
        }
    }
    const QPalette wanted = lookIsDark(chosen) ? darkPalette() : lightPalette();
    if (QGuiApplication::palette() != wanted) {
        QGuiApplication::setPalette(wanted);
    }
}

Theme themeInUse()
{
    return chosenTheme();
}

} // namespace chrono_vault
