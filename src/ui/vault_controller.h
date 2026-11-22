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

#include <QHash>
#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// --- Version ------------------------------------------------------------
// vault_controller.h 0.0.14

// --- Version history ----------------------------------------------------
// v0.0.14: Categories and templates, the person's own too; tags; favourites; attachments; what the list is
//          narrowed to (D-027, D-028, D-029)
// v0.0.6: The settings' defaults, for Restore defaults (D-022)
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
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
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
    // What the list is narrowed to: "" (Home), "favourites", "category:<id>"
    // or "tag:<tag>" (D-027, D-028).
    [[nodiscard]] QString filter() const { return chosenFilter; }
    void setFilter(const QString& filter);
    // Goes up whenever the unlocked vault's contents change, so the windows
    // read their lists again.
    [[nodiscard]] int revision() const { return contentsRevision; }
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

    // An item as the editor uses it: {id, templateId, templateName,
    // categoryId, isNote, title, fields: [{role, label, name, kind, value}],
    // icon, defaultIcon, tags, favourite, attachments: [{id, name, size}]}.
    // An unknown id gives an empty map.
    [[nodiscard]] Q_INVOKABLE QVariantMap item(const QString& id) const;
    // A new item that is not in the vault until it is saved.
    [[nodiscard]] Q_INVOKABLE QVariantMap newItemFrom(const QString& templateId) const;
    [[nodiscard]] Q_INVOKABLE QVariantMap newLogin() const;
    [[nodiscard]] Q_INVOKABLE QVariantMap newSecureNote() const;

    // Categories and templates (D-027). Every category: {id, name, icon,
    // builtIn, hidden, count}; every template: {id, name, categoryId,
    // categoryName, builtIn, fields: [{role, label, name, kind}]}.
    [[nodiscard]] Q_INVOKABLE QVariantList categories() const;
    [[nodiscard]] Q_INVOKABLE QVariantList templates() const;
    // A category of the person's own, new (empty id) or renamed: its id, or
    // empty when it is not kept, with a message saying why.
    Q_INVOKABLE QString saveCategory(const QString& id, const QString& name);
    // Only a category of the person's own that no item or template uses.
    Q_INVOKABLE bool deleteCategory(const QString& id);
    Q_INVOKABLE void setCategoryHidden(const QString& id, bool hidden);
    // A template of the person's own, new (empty id) or changed: {id, name,
    // categoryId, fields: [{role, label, kind}]}. Its id, or empty.
    Q_INVOKABLE QString saveTemplate(const QVariantMap& itemTemplate);
    Q_INVOKABLE bool deleteTemplate(const QString& id);
    // A copy of any template, to be changed and saved as the person's own.
    [[nodiscard]] Q_INVOKABLE QVariantMap copyOfTemplate(const QString& id) const;

    // Tags (D-028): every tag in use and every tag above one, in the order
    // of the tree: {tag, name, depth, count}.
    [[nodiscard]] Q_INVOKABLE QVariantList tags() const;
    // Renames a tag, and the tags nested under it, in every item.
    Q_INVOKABLE bool renameTag(const QString& tag, const QString& newName);
    // Takes a tag, and the tags nested under it, off every item.
    Q_INVOKABLE void removeTag(const QString& tag);
    Q_INVOKABLE void setFavourite(const QString& id, bool favourite);

    // Attachments (D-029). Reads a file off the window's thread; the answer
    // comes as attachmentRead with the id returned here.
    Q_INVOKABLE QString readAttachment(const QUrl& file);
    // Writes an attachment of a saved item where the person chose.
    Q_INVOKABLE void saveAttachment(const QString& itemId, const QString& attachmentId, const QUrl& file);
    [[nodiscard]] Q_INVOKABLE QString sizeText(qint64 bytes) const;
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
    // The settings before the person changes them: what Restore defaults in
    // the settings window puts back (D-022).
    [[nodiscard]] Q_INVOKABLE QVariantMap settingsDefaults() const;

    // Tests only: a cheap key derivation instead of the standard one.
    void setKdfParameters(const crypto::KdfParameters& parameters) { kdfParameters = parameters; }
    // Tests only: true once every save that was started has finished.
    [[nodiscard]] bool isIdle() const;

signals:
    void stateChanged();
    void vaultPathChanged();
    void messageChanged();
    void searchTextChanged();
    void filterChanged();
    void revisionChanged();
    // A file the person added is read (ok) or not; the id is readAttachment's.
    void attachmentRead(const QString& requestId, const QString& name, qint64 size, bool ok);
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
    // The list and every window that shows the contents are told of a change.
    void contentsChanged();
    [[nodiscard]] qsizetype attachmentBytes() const;

    State currentState = State::NoVault;
    QString path;
    QString currentMessage;
    bool trouble = false;
    QString search;
    QString chosenFilter;
    int contentsRevision = 0;
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
    QThreadPool filePool;    // reading and writing attachments
    // Files added in the editor, kept until the item is saved; dropped with
    // the unlocked vault.
    QHash<QString, Attachment> pendingAttachments;
    quint64 vaultSession = 0; // goes up each time the unlocked vault is put away
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
