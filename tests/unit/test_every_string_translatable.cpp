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
// test_every_string_translatable.cpp 0.4.72

// --- Version history ----------------------------------------------------
// v0.4.72: Also texts inside ? : choices, on their continuation lines and in lists
// v0.0.2: Initial release

// D-008: every text the person can see goes through the translation call.
// In the windows that means: a property that shows words (text, title,
// placeholderText, ...) is never given a bare string, only qsTr("...").
namespace {

// A visible text left bare in a choice (a ? "…" : …), on its continuation
// line, or in a list: what is not inside qsTr, nor compared with a value.
bool bareTextIn(const QString& line)
{
    static const QRegularExpression visible(QStringLiteral(
        "^(text|title|placeholderText|label|description|ToolTip\\.text|Accessible\\.name|Accessible\\.description)\\s*:"));
    static const QRegularExpression translated(QStringLiteral("qsTr\\(\"(?:[^\"\\\\]|\\\\.)*\""));
    static const QRegularExpression compared(QStringLiteral("(===|!==|==|!=)\\s*\"(?:[^\"\\\\]|\\\\.)*\""));
    static const QRegularExpression literal(QStringLiteral("\"((?:[^\"\\\\]|\\\\.)*)\""));
    static const QRegularExpression letter(QStringLiteral("[A-Za-z]"));
    static const QRegularExpression words(QStringLiteral("[A-Za-z]{2,} [A-Za-z]{2,}"));
    QString rest = line;
    rest.remove(translated).remove(compared);
    const bool onVisible = visible.match(line).hasMatch();
    if (!onVisible && !line.startsWith(QLatin1Char(':')) && !line.startsWith(QLatin1Char('?')) &&
        !line.contains(QLatin1Char('['))) {
        return false;
    }
    auto found = literal.globalMatch(rest);
    while (found.hasNext()) {
        const QString text = found.next().captured(1);
        if (onVisible ? text.contains(letter) : text.contains(words)) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestEveryStringTranslatable : public QObject
{
    Q_OBJECT

private slots:
    void every_visible_text_in_the_windows_goes_through_qsTr();
    void the_rule_for_choices_and_lists_finds_bare_texts();
    void texts_in_choices_and_lists_go_through_qsTr_too();
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

void TestEveryStringTranslatable::the_rule_for_choices_and_lists_finds_bare_texts()
{
    QVERIFY(bareTextIn(QStringLiteral("text: open ? \"Close\" : qsTr(\"Open\")")));
    QVERIFY(bareTextIn(QStringLiteral(": \"Choose the folder first\"")));
    QVERIFY(bareTextIn(QStringLiteral("model: [\"Every day\", qsTr(\"Never\")]")));
    QVERIFY(!bareTextIn(QStringLiteral("text: mode === \"open\" ? qsTr(\"Open\") : qsTr(\"Save\")")));
    QVERIFY(!bareTextIn(QStringLiteral("color: chosen ? palette.highlight : \"transparent\"")));
    QVERIFY(!bareTextIn(QStringLiteral("sequences: [\"Return\", \"Enter\"]")));
}

void TestEveryStringTranslatable::texts_in_choices_and_lists_go_through_qsTr_too()
{
    const QDir qmlFolder(QStringLiteral(CHRONO_VAULT_SOURCE_DIR "/qml"));
    const QStringList qmlFiles = qmlFolder.entryList({QStringLiteral("*.qml")}, QDir::Files);
    QVERIFY2(!qmlFiles.isEmpty(), "No QML files found");
    QStringList offenders;
    for (const QString& name : qmlFiles) {
        QFile file(qmlFolder.filePath(name));
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(name));
        int lineNumber = 0;
        while (!file.atEnd()) {
            ++lineNumber;
            const QString line = QString::fromUtf8(file.readLine()).trimmed();
            if (!line.startsWith(QLatin1String("//")) && bareTextIn(line)) {
                offenders << QStringLiteral("%1:%2: %3").arg(name).arg(lineNumber).arg(line);
            }
        }
    }
    QVERIFY2(offenders.isEmpty(), qPrintable(offenders.join(QLatin1Char('\n'))));
}

QTEST_GUILESS_MAIN(TestEveryStringTranslatable)
#include "test_every_string_translatable.moc"
