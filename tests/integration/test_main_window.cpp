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
#include "licences.h"
#include "version.h"

#include <QFont>
#include <QList>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QSettings>
#include <QStandardPaths>
#include <QtQml/QQmlExtensionPlugin>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_main_window.cpp 0.0.11

// --- Version history ----------------------------------------------------
// v0.0.11: The About box at 555 by 440, never smaller
// v0.0.9: The About box's name, copyright and link lined up with the icon
// v0.0.8: The About box shows every licence, ours first, Check for update at the bottom left; the tray text
// v0.0.4: Chrono Vault's own style and fonts; the version in the title
// v0.0.3: Settings of the test's own, never the person's
// v0.0.1: Initial release

Q_IMPORT_QML_PLUGIN(ChronoVaultPlugin)

class TestMainWindow : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void the_main_window_loads_without_qml_errors_and_is_titled_chrono_vault();
    void the_about_box_opens_at_555_by_440();
    void a_smaller_remembered_about_box_opens_at_its_minimum();
    void the_about_box_shows_every_licence_ours_first();
    void the_tray_setting_says_keep_chrono_vault_running();
    void the_licences_scroll_by_keys_and_nothing_bounces();
};

void TestMainWindow::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_main_window"));
    chrono_vault::applyStyle();
}

void TestMainWindow::the_main_window_loads_without_qml_errors_and_is_titled_chrono_vault()
{
    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });

    engine.loadFromModule("ChronoVault", "Main");

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QCOMPARE(window->title(), QStringLiteral("Chrono Vault ") + chrono_vault::programVersion());

    // The Noto fonts, carried inside the program (D-017).
    QCOMPARE(QGuiApplication::font().family(), QStringLiteral("Noto Sans"));
    auto* masterPassword = window->findChild<QObject*>(QStringLiteral("masterPasswordField"));
    QVERIFY(masterPassword != nullptr);
    QCOMPARE(masterPassword->property("font").value<QFont>().family(), QStringLiteral("Noto Sans Mono"));

    // Leave nothing behind (OR-11): the window goes with the engine.
    window->close();
}

void TestMainWindow::the_about_box_opens_at_555_by_440()
{
    // Nothing remembered: it opens at 555 by 440 (the owner).
    QSettings().remove(QStringLiteral("windows/about"));
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* about = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("aboutWindow"));
    QVERIFY(about);
    QCOMPARE(about->property("width").toInt(), 555);
    QCOMPARE(about->property("height").toInt(), 440);
    QSettings().remove(QStringLiteral("windows/about"));
}

void TestMainWindow::a_smaller_remembered_about_box_opens_at_its_minimum()
{
    // Remembered at 500 by 300 from an older version: raised to 555 by 440.
    QSettings().setValue(QStringLiteral("windows/about/geometry"), QRect(100, 100, 500, 300));
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    auto* about = qobject_cast<QWindow*>(engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("aboutWindow")));
    QVERIFY(about);
    QCOMPARE(about->width(), 555);
    QCOMPARE(about->height(), 440);
    QSettings().remove(QStringLiteral("windows/about"));
}

