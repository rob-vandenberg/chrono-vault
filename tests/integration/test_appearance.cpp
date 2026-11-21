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
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QWindow>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

#include <memory>

// --- Version ------------------------------------------------------------
// test_appearance.cpp 0.0.7

// --- Version history ----------------------------------------------------
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
};

void TestAppearance::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_appearance"));
    QSettings().clear();
    applyStyle();
}

void TestAppearance::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
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

QTEST_MAIN(TestAppearance)
#include "test_appearance.moc"
