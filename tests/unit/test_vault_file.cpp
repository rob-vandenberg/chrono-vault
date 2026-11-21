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

#include "vault_file.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_vault_file.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

using namespace chrono_vault;

namespace {

const crypto::KdfParameters quick{1024, 1, 1};

QByteArray readAll(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeAll(const QString& path, const QByteArray& data)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(data), data.size());
}

} // namespace

class TestVaultFile : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void a_saved_vault_opens_with_its_password();
    void a_saved_vault_opens_again_and_again_after_new_saves();
    void a_wrong_password_does_not_open();
    void a_missing_file_says_it_is_missing();
    void a_file_of_another_kind_is_not_a_vault();
    void a_newer_format_is_refused();
    void a_truncated_vault_is_damaged();
    void a_changed_byte_anywhere_is_damaged();
    void a_header_asking_for_too_much_memory_is_damaged_not_tried();
    void an_interrupted_save_leaves_the_old_vault_whole_data();
    void an_interrupted_save_leaves_the_old_vault_whole();
    void a_left_over_saving_file_does_not_stop_the_next_save();
    void a_save_to_a_folder_that_does_not_exist_fails_and_leaves_nothing();

private:
    QTemporaryDir* folder = nullptr;
    QString vaultPath;
    VaultKey key;
};

void TestVaultFile::init()
{
    folder = new QTemporaryDir;
    QVERIFY(folder->isValid());
    vaultPath = folder->filePath(QStringLiteral("test.chronovault"));
    key = makeVaultKey("master password", quick);
    QVERIFY(key.isValid());
}

void TestVaultFile::cleanup()
{
    delete folder;
    folder = nullptr;
}

void TestVaultFile::a_saved_vault_opens_with_its_password()
{
    QCOMPARE(saveVaultFile(vaultPath, "contents", key), SaveStatus::Saved);
    const OpenResult result = openVaultFile(vaultPath, "master password");
    QCOMPARE(result.status, OpenStatus::Opened);
    QCOMPARE(result.contents, QByteArray("contents"));
    QCOMPARE(result.key.key, key.key);
    QVERIFY(!QFile::exists(savingPathFor(vaultPath)));
}

void TestVaultFile::a_saved_vault_opens_again_and_again_after_new_saves()
{
    for (int round = 0; round < 3; ++round) {
        const QByteArray contents = "contents " + QByteArray::number(round);
        QCOMPARE(saveVaultFile(vaultPath, contents, key), SaveStatus::Saved);
        const OpenResult result = openVaultFile(vaultPath, "master password");
        QCOMPARE(result.status, OpenStatus::Opened);
        QCOMPARE(result.contents, contents);
    }
}

void TestVaultFile::a_wrong_password_does_not_open()
{
    QCOMPARE(saveVaultFile(vaultPath, "contents", key), SaveStatus::Saved);
    const OpenResult result = openVaultFile(vaultPath, "Master password");
    QCOMPARE(result.status, OpenStatus::WrongPasswordOrDamaged);
    QVERIFY(result.contents.isEmpty());
    QVERIFY(!result.key.isValid());
}

void TestVaultFile::a_missing_file_says_it_is_missing()
{
    QCOMPARE(openVaultFile(vaultPath, "master password").status, OpenStatus::FileMissing);
}

void TestVaultFile::a_file_of_another_kind_is_not_a_vault()
{
    writeAll(vaultPath, "just some text that is not a vault at all");
    QCOMPARE(openVaultFile(vaultPath, "master password").status, OpenStatus::NotAVault);
    writeAll(vaultPath, QByteArray());
    QCOMPARE(openVaultFile(vaultPath, "master password").status, OpenStatus::NotAVault);
}

void TestVaultFile::a_newer_format_is_refused()
{
    QCOMPARE(saveVaultFile(vaultPath, "contents", key), SaveStatus::Saved);
    QByteArray data = readAll(vaultPath);
    qToBigEndian<quint16>(2, data.data() + 8);
    writeAll(vaultPath, data);
    QCOMPARE(openVaultFile(vaultPath, "master password").status, OpenStatus::NewerFormat);
}

