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

#include "item_icons.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_item_icons.cpp 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: Initial release

using namespace chrono_vault;

// A picked picture becomes an item icon (D-018).
class TestItemIcons : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void every_allowed_format_becomes_a_64_pixel_png_data();
    void every_allowed_format_becomes_a_64_pixel_png();
    void a_wide_picture_keeps_its_shape();
    void a_file_over_5_mb_is_refused();
    void a_damaged_file_is_refused();
    void another_format_is_refused();
    void a_missing_file_is_said_to_be_missing();

private:
    QString writePicture(const QString& name, const char* format, QSize size = QSize(100, 100));
    QTemporaryDir* folder = nullptr;
};

void TestItemIcons::init()
{
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
}

void TestItemIcons::cleanup()
{
    delete folder;
    folder = nullptr;
}

QString TestItemIcons::writePicture(const QString& name, const char* format, QSize size)
{
    QImage picture(size, QImage::Format_ARGB32);
    picture.fill(QColor(0x2b, 0x6c, 0xb0));
    const QString path = folder->filePath(name);
    if (!picture.save(path, format)) {
        return {};
    }
    return path;
}

void TestItemIcons::every_allowed_format_becomes_a_64_pixel_png_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QByteArray>("format");
    QTest::newRow("jpg") << QStringLiteral("picture.jpg") << QByteArray("JPG");
    QTest::newRow("jpeg") << QStringLiteral("picture.jpeg") << QByteArray("JPG");
    QTest::newRow("png") << QStringLiteral("picture.png") << QByteArray("PNG");
    QTest::newRow("bmp") << QStringLiteral("picture.bmp") << QByteArray("BMP");
    QTest::newRow("ico") << QStringLiteral("picture.ico") << QByteArray("ICO");
    QTest::newRow("svg") << QStringLiteral("picture.svg") << QByteArray();
}

void TestItemIcons::every_allowed_format_becomes_a_64_pixel_png()
{
    QFETCH(QString, name);
    QFETCH(QByteArray, format);
    QString path;
    if (format.isEmpty()) {
        path = folder->filePath(name);
        QFile svg(path);
        QVERIFY(svg.open(QIODevice::WriteOnly));
        svg.write(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 10 10"><rect width="10" height="10" fill="#2b6cb0"/></svg>)");
    } else {
        path = writePicture(name, format.constData(), QSize(48, 48));
    }
    QVERIFY(!path.isEmpty());

    const IconResult result = readIcon(path);
    QCOMPARE(result.status, IconStatus::Read);
    QVERIFY(result.png.startsWith("\x89PNG"));
    const QImage icon = QImage::fromData(result.png, "PNG");
    QCOMPARE(icon.size(), QSize(iconSide, iconSide));
}

void TestItemIcons::a_wide_picture_keeps_its_shape()
{
    const QString path = writePicture(QStringLiteral("wide.png"), "PNG", QSize(1000, 500));
    const IconResult result = readIcon(path);
    QCOMPARE(result.status, IconStatus::Read);
    const QImage icon = QImage::fromData(result.png, "PNG");
    QCOMPARE(icon.size(), QSize(iconSide, iconSide));
    // 64 wide and 32 high, in the middle: clear above, filled in the centre.
    QCOMPARE(qAlpha(icon.pixel(32, 2)), 0);
    QVERIFY(qAlpha(icon.pixel(32, 32)) > 0);
}

void TestItemIcons::a_file_over_5_mb_is_refused()
{
    const QString path = folder->filePath(QStringLiteral("huge.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(largestIconFileBytes + 1));
    file.close();
    QCOMPARE(readIcon(path).status, IconStatus::TooLarge);
}

void TestItemIcons::a_damaged_file_is_refused()
{
    const QString path = folder->filePath(QStringLiteral("damaged.png"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("\x89PNG\r\n\x1a\n then nothing a picture needs");
    file.close();
    const IconResult result = readIcon(path);
    QCOMPARE(result.status, IconStatus::NotAPicture);
    QVERIFY(result.png.isEmpty());
}

void TestItemIcons::another_format_is_refused()
{
    const QString path = folder->filePath(QStringLiteral("picture.gif"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("GIF89a");
    file.close();
    QCOMPARE(readIcon(path).status, IconStatus::NotSupported);
}

void TestItemIcons::a_missing_file_is_said_to_be_missing()
{
    QCOMPARE(readIcon(folder->filePath(QStringLiteral("nothing.png"))).status, IconStatus::NotFound);
}

QTEST_MAIN(TestItemIcons)
#include "test_item_icons.moc"
