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

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_no_bounce.cpp 0.7.112

// --- Version history ----------------------------------------------------
// v0.7.112: Initial release (D-046)

// D-046: nothing bounces. Every ListView, Flickable and GridView of ours stops
// at its ends, and so do the lists Qt's style builds for menus and choice boxes.
namespace {

// The block that starts at the opening brace on line `first`, up to its
// closing brace; empty when the file ends before it closes.
QString blockFrom(const QStringList& lines, qsizetype first)
{
    QString block;
    int depth = 0;
    for (qsizetype index = first; index < lines.size(); ++index) {
        const QString& line = lines.at(index);
        block += line + QLatin1Char('\n');
        depth += int(line.count(QLatin1Char('{'))) - int(line.count(QLatin1Char('}')));
        if (depth <= 0)
            return block;
    }
    return {};
}

// The text of a QML file, or an empty string when it cannot be read.
QString textOf(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

}  // namespace

class TestNoBounce : public QObject {
    Q_OBJECT

private slots:
    void every_list_of_ours_stops_at_its_ends();
    void the_style_s_menus_and_choice_boxes_stop_at_their_ends();
};

void TestNoBounce::every_list_of_ours_stops_at_its_ends()
{
    // A list declared on a line of its own, or as a property's value.
    static const QRegularExpression list(QStringLiteral(
        "^\\s*(?:[A-Za-z.]+\\s*:\\s*)?(ListView|Flickable|GridView)\\s*\\{"));
    QStringList offenders;
    int found = 0;
    const QString root = QStringLiteral(CHRONO_VAULT_SOURCE_DIR);
    for (const QString& folder : {QStringLiteral("qml"), QStringLiteral("style")}) {
        const QDir dir(root + QLatin1Char('/') + folder);
        const QStringList files = dir.entryList({QStringLiteral("*.qml")}, QDir::Files);
        QVERIFY2(!files.isEmpty(), qPrintable(dir.path()));
        for (const QString& name : files) {
            const QStringList lines = textOf(dir.filePath(name)).split(QLatin1Char('\n'));
            for (qsizetype index = 0; index < lines.size(); ++index) {
                const auto match = list.match(lines.at(index));
                if (!match.hasMatch())
                    continue;
                ++found;
                if (!blockFrom(lines, index).contains(QStringLiteral("Flickable.StopAtBounds")))
                    offenders << QStringLiteral("%1/%2:%3 %4 bounces")
                                     .arg(folder, name)
                                     .arg(index + 1)
                                     .arg(match.captured(1));
            }
        }
    }
    QVERIFY(found > 0);
    QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1Char('\n'))));
}

void TestNoBounce::the_style_s_menus_and_choice_boxes_stop_at_their_ends()
{
    // Their lists are built by Qt's style, so they are stopped from outside;
    // so are the scroll views' (D-046).
    const QString root = QStringLiteral(CHRONO_VAULT_SOURCE_DIR "/style/");
    for (const QString& name : {QStringLiteral("Menu.qml"), QStringLiteral("ComboBox.qml"),
                               QStringLiteral("ScrollView.qml")}) {
        const QString text = textOf(root + name);
        QVERIFY2(!text.isEmpty(), qPrintable(name));
        QVERIFY2(text.contains(QStringLiteral("\"boundsBehavior\"")) &&
                     text.contains(QStringLiteral("Flickable.StopAtBounds")),
                 qPrintable(name + QStringLiteral(" bounces")));
    }
}

QTEST_GUILESS_MAIN(TestNoBounce)
#include "test_no_bounce.moc"
