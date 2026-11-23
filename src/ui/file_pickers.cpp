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

#include "appearance.h"
#include "places_here.h"
#include "settings_files.h"

#include <QDir>
#include <QFileDialog>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QWindow>

// --- Version ------------------------------------------------------------
// file_pickers.cpp 0.2.51

// --- Version history ----------------------------------------------------
// v0.2.51: Initial release, after Chrono Viewer's file pickers (D-055)

namespace chrono_vault {

namespace {

// A dialog first opens at 880 by 640 times the UI scaling, as Chrono
// Viewer's; a size the person left it at wins.
constexpr int firstWidth = 880;
constexpr int firstHeight = 640;

QString memoryKey(const QString& memoryName)
{
    return QStringLiteral("fileDialogs/") + (memoryName.isEmpty() ? QStringLiteral("file") : memoryName);
}

// The window a dialog opens over, from an item or a window.
QWindow* windowOf(QObject* object)
{
    if (auto* item = qobject_cast<QQuickItem*>(object)) {
        return item->window();
    }
    return qobject_cast<QWindow*>(object);
}

// The dialog that is open now, if any.
QPointer<QFileDialog>& openDialog()
{
    static QPointer<QFileDialog> dialog;
    return dialog;
}

// Fusion, as every window of Chrono Vault (D-017); one for all dialogs.
QStyle* fusion()
{
    static QStyle* style = QStyleFactory::create(QStringLiteral("Fusion"));
    return style;
}

} // namespace

QUrl FilePickers::pick(const QString& memoryName, const QString& title, bool save, const QStringList& nameFilters,
                       const QString& defaultSuffix, const QUrl& selectedFile, const QUrl& folder, QObject* window)
{
    QFileDialog dialog;
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    if (QStyle* style = fusion()) {
        dialog.setStyle(style);
    }
    dialog.setWindowTitle(title);
    dialog.setAcceptMode(save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
    dialog.setFileMode(save ? QFileDialog::AnyFile : QFileDialog::ExistingFile);
    if (!nameFilters.isEmpty()) {
        dialog.setNameFilters(nameFilters);
    }
    if (!defaultSuffix.isEmpty()) {
        dialog.setDefaultSuffix(defaultSuffix);
    }
    if (selectedFile.isLocalFile()) {
        dialog.setDirectory(QFileInfo(selectedFile.toLocalFile()).absolutePath());
        dialog.selectFile(selectedFile.toLocalFile());
    } else {
        dialog.setDirectory(folder.isLocalFile() ? folder.toLocalFile() : QDir::homePath());
    }
    dialog.setSidebarUrls(places_here::everyPlace());

    // Over the window it was opened from, and put back as it was left.
    dialog.winId();
    if (QWindow* over = windowOf(window); over != nullptr && dialog.windowHandle() != nullptr) {
        dialog.windowHandle()->setTransientParent(over);
    }
    const QByteArray kept = SettingsFile().value(memoryKey(memoryName)).toByteArray();
    if (kept.isEmpty() || !dialog.restoreGeometry(kept)) {
        const double factor = Appearance::factor();
        dialog.resize(qRound(firstWidth * factor), qRound(firstHeight * factor));
    }
    places_here::showEveryPlaceWhole(dialog);

    openDialog() = &dialog;
    const bool chosen = dialog.exec() == QDialog::Accepted;
    openDialog() = nullptr;
    SettingsFile().setValue(memoryKey(memoryName), dialog.saveGeometry());
    if (!chosen || dialog.selectedUrls().isEmpty()) {
        return {};
    }
    return dialog.selectedUrls().constFirst();
}

void FilePickers::closeOpenDialog()
{
    if (QFileDialog* dialog = openDialog()) {
        dialog->reject();
    }
}

QUrl FilePickers::homeFolder() const
{
    return QUrl::fromLocalFile(QDir::homePath());
}

} // namespace chrono_vault