void TestMainWindow::the_about_box_shows_every_licence_ours_first()
{
    // Every licence that ships has its text inside the program, ours first.
    const QList<chrono_vault::Licence> licences = chrono_vault::shippedLicences();
    QVERIFY(licences.size() >= 6);
    QVERIFY(licences.constFirst().covers.contains(QStringLiteral("GNU Affero General Public License")));
    QVERIFY(licences.constFirst().text.contains(QStringLiteral("GNU AFFERO GENERAL PUBLIC LICENSE")));
    for (const QString& expected : {QStringLiteral("GNU LESSER GENERAL PUBLIC LICENSE"),
                                    QStringLiteral("Version 3, 29 June 2007"), QStringLiteral("Version 2, June 1991"),
                                    QStringLiteral("Apache License"), QStringLiteral("SIL OPEN FONT LICENSE")}) {
        bool found = false;
        for (const chrono_vault::Licence& licence : licences) {
            found = found || licence.text.contains(expected);
        }
        QVERIFY2(found, qPrintable(expected));
    }
    for (int i = 0; i < 6; ++i) {
        QVERIFY2(!licences.at(i).text.isEmpty(), qPrintable(licences.at(i).covers));
    }

    QList<QQmlError> qmlWarnings;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                     [&qmlWarnings](const QList<QQmlError>& warnings) { qmlWarnings.append(warnings); });
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* about = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("aboutWindow"));
    QVERIFY(about);
    // Never smaller than 555 by 440 (the owner), also when a smaller size is
    // remembered from before.
    QCOMPARE(about->property("minimumWidth").toInt(), 555);
    QCOMPARE(about->property("minimumHeight").toInt(), 440);
    QVERIFY(QMetaObject::invokeMethod(about, "open"));
    QTRY_VERIFY(about->property("visible").toBool());

    // The text area: our licence first, then a bar, then the others.
    QObject* text = about->findChild<QObject*>(QStringLiteral("licenceText"));
    QVERIFY(text);
    QVERIFY(text->property("readOnly").toBool());
    const QString shown = text->property("text").toString();
    const qsizetype ours = shown.indexOf(QStringLiteral("GNU AFFERO GENERAL PUBLIC LICENSE"));
    const qsizetype qt = shown.indexOf(QStringLiteral("GNU LESSER GENERAL PUBLIC LICENSE"));
    QVERIFY(ours >= 0 && qt > ours);
    QVERIFY(text->property("length").toInt() > 10000);

    // Check for update at the bottom left, Close at the bottom right, on one row.
    auto* check = qobject_cast<QQuickItem*>(about->findChild<QObject*>(QStringLiteral("checkForUpdateButton")));
    QVariant closeVariant;
    QVERIFY(QMetaObject::invokeMethod(about, "standardButton", Q_RETURN_ARG(QVariant, closeVariant),
                                      Q_ARG(QVariant, 0x00200000))); // Dialog.Close
    auto* close = qobject_cast<QQuickItem*>(closeVariant.value<QObject*>());
    QVERIFY(check && close);
    auto* window = qobject_cast<QQuickWindow*>(about);
    QTRY_VERIFY(close->width() > 0);
    const QPointF checkAt = check->mapToScene(QPointF(0, 0));
    const QPointF closeAt = close->mapToScene(QPointF(0, 0));
    QVERIFY(qAbs((checkAt.y() + check->height() / 2) - (closeAt.y() + close->height() / 2)) <= 1.0);
    QVERIFY(checkAt.x() < window->width() / 4.0);
    QVERIFY(closeAt.x() + close->width() > window->width() * 3 / 4.0);

    // The name at the top of the icon, the link at its bottom, the copyright
    // exactly in the middle (D-021).
    auto* nameLine = qobject_cast<QQuickItem*>(about->findChild<QObject*>(QStringLiteral("nameLine")));
    auto* copyrightLine = qobject_cast<QQuickItem*>(about->findChild<QObject*>(QStringLiteral("copyrightLine")));
    auto* siteLine = qobject_cast<QQuickItem*>(about->findChild<QObject*>(QStringLiteral("siteLine")));
    QVERIFY(nameLine && copyrightLine && siteLine);
    QVERIFY(siteLine->property("text").toString().contains(QStringLiteral("https://github.com/rob-vandenberg/chrono-vault")));
    auto* heading = siteLine->parentItem();
    const double top = heading->mapToScene(QPointF(0, 0)).y();
    const double bottom = top + heading->height();
    QVERIFY(qAbs(nameLine->mapToScene(QPointF(0, 0)).y() - top) <= 1.0);
    QVERIFY(qAbs(siteLine->mapToScene(QPointF(0, siteLine->height())).y() - bottom) <= 1.0);
    const double nameMiddle = nameLine->mapToScene(QPointF(0, nameLine->height() / 2)).y();
    const double siteMiddle = siteLine->mapToScene(QPointF(0, siteLine->height() / 2)).y();
    const double copyrightMiddle = copyrightLine->mapToScene(QPointF(0, copyrightLine->height() / 2)).y();
    QVERIFY(qAbs(copyrightMiddle - (nameMiddle + siteMiddle) / 2) <= 1.0);

    QString warningText;
    for (const QQmlError& warning : std::as_const(qmlWarnings)) {
        warningText += warning.toString() + QLatin1Char('\n');
    }
    QVERIFY2(qmlWarnings.isEmpty(), qPrintable(warningText));
}

void TestMainWindow::the_tray_setting_says_keep_chrono_vault_running()
{
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* box = engine.rootObjects().constFirst()->findChild<QObject*>(QStringLiteral("keepInTrayBox"));
    QVERIFY(box);
    QCOMPARE(box->property("text").toString(), QStringLiteral("Keep Chrono Vault running in the system tray"));
}

void TestMainWindow::the_licences_scroll_by_keys_and_nothing_bounces()
{
    QQmlApplicationEngine engine;
    engine.loadFromModule("ChronoVault", "Main");
    QVERIFY(!engine.rootObjects().isEmpty());
    QObject* root = engine.rootObjects().constFirst();
    auto* about = qobject_cast<QQuickWindow*>(root->findChild<QObject*>(QStringLiteral("aboutWindow")));
    QVERIFY(about);
    QVERIFY(QMetaObject::invokeMethod(about, "open"));
    QTRY_VERIFY(about->isVisible());
    about->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(about));
    auto* text = about->findChild<QQuickItem*>(QStringLiteral("licenceText"));
    QVERIFY(text);
    QTRY_VERIFY(text->hasActiveFocus()); // the keys work at once
    QQuickItem* flick = text->parentItem();
    while (flick && !flick->property("contentY").isValid()) {
        flick = flick->parentItem();
    }
    QVERIFY(flick);
    const auto y = [flick]() { return flick->property("contentY").toReal(); };
    QCOMPARE(y(), 0.0);
    QTest::keyClick(about, Qt::Key_Down);
    QVERIFY(y() > 0);
    const qreal afterLine = y();
    QTest::keyClick(about, Qt::Key_PageDown);
    QVERIFY(y() > afterLine + flick->height() / 2);
    QTest::keyClick(about, Qt::Key_End);
    QCOMPARE(y(), flick->property("contentHeight").toReal() - flick->height());
    QTest::keyClick(about, Qt::Key_PageUp);
    QVERIFY(y() < flick->property("contentHeight").toReal() - flick->height());
    QTest::keyClick(about, Qt::Key_Home);
    QCOMPARE(y(), 0.0);
    QTest::keyClick(about, Qt::Key_Up); // at the start: stays
    QCOMPARE(y(), 0.0);
    // No bounce: the page stops at its ends.
    QCOMPARE(flick->property("boundsBehavior").toInt(), 0); // Flickable.StopAtBounds
    auto* bar = about->findChild<QQuickItem*>(QStringLiteral("licenceScrollBar"));
    QVERIFY(bar && bar->isVisible());
    QVERIFY(QMetaObject::invokeMethod(about, "close"));
}

QTEST_MAIN(TestMainWindow)
#include "test_main_window.moc"
