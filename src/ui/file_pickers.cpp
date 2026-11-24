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
#include "quick_unlock.h"
#include "settings_files.h"

#include <QDir>
#include <QFileDialog>
#include <QGuiApplication>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// --- Version ------------------------------------------------------------
// file_pickers.cpp 0.5.80

// --- Version history ----------------------------------------------------
// v0.5.80: Only a comment: the dialog's invisibility had another cause (D-055)
// v0.5.79: Explorer's dialog gets its owner from Qt (D-055)
// v0.5.78.2: The folder the import's file dialog opens in, in the --debug log (a debug version)
// v0.5.78.1: Import's file dialog in the --debug log: the windows before, a second after it opens, and after (a debug version)
// v0.3.64: The default folder of a new vault, Documents/chrono-vault (D-058)
// v0.2.52: Explorer's own dialog on Windows (D-055)
// v0.2.51: Initial release, after Chrono Viewer's file pickers (D-055)

namespace chrono_vault {

namespace {

// On Windows the dialog is Explorer's own, which brings its own places, look
// and size; elsewhere it is Qt's own window, as Chrono Viewer's (D-055).
#ifdef Q_OS_WIN
constexpr bool explorersOwn = true;
#else
constexpr bool explorersOwn = false;
#endif

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

// --- The --debug log of Import's file dialog (debug version 0.5.78.1) ---

QString windowWords(const QWindow* window)
{
    return window != nullptr ? QStringLiteral("\"%1\"").arg(window->title()) : QStringLiteral("none");
}

#ifdef Q_OS_WIN
QString nativeWords(HWND window)
{
    if (window == nullptr) {
        return QStringLiteral("none");
    }
    wchar_t title[256] = {};
    GetWindowTextW(window, title, 256);
    wchar_t className[128] = {};
    GetClassNameW(window, className, 128);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    RECT place{};
    GetWindowRect(window, &place);
    return QStringLiteral("0x%1 \"%2\" class %3, %4, %5, at %6,%7 size %8x%9, owner 0x%10")
        .arg(reinterpret_cast<quintptr>(window), 0, 16)
        .arg(QString::fromWCharArray(title), QString::fromWCharArray(className))
        .arg(process == GetCurrentProcessId() ? QStringLiteral("ours") : QStringLiteral("another program"))
        .arg(IsWindowVisible(window) != 0 ? QStringLiteral("visible") : QStringLiteral("hidden"))
        .arg(place.left)
        .arg(place.top)
        .arg(place.right - place.left)
        .arg(place.bottom - place.top)
        .arg(reinterpret_cast<quintptr>(GetWindow(window, GW_OWNER)), 0, 16);
}

BOOL CALLBACK logOurWindow(HWND window, LPARAM)
{
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process == GetCurrentProcessId()) {
        fingerLog(QStringLiteral("import:   our window %1%2")
                      .arg(nativeWords(window))
                      .arg(GetActiveWindow() == window ? QStringLiteral(", active") : QString()));
    }
    return TRUE;
}
#endif

void logWindows(const QString& step, const QWindow* watched)
{
    // Qt's active window is the one with the keyboard.
    QString line = QStringLiteral("import: %1; Qt's active window %2")
                       .arg(step, windowWords(QGuiApplication::focusWindow()));
    if (watched != nullptr) {
        line += QStringLiteral("; Import window %1").arg(watched->isVisible() ? QStringLiteral("visible") : QStringLiteral("hidden"));
    }
#ifdef Q_OS_WIN
    line += QStringLiteral("; in front %1").arg(nativeWords(GetForegroundWindow()));
#endif
    fingerLog(line);
}

void logEveryWindow()
{
#ifdef Q_OS_WIN
    fingerLog(QStringLiteral("import: a second after the file dialog opened, in front %1").arg(nativeWords(GetForegroundWindow())));
    EnumWindows(logOurWindow, 0);
#else
    logWindows(QStringLiteral("a second after the file dialog opened"), nullptr);
#endif
}

} // namespace

QUrl FilePickers::pick(const QString& memoryName, const QString& title, bool save, const QStringList& nameFilters,
                       const QString& defaultSuffix, const QUrl& selectedFile, const QUrl& folder, QObject* window)
{
    QFileDialog dialog;
    dialog.setOption(QFileDialog::DontUseNativeDialog, !explorersOwn);
    if (QStyle* style = fusion(); style != nullptr && !explorersOwn) {
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
    if (!explorersOwn) {
        dialog.setSidebarUrls(places_here::everyPlace());
    }

    // Qt's own dialog stands over the window it was opened from, which waits
    // for it, and is put back as it was left. Explorer's own gets the active
    // window as its owner from Qt, and Explorer remembers its own size.
    if (!explorersOwn) {
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
    }

    // Import's export dialog in the --debug log (debug version 0.5.78.1).
    const bool logged = memoryName == QLatin1String("ImportWindow.exportDialog");
    if (logged) {
        logWindows(QStringLiteral("file dialog opening in folder \"%1\", owner %2")
                       .arg(QDir::toNativeSeparators(dialog.directory().absolutePath()), windowWords(windowOf(window))),
                   nullptr);
        QTimer::singleShot(1000, &dialog, []() { logEveryWindow(); });
    }
    openDialog() = &dialog;
    const bool chosen = dialog.exec() == QDialog::Accepted;
    openDialog() = nullptr;
    if (logged) {
        logWindows(chosen ? QStringLiteral("file dialog returned a file") : QStringLiteral("file dialog cancelled"), nullptr);
    }
    if (!explorersOwn) {
        SettingsFile().setValue(memoryKey(memoryName), dialog.saveGeometry());
    }
    if (!chosen || dialog.selectedUrls().isEmpty()) {
        return {};
    }
    return dialog.selectedUrls().constFirst();
}

void FilePickers::logImportStep(const QString& step, QObject* window)
{
    logWindows(step, windowOf(window));
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

QUrl FilePickers::vaultsFolder() const
{
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString folder = documents + QStringLiteral("/chrono-vault");
    if (documents.isEmpty() || !QDir().mkpath(folder)) {
        return homeFolder();
    }
    return QUrl::fromLocalFile(folder);
}

} // namespace chrono_vault
