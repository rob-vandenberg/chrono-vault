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

#include "app_setup.h"
#include "appearance.h"
#include "program_scale.h"
#include "theme.h"
#include "vault_controller.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "settings_files.h"
#include <QStandardPaths>
#include <QStyleHints>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>
#include <QScreen>
#include <QQmlContext>
#include <QQuickItem>
#include <QQmlProperty>
#include <QFontInfo>
#include <QQuickWindow>

#include <memory>

// --- Version ------------------------------------------------------------
// test_appearance.cpp 0.0.38

// --- Version history ----------------------------------------------------
// v0.0.38: The settings in chrono-vault.ini, the vaults in chrono-vault.conf (D-026)
// v0.0.11: The settings window at 730 by 650; the slider's steps, marks above it and room around it
// v0.0.10: The kept look drawn from the start, whatever the desktop's theme; text, icons, boxes and margins double at 200%
// v0.0.7: Initial release

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

using namespace chrono_vault;

// The theme and the UI scaling (D-023).
class TestAppearance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void light_and_dark_are_two_looks_and_follow_asks_the_system();
    void a_theme_is_kept_under_its_name();
    void the_scale_stays_within_its_limits_and_never_scales_twice();
    void the_settings_window_shows_at_once_and_keeps_only_with_ok();
    void the_kept_look_is_drawn_from_the_start_whatever_the_desktop();
    void at_twice_the_scale_text_icons_boxes_and_margins_all_double();
    void the_settings_window_and_its_slider_have_their_sizes();
};

void TestAppearance::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_appearance"));
    SettingsFile().clear();
    VaultsFile().clear();
    applyStyle();
}

void TestAppearance::cleanupTestCase()
{
    SettingsFile settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
    QFile::remove(VaultsFile().fileName());
}

void TestAppearance::light_and_dark_are_two_looks_and_follow_asks_the_system()
{
    QVERIFY(lightPalette().color(QPalette::Window) != darkPalette().color(QPalette::Window));
    QVERIFY(lightPalette().color(QPalette::Window).lightness() > darkPalette().color(QPalette::Window).lightness());
    QVERIFY(!lookIsDark(Theme::Light));
    QVERIFY(lookIsDark(Theme::Dark));
    QCOMPARE(lookIsDark(Theme::FollowTheSystem),
             QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark);

    applyTheme(Theme::Dark);
    QCOMPARE(QGuiApplication::palette().color(QPalette::Window), darkPalette().color(QPalette::Window));
    applyTheme(Theme::Light);
    QCOMPARE(QGuiApplication::palette().color(QPalette::Window), lightPalette().color(QPalette::Window));
}

void TestAppearance::a_theme_is_kept_under_its_name()
{
    for (const Theme theme : {Theme::Light, Theme::Dark, Theme::FollowTheSystem}) {
        QCOMPARE(themeFromName(nameOfTheme(theme)), theme);
    }
    // A name no version wrote follows the system.
    QCOMPARE(themeFromName(QStringLiteral("purple")), Theme::FollowTheSystem);
}

void TestAppearance::the_scale_stays_within_its_limits_and_never_scales_twice()
{
    QCOMPARE(programScaleHeldToItsLimits(10), smallestProgramScale);
    QCOMPARE(programScaleHeldToItsLimits(1000), largestProgramScale);
    QCOMPARE(programScaleHeldToItsLimits(110), 110);

    forgetDesktopFontForTests();
    const QFont desktop = QGuiApplication::font();
    const double desktopSize = desktop.pointSizeF() > 0 ? desktop.pointSizeF() : desktop.pixelSize();
    const auto sizeNow = []() {
        const QFont font = QGuiApplication::font();
        return font.pointSizeF() > 0 ? font.pointSizeF() : font.pixelSize();
    };
    applyProgramScale(200);
    QCOMPARE(sizeNow(), desktopSize * 2);
    applyProgramScale(200); // twice the same: still twice the desktop's size
    QCOMPARE(sizeNow(), desktopSize * 2);
    applyProgramScale(100);
    QCOMPARE(sizeNow(), desktopSize);
    QCOMPARE(programScaleInUse(), 100);
}

