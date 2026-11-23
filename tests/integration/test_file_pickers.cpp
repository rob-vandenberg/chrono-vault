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
#include "file_pickers.h"
#include "places_here.h"
#include "settings_files.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

// --- Version ------------------------------------------------------------
// test_file_pickers.cpp 0.2.51

// --- Version history ----------------------------------------------------
// v0.2.51: Initial release (D-055)

using namespace chrono_vault;

namespace {
// A desktop's list of places, as Dolphin writes it: a cloud drive, a hidden
// place, the trash, and one folder written twice.
const char* const samplePlaces =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<xbel>\n"
    " <bookmark href=\"file:///home/someone/CloudDrive\"><title>Cloud</title></bookmark>\n"
    " <bookmark href=\"file:///home/someone/Hidden\"><info><metadata><IsHidden>true</IsHidden></metadata></info>"
    "</bookmark>\n"
    " <bookmark href=\"trash:/\"><title>Trash</title></bookmark>\n"
    " <bookmark href=\"file:///home/someone/Documents/\"><title>Documents</title></bookmark>\n"
    " <bookmark href=\"file:///home/someone/Documents\"><title>Again</title></bookmark>\n"
    "</xbel>\n";
} // namespace


// The file dialogs as Chrono Viewer's (D-055): the desktop's own places,
// the filter, the starting file and the size kept.
class TestFilePickers : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void the_places_are_the_desktops_own_list()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString list = folder.filePath(QStringLiteral("user-places.xbel"));
        QFile file(list);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(samplePlaces);
        file.close();
        // Only folders on this machine, not the hidden one, each once.
        QCOMPARE(places_here::placesInFile(list),
                 QList<QUrl>({QUrl::fromLocalFile(QStringLiteral("/home/someone/CloudDrive")),
                              QUrl::fromLocalFile(QStringLiteral("/home/someone/Documents"))}));
        // A damaged or missing file gives nothing, never a crash (OR-4).
        QVERIFY(places_here::placesInFile(folder.filePath(QStringLiteral("missing.xbel"))).isEmpty());
        QFile broken(folder.filePath(QStringLiteral("broken.xbel")));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("<xbel><bookmark href=");
        broken.close();
        QVERIFY(places_here::placesInFile(broken.fileName()).isEmpty());
    }

    void a_dialog_shows_the_places_the_filter_and_the_file_and_keeps_its_size()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString wanted = folder.filePath(QStringLiteral("Mine.chronovault"));
        FilePickers pickers;
        QStringList filters;
        QString selected;
        bool ownWindow = false;
        QList<QUrl> sidebar;
        QTimer::singleShot(0, [&]() {
            auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            ownWindow = dialog->testOption(QFileDialog::DontUseNativeDialog);
            filters = dialog->nameFilters();
            selected = dialog->selectedFiles().value(0);
            sidebar = dialog->sidebarUrls();
            dialog->resize(700, 500);
            static_cast<QDialog*>(dialog)->accept(); // as the Save button does
        });
        const QUrl chosen = pickers.pick(QStringLiteral("test.save"), QStringLiteral("Save"), true,
                                         {QStringLiteral("Chrono Vault vaults (*.chronovault)")},
                                         QStringLiteral("chronovault"), QUrl::fromLocalFile(wanted), QUrl(), nullptr);
        QVERIFY(ownWindow);
        QCOMPARE(filters, QStringList{QStringLiteral("Chrono Vault vaults (*.chronovault)")});
        QCOMPARE(selected, wanted);
        QCOMPARE(sidebar, places_here::everyPlace());
        QCOMPARE(chosen, QUrl::fromLocalFile(wanted));

        // Cancelled: nothing chosen; it opens at the size it was left at.
        QSize size;
        QTimer::singleShot(0, [&]() {
            auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            size = dialog->size();
            dialog->reject();
        });
        QVERIFY(pickers.pick(QStringLiteral("test.save"), QStringLiteral("Save"), true, {}, QString(), QUrl(),
                             QUrl::fromLocalFile(folder.path()), nullptr)
                    .isEmpty());
        QCOMPARE(size, QSize(700, 500));
    }
};

QTEST_MAIN(TestFilePickers)
#include "test_file_pickers.moc"