void TestVaultFile::a_truncated_vault_is_damaged()
{
    QCOMPARE(saveVaultFile(vaultPath, "contents", key), SaveStatus::Saved);
    const QByteArray data = readAll(vaultPath);
    for (const qsizetype length : {qsizetype(8), qsizetype(20), qsizetype(40), qsizetype(60), data.size() - 1}) {
        writeAll(vaultPath, data.left(length));
        QCOMPARE(openVaultFile(vaultPath, "master password").status, OpenStatus::WrongPasswordOrDamaged);
    }
}

void TestVaultFile::a_changed_byte_anywhere_is_damaged()
{
    QCOMPARE(saveVaultFile(vaultPath, "contents", key), SaveStatus::Saved);
    const QByteArray data = readAll(vaultPath);
    // Past the 8 letters that say "this is a vault".
    for (qsizetype index = 8; index < data.size(); ++index) {
        QByteArray damaged = data;
        damaged[index] = static_cast<char>(damaged[index] ^ 0x01);
        writeAll(vaultPath, damaged);
        const OpenStatus status = openVaultFile(vaultPath, "master password").status;
        QVERIFY2(status == OpenStatus::WrongPasswordOrDamaged || status == OpenStatus::NewerFormat,
                 qPrintable(QString::number(index)));
    }
}

void TestVaultFile::a_header_asking_for_too_much_memory_is_damaged_not_tried()
{
    QCOMPARE(saveVaultFile(vaultPath, "contents", key), SaveStatus::Saved);
    QByteArray data = readAll(vaultPath);
    qToBigEndian<quint32>(0xFFFFFFFFu, data.data() + 12);
    writeAll(vaultPath, data);
    QCOMPARE(openVaultFile(vaultPath, "master password").status, OpenStatus::WrongPasswordOrDamaged);
}

void TestVaultFile::an_interrupted_save_leaves_the_old_vault_whole_data()
{
    QTest::addColumn<SaveStep>("step");
    QTest::newRow("halfway through writing") << SaveStep::WritingHalf;
    QTest::newRow("written, not flushed") << SaveStep::Written;
    QTest::newRow("flushed, not verified") << SaveStep::Flushed;
    QTest::newRow("verified, not replaced") << SaveStep::Verified;
}

void TestVaultFile::an_interrupted_save_leaves_the_old_vault_whole()
{
    QFETCH(SaveStep, step);
    QCOMPARE(saveVaultFile(vaultPath, "the old contents", key), SaveStatus::Saved);

    const SaveStatus status = saveVaultFile(vaultPath, "the new contents", key,
                                            [step](SaveStep reached) { return reached != step; });
    QCOMPARE(status, SaveStatus::Interrupted);

    const OpenResult result = openVaultFile(vaultPath, "master password");
    QCOMPARE(result.status, OpenStatus::Opened);
    QCOMPARE(result.contents, QByteArray("the old contents"));
}

void TestVaultFile::a_left_over_saving_file_does_not_stop_the_next_save()
{
    QCOMPARE(saveVaultFile(vaultPath, "the old contents", key), SaveStatus::Saved);
    writeAll(savingPathFor(vaultPath), "half a file left by a crash");
    QCOMPARE(saveVaultFile(vaultPath, "the new contents", key), SaveStatus::Saved);
    QCOMPARE(openVaultFile(vaultPath, "master password").contents, QByteArray("the new contents"));
    QVERIFY(!QFile::exists(savingPathFor(vaultPath)));
}

void TestVaultFile::a_save_to_a_folder_that_does_not_exist_fails_and_leaves_nothing()
{
    const QString path = folder->filePath(QStringLiteral("no such folder/test.chronovault"));
    QCOMPARE(saveVaultFile(path, "contents", key), SaveStatus::CannotWrite);
    QVERIFY(!QFile::exists(path));
}

QTEST_GUILESS_MAIN(TestVaultFile)
#include "test_vault_file.moc"