void TestAppearance::the_settings_window_shows_at_once_and_keeps_only_with_ok()
{
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* main = engine.rootObjects().constFirst();
    auto* appearance = engine.singletonInstance<Appearance*>("ChronoVault", "Appearance");
    QObject* settings = main->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(appearance && settings);
    QObject* dark = settings->findChild<QObject*>(QStringLiteral("darkTheme"));
    QObject* topics = settings->findChild<QObject*>(QStringLiteral("topics"));
    QObject* ok = settings->findChild<QObject*>(QStringLiteral("okButton"));
    QObject* cancel = settings->findChild<QObject*>(QStringLiteral("cancelButton"));
    QObject* restore = settings->findChild<QObject*>(QStringLiteral("restoreDefaultsButton"));
    QVERIFY(dark && topics && ok && cancel && restore);
    Appearance::applyKept();
    QCOMPARE(appearance->theme(), Appearance::defaultTheme());
    QCOMPARE(appearance->scale(), Appearance::defaultScale());
    const QPalette before = QGuiApplication::palette();

    // Dark and 150% show at once; Cancel puts back what is kept.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(settings, "showTheme", Q_ARG(QVariant, 1)));
    QVERIFY(QMetaObject::invokeMethod(settings, "showScale", Q_ARG(QVariant, 150)));
    QCOMPARE(QGuiApplication::palette().color(QPalette::Window), darkPalette().color(QPalette::Window));
    QCOMPARE(programScaleInUse(), 150);
    QVERIFY(dark->property("checked").toBool());
    // The topics' panel grows with the text, so no name is cut off.
    QObject* panel = settings->findChild<QObject*>(QStringLiteral("topicsPanel"));
    QObject* names = settings->findChild<QObject*>(QStringLiteral("topicNames"));
    QVERIFY(panel && names);
    QVERIFY(QMetaObject::invokeMethod(settings, "showScale", Q_ARG(QVariant, 100)));
    QTRY_VERIFY(panel->property("width").toDouble() >= names->property("implicitWidth").toDouble());
    const double wideAt100 = panel->property("width").toDouble();
    QVERIFY(QMetaObject::invokeMethod(settings, "showScale", Q_ARG(QVariant, 150)));
    QTRY_VERIFY(panel->property("width").toDouble() > wideAt100 * 1.3);
    QVERIFY(QMetaObject::invokeMethod(cancel, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QCOMPARE(QGuiApplication::palette().color(QPalette::Window), before.color(QPalette::Window));
    QCOMPARE(programScaleInUse(), 100);
    QCOMPARE(appearance->theme(), Appearance::defaultTheme());

    // With OK they are kept, and stay after the window closes.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(settings, "showTheme", Q_ARG(QVariant, 1)));
    QVERIFY(QMetaObject::invokeMethod(settings, "showScale", Q_ARG(QVariant, 150)));
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QCOMPARE(appearance->theme(), 1);
    QCOMPARE(appearance->scale(), 150);
    QCOMPARE(QGuiApplication::palette().color(QPalette::Window), darkPalette().color(QPalette::Window));
    QCOMPARE(programScaleInUse(), 150);

    // Restore defaults on General shows Follow the system and 100% at once,
    // and OK keeps them.
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(settings->property("visible").toBool());
    topics->setProperty("currentIndex", 0);
    QVERIFY(QMetaObject::invokeMethod(restore, "clicked"));
    QCOMPARE(programScaleInUse(), 100);
    QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
    QTRY_VERIFY(!settings->property("visible").toBool());
    QCOMPARE(appearance->theme(), Appearance::defaultTheme());
    QCOMPARE(appearance->scale(), Appearance::defaultScale());
}

void TestAppearance::the_kept_look_is_drawn_from_the_start_whatever_the_desktop()
{
    // Kept dark, put in use before any window as the program starts: the
    // windows draw the program's colours, never the desktop theme's.
    SettingsFile().setValue(QStringLiteral("view/theme"), QStringLiteral("dark"));
    Appearance::applyKept();
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* settings = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    for (QObject* window : {engine.rootObjects().constFirst(), settings}) {
        QObject* palette = window->property("palette").value<QObject*>();
        QVERIFY(palette);
        QCOMPARE(palette->property("window").value<QColor>(), darkPalette().color(QPalette::Window));
        QCOMPARE(palette->property("highlight").value<QColor>(), darkPalette().color(QPalette::Highlight));
    }
    // New Item: a flat fill in the program's own blue.
    QObject* fill = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("newItemFill"));
    QVERIFY(fill);
    QObject* newItem = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("newItemButton"));
    QVERIFY(newItem);
    if (newItem->property("enabled").toBool()) {
        QCOMPARE(fill->property("color").value<QColor>(), darkPalette().color(QPalette::Highlight));
    }
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    auto* window = qobject_cast<QQuickWindow*>(settings);
    QTRY_VERIFY(window->isExposed());
    const QImage shot = window->grabWindow();
    QCOMPARE(shot.pixelColor(shot.width() - 20, shot.height() / 2), darkPalette().color(QPalette::Window));
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));
    SettingsFile().remove(QStringLiteral("view/theme"));
    Appearance::applyKept();
}

