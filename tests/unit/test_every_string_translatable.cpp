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
// test_every_string_translatable.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

// D-008: every text the person can see goes through the translation call.
// In the windows that means: a property that shows words (text, title,
// placeholderText, ...) is never given a bare string, only qsTr("...").
class TestEveryStringTranslatable : public QObject
{
    Q_OBJECT

private slots:
    void every_visible_text_in_the_windows_goes_through_qsTr();
};

void TestEveryStringTranslatable::every_visible_text_in_the_windows_goes_through_qsTr()
{
    const QDir qmlFolder(QStringLiteral(CHRONO_VAULT_SOURCE_DIR "/qml"));
    const QStringList qmlFiles = qmlFolder.entryList({QStringLiteral("*.qml")}, QDir::Files);
    QVERIFY2(!qmlFiles.isEmpty(), "No QML files found");

    static const QRegularExpression bareText(
        QStringLiteral(R"(\b(text|title|placeholderText|label|description)\s*:\s*"[^"]*[A-Za-z][^"]*")"));

    QStringList offenders;
    for (const QString& name : qmlFiles) {
        QFile file(qmlFolder.filePath(name));
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(name));
        int lineNumber = 0;
        while (!file.atEnd()) {
            ++lineNumber;
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (line.startsWith(QLatin1String("//"))) {
                continue;
            }
            if (bareText.match(line).hasMatch()) {
                offenders << QStringLiteral("%1:%2: %3").arg(name).arg(lineNumber).arg(line);
            }
        }
    }
    QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1Char('\n'))));
}

QTEST_GUILESS_MAIN(TestEveryStringTranslatable)
#include "test_every_string_translatable.moc"
