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

#pragma once

#include "item_list_model.h"
#include "vault_contents.h"
#include "vault_file.h"

#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// --- Version ------------------------------------------------------------
// vault_controller.h 0.0.4

// --- Version history ----------------------------------------------------
// v0.0.4: The program's version, for the window titles; the General settings; item icons
// v0.0.3: Initial release

namespace chrono_vault {

// What the windows call. It holds the unlocked vault, does every slow step
// (deriving the key, reading and writing the file) off the window's thread
// (OR-5), and saves after every change, safely (OR-15).
class VaultController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString vaultPath READ vaultPath NOTIFY vaultPathChanged)
    Q_PROPERTY(QString vaultName READ vaultName NOTIFY vaultPathChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageIsTrouble READ messageIsTrouble NOTIFY messageChanged)
    Q_PROPERTY(bool unsavedChanges READ unsavedChanges NOTIFY unsavedChangesChanged)
    Q_PROPERTY(bool saveFailed READ saveFailed NOTIFY saveFailedChanged)
    Q_PROPERTY(bool hasKeptChanges READ hasKeptChanges NOTIFY keptChangesChanged)
    Q_PROPERTY(ItemListModel* items READ items CONSTANT)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(int autoLockMinutes READ autoLockMinutes WRITE setAutoLockMinutes NOTIFY autoLockMinutesChanged)
    Q_PROPERTY(bool keepInTray READ keepInTray WRITE setKeepInTray NOTIFY keepInTrayChanged)
    Q_PROPERTY(bool startAtStartup READ startAtStartup WRITE setStartAtStartup NOTIFY startAtStartupChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY startMinimizedChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY trayAvailableChanged)
    Q_PROPERTY(int clipboardClearSeconds READ clipboardClearSeconds WRITE setClipboardClearSeconds NOTIFY
                   clipboardClearSecondsChanged)

public:
    enum class State { NoVault, Locked, Busy, Unlocked };
    Q_ENUM(State)

    explicit VaultController(QObject* parent = nullptr);
    ~VaultController() override;

    [[nodiscard]] State state() const { return currentState; }
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString vaultPath() const { return path; }
    [[nodiscard]] QString vaultName() const;
    [[nodiscard]] QString message() const { return currentMessage; }
    [[nodiscard]] bool messageIsTrouble() const { return trouble; }
    // A change is in memory that the vault on disk does not have yet,
    // because a save failed or is still running.
    [[nodiscard]] bool unsavedChanges() const { return dirty; }
    // The last save failed, and its change is not on disk yet.
    [[nodiscard]] bool saveFailed() const { return failed; }
    // The locked vault holds changes, kept through the lock, that are saved
    // at the next unlock.
    [[nodiscard]] bool hasKeptChanges() const { return !unsavedSealed.isEmpty() && unsavedPath == path; }
    [[nodiscard]] ItemListModel* items() { return &listModel; }
    [[nodiscard]] QString searchText() const { return search; }
    void setSearchText(const QString& text);
    [[nodiscard]] int autoLockMinutes() const { return lockMinutes; }
    void setAutoLockMinutes(int minutes);
    // The General settings (D-019).
    [[nodiscard]] bool keepInTray() const { return keepRunning; }
    void setKeepInTray(bool keep);
    [[nodiscard]] bool startAtStartup() const;
    void setStartAtStartup(bool start);
    [[nodiscard]] bool startMinimized() const { return minimizedAtStart; }
    void setStartMinimized(bool minimized);
    [[nodiscard]] bool trayAvailable() const { return trayExists; }
    void setTrayAvailable(bool available);
    [[nodiscard]] int clipboardClearSeconds() const { return clearSeconds; }
    void setClipboardClearSeconds(int seconds);

    // Creates a new vault at the chosen place and opens it.
    Q_INVOKABLE void createVault(const QUrl& file, const QString& password, const QString& repeated);
    // Chooses an existing vault file; it is then unlocked with unlock().
    Q_INVOKABLE void chooseVault(const QUrl& file);
    // False, and says why, when a new vault cannot be made at this place:
    // asked right after the place is chosen, before any password is typed.
    Q_INVOKABLE bool canCreateAt(const QUrl& file);
    Q_INVOKABLE void unlock(const QString& password);
    Q_INVOKABLE void lock();
    // Back to the start, to create or choose another vault.
    Q_INVOKABLE void closeVault();

    // An item as the editor uses it: {id, isNote, title, fields: [{role,
    // label, name, kind, value}]}. An unknown id gives an empty map.
    [[nodiscard]] Q_INVOKABLE QVariantMap item(const QString& id) const;
    // A new item that is not in the vault until it is saved.
    [[nodiscard]] Q_INVOKABLE QVariantMap newLogin() const;
    [[nodiscard]] Q_INVOKABLE QVariantMap newSecureNote() const;
    // Stores the item (new or changed) and saves the vault.
    Q_INVOKABLE bool saveItem(const QVariantMap& item);
    Q_INVOKABLE void deleteItem(const QString& id);
    // Reads a picked picture as an item icon (D-018): the PNG in base64, or
    // empty when the file is not used, with a message saying why.
    [[nodiscard]] Q_INVOKABLE QString readItemIcon(const QUrl& file);
    // Saves again after a save failed.
    Q_INVOKABLE void retrySave();

    // Copies text, marked as secret for clipboard history tools, and clears
    // it again after the chosen number of seconds (D-007).
    Q_INVOKABLE void copyToClipboard(const QString& text);
    [[nodiscard]] Q_INVOKABLE QString generatePassword(int length, bool uppercase, bool lowercase, bool digits,
                                                       bool symbols) const;
    // The current one-time code for a secret, or empty when it is not one.
    [[nodiscard]] Q_INVOKABLE QString oneTimeCode(const QString& secret) const;
    [[nodiscard]] Q_INVOKABLE int oneTimeCodeSecondsLeft(const QString& secret) const;
    [[nodiscard]] Q_INVOKABLE QString fieldName(const QString& role) const;
    // The person is logging out or shutting down: the window then ends the
    // program instead of hiding to the tray, so the logout is never held up.
    [[nodiscard]] Q_INVOKABLE bool isSessionEnding() const;

    // Tests only: a cheap key derivation instead of the standard one.
    void setKdfParameters(const crypto::KdfParameters& parameters) { kdfParameters = parameters; }
    // Tests only: true once every save that was started has finished.
    [[nodiscard]] bool isIdle() const;