void TestAppearance::at_twice_the_scale_text_icons_boxes_and_margins_all_double()
{
    // Steps of 5%: a value between two steps goes to the nearest one.
    QCOMPARE(programScaleHeldToItsLimits(112), 110);
    QCOMPARE(programScaleHeldToItsLimits(113), 115);
    QCOMPARE(programScaleHeldToItsLimits(49), 50);
    QCOMPARE(programScaleHeldToItsLimits(401), 400);

    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* settings = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings && QMetaObject::invokeMethod(settings, "showWindow"));
    QObject* box = settings->findChild<QObject*>(QStringLiteral("startAtStartupBox"));
    QObject* ok = settings->findChild<QObject*>(QStringLiteral("okButton"));
    QObject* about = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("aboutWindow"));
    QVERIFY(box && ok && about);

    const auto measure = [&]() {
        QObject* indicator = box->property("indicator").value<QObject*>();
        return QList<double>{QFontInfo(QGuiApplication::font()).pointSizeF(),
                             indicator ? indicator->property("width").toDouble() : -1.0,
                             about->property("edge").toDouble(), box->property("padding").toDouble()};
    };
    QVERIFY(QMetaObject::invokeMethod(settings, "showScale", Q_ARG(QVariant, 100)));
    QTRY_VERIFY(measure().at(1) > 0);
    const QList<double> single = measure();
    QVERIFY(QMetaObject::invokeMethod(settings, "showScale", Q_ARG(QVariant, 200)));
    QTRY_VERIFY(measure().at(1) > single.at(1));
    const QList<double> twice = measure();
    QCOMPARE(twice.at(0), single.at(0) * 2); // the text
    QCOMPARE(twice.at(1), single.at(1) * 2); // a tick box
    QCOMPARE(twice.at(2), single.at(2) * 2); // a dialog's margin
    QCOMPARE(twice.at(3), single.at(3) * 2); // a control's padding
    // An icon: the OK button's.
    QCOMPARE(QQmlProperty::read(ok, QStringLiteral("icon.width")).toDouble(), 32.0);
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));
    QTRY_COMPARE(programScaleInUse(), 100);
    SettingsFile().remove(QStringLiteral("windows/settings")); // leaves no remembered size behind
}

void TestAppearance::the_settings_window_and_its_slider_have_their_sizes()
{
    // The size it opens at, not one an earlier test left remembered.
    SettingsFile().remove(QStringLiteral("windows/settings"));
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* settings = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("settingsWindow"));
    QVERIFY(settings);
    auto* window = qobject_cast<QWindow*>(settings);
    QVERIFY(window);
    // Opens at 730 by 650 and is never smaller (the owner).
    QCOMPARE(window->width(), 730);
    QCOMPARE(window->height(), 650);
    QCOMPARE(window->minimumWidth(), 730);
    QCOMPARE(window->minimumHeight(), 650);

    // At 400% the size is larger than the screen: it gives way to the
    // screen's free room (the owner).
    applyProgramScale(400);
    const QSize room = QGuiApplication::primaryScreen()->availableGeometry().size();
    QVERIFY(window->minimumHeight() <= room.height());
    QVERIFY(window->minimumWidth() <= room.width());
    applyProgramScale(100);
    QCOMPARE(window->minimumHeight(), 650);
    // The title bar and frame are taken off the inside: the whole window is 730 by 650.
    settings->setProperty("frame", QSizeF(8, 40));
    QCOMPARE(window->minimumWidth(), 730 - 8);
    QCOMPARE(window->minimumHeight(), 650 - 40);
    settings->setProperty("frame", QSizeF(0, 0));
    QVERIFY(QMetaObject::invokeMethod(settings, "showWindow"));
    QTRY_VERIFY(window->isExposed());
    auto* slider = qobject_cast<QQuickItem*>(settings->findChild<QObject*>(QStringLiteral("scaleSlider")));
    QObject* marks = settings->findChild<QObject*>(QStringLiteral("scaleMarks"));
    QObject* row = settings->findChild<QObject*>(QStringLiteral("scaleRow"));
    QObject* percent = settings->findChild<QObject*>(QStringLiteral("scalePercent"));
    QVERIFY(slider && marks && row && percent);
    // Steps of 5%.
    QCOMPARE(slider->property("stepSize").toDouble(), 5.0);
    // 15 marks, every 25% from 50% to 400%, above the slider.
    QCOMPARE(marks->property("count").toInt(), 15);
    QQuickItem* mark = nullptr;
    QVERIFY(QMetaObject::invokeMethod(marks, "itemAt", Q_RETURN_ARG(QQuickItem*, mark), Q_ARG(int, 0)));
    QVERIFY(mark);
    QVERIFY(mark->mapToScene(QPointF(0, mark->height())).y() <= slider->mapToScene(QPointF(0, 0)).y());
    // Half a line height of room above and below.
    const double halfLine = qRound(percent->property("implicitHeight").toDouble() / 2);
    QCOMPARE(QQmlProperty::read(row, QStringLiteral("Layout.topMargin"), qmlContext(row)).toDouble(), halfLine);
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));
}

QTEST_MAIN(TestAppearance)
#include "test_appearance.moc"
