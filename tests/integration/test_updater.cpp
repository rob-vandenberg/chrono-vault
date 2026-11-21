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

#include "updater.h"
#include "version.h"

#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_updater.cpp 0.0.5

// --- Version history ----------------------------------------------------
// v0.0.5: Initial release

using namespace chrono_vault;
using State = Updater::State;

namespace {

QByteArray releaseWith(const QString& tag, const QString& file)
{
    return QString::fromUtf8(R"([{"tag_name":"%1","html_url":"https://github.com/x","assets":[{"name":"%2","browser_download_url":"https://github.com/x/%2","size":10,"digest":"sha256:%3"}]}])")
        .arg(tag, file, QString(64, QLatin1Char('c')))
        .toUtf8();
}

} // namespace

// The update (D-021), with GitHub's answer prepared: no test needs the network.
class TestUpdater : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void only_an_installed_copy_counts_as_installed();
    void the_fingerprint_of_a_file_is_its_sha256();
    void a_newer_version_for_this_system_is_offered_and_can_be_skipped();
    void nothing_newer_says_this_is_the_newest();
    void a_copy_not_installed_is_told_so_without_the_network();
    void a_refused_install_can_be_tried_again();
    void a_broken_off_download_can_be_tried_again();
};

void TestUpdater::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Chrono Vault Tests"));
    QCoreApplication::setApplicationName(QStringLiteral("test_updater"));
    QSettings().clear();
}

void TestUpdater::cleanupTestCase()
{
    QSettings settings;
    settings.clear();
    settings.sync();
    QFile::remove(settings.fileName());
}

void TestUpdater::only_an_installed_copy_counts_as_installed()
{
#ifdef Q_OS_WIN
    QCOMPARE(Updater::installKindFrom(QStringLiteral("C:/Program Files/Chrono Vault"), false, false, true), InstallKind::Windows);
    QCOMPARE(Updater::installKindFrom(QStringLiteral("C:/c++/chrono-vault/build"), false, false, false), InstallKind::None);
#else
    QCOMPARE(Updater::installKindFrom(QStringLiteral("/opt/chrono-vault"), true, false, false), InstallKind::Deb);
    QCOMPARE(Updater::installKindFrom(QStringLiteral("/opt/chrono-vault"), false, true, false), InstallKind::Rpm);
    QCOMPARE(Updater::installKindFrom(QStringLiteral("/opt/chrono-vault"), false, false, false), InstallKind::None);
    // A build in a project folder, even with the package installed beside it.
    QCOMPARE(Updater::installKindFrom(QStringLiteral("/home/someone/chrono-vault/build/src/app"), true, false, false),
             InstallKind::None);
#endif
}

void TestUpdater::the_fingerprint_of_a_file_is_its_sha256()
{
    QTemporaryDir folder;
    const QString path = folder.filePath(QStringLiteral("file"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("abc");
    file.close();
    QCOMPARE(Updater::fingerprintOf(path), QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    QVERIFY(Updater::fingerprintOf(folder.filePath(QStringLiteral("missing"))).isEmpty());
}

void TestUpdater::a_newer_version_for_this_system_is_offered_and_can_be_skipped()
{
    Updater updater;
    updater.setInstallKindForTests(InstallKind::Deb);
    updater.answerForTests(releaseWith(QStringLiteral("99.0.0"), QStringLiteral("chrono-vault_99.0.0_amd64.deb")));
    QCOMPARE(updater.state(), State::Available);
    QCOMPARE(updater.newVersion(), QStringLiteral("99.0.0"));

    updater.skipThisVersion();
    QCOMPARE(updater.state(), State::Idle);
    QCOMPARE(QSettings().value(QStringLiteral("updates/skipped")).toString(), QStringLiteral("99.0.0"));
}

void TestUpdater::nothing_newer_says_this_is_the_newest()
{
    Updater updater;
    updater.setInstallKindForTests(InstallKind::Rpm);
    updater.answerForTests(releaseWith(programVersion(), QStringLiteral("chrono-vault_x_x86_64.rpm")));
    QCOMPARE(updater.state(), State::Current);
    QVERIFY(updater.message().contains(programVersion()));
    // A newer release carrying only another system's file is no update here.
    updater.answerForTests(releaseWith(QStringLiteral("99.0.0"), QStringLiteral("chrono-vault_99.0.0_amd64.deb")));
    QCOMPARE(updater.state(), State::Current);
}

void TestUpdater::a_refused_install_can_be_tried_again()
{
    Updater updater;
    updater.setInstallKindForTests(InstallKind::Deb);
    // The person cancels the system's password prompt: the checked file stays
    // and Install is offered again, without a new download.
    updater.installAnsweredForTests(false, QStringLiteral("Not authorized"));
    QCOMPARE(updater.state(), State::Downloaded);
    QVERIFY(updater.message().contains(QStringLiteral("Not authorized")));
    QVERIFY(updater.message().contains(QStringLiteral("try again")));
}

void TestUpdater::a_broken_off_download_can_be_tried_again()
{
    Updater updater;
    updater.setInstallKindForTests(InstallKind::Deb);
    // A file address on this computer where nothing answers: the download
    // breaks off at once, as when the connection drops.
    updater.answerForTests(QString::fromUtf8(R"([{"tag_name":"99.0.0","html_url":"https://github.com/x","assets":[{"name":"chrono-vault_99.0.0_amd64.deb","browser_download_url":"https://127.0.0.1:9/chrono-vault_99.0.0_amd64.deb","size":10,"digest":"sha256:%1"}]}])")
                               .arg(QString(64, QLatin1Char('c')))
                               .toUtf8());
    QCOMPARE(updater.state(), State::Available);
    updater.download();
    QTRY_VERIFY(updater.state() != State::Downloading);
    QCOMPARE(updater.state(), State::Available);
    QVERIFY(updater.message().contains(QStringLiteral("try again")));
    QCOMPARE(updater.newVersion(), QStringLiteral("99.0.0"));
}

void TestUpdater::a_copy_not_installed_is_told_so_without_the_network()
{
    Updater updater;
    updater.setInstallKindForTests(InstallKind::None);
    updater.checkNow();
    QCOMPARE(updater.state(), State::Failed);
    QVERIFY(updater.message().contains(QStringLiteral("not installed with an installer")));
}

QTEST_MAIN(TestUpdater)
#include "test_updater.moc"