signals:
    void stateChanged();
    void vaultPathChanged();
    void messageChanged();
    void searchTextChanged();
    void autoLockMinutesChanged();
    void clipboardClearSecondsChanged();
    void keepInTrayChanged();
    void startAtStartupChanged();
    void startMinimizedChanged();
    void trayAvailableChanged();
    void unsavedChangesChanged();
    void saveFailedChanged();
    void keptChangesChanged();
    // The vault is about to lock: the windows store what is being typed.
    void locking();
    // The vault was saved after a change; the item with this id, if any, is
    // the one to show.
    void saved(const QString& itemId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setState(State state);
    void say(const QString& text, bool isTrouble);
    void setPath(const QString& newPath);
    void saveInBackground(const QString& itemId);
    void restartAutoLock();
    void forgetUnlockedVault();
    void setDirty(bool isDirty);
    void setFailed(bool hasFailed);
    void keepUnsavedWhileLocked();
    void takeBackUnsaved();

    State currentState = State::NoVault;
    QString path;
    QString currentMessage;
    bool trouble = false;
    QString search;
    int lockMinutes = 5;
    int clearSeconds = 30;
    bool keepRunning = true;
    bool minimizedAtStart = false;
    bool trayExists = false;

    VaultContents contents;
    VaultKey key;
    crypto::KdfParameters kdfParameters;
    ItemListModel listModel;

    QThreadPool savePool;    // one thread, so saves happen in order
    QThreadPool unlockPool;  // deriving keys
    int savesRunning = 0;
    bool dirty = false;
    bool failed = false;
    QTimer messageTimer; // plain information disappears after a while
    // Unsaved changes kept through a lock, sealed with the vault's key so
    // nothing readable stays in memory (OR-15).
    QByteArray unsavedSealed;
    QString unsavedPath;
    quint64 unlockAttempt = 0;

    QTimer autoLockTimer;
    QTimer clipboardTimer;
    QString copiedText;
};

} // namespace chrono_vault
