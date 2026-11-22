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
#include "json_import.h"
#include "quick_unlock.h"
#include "system_activity.h"
#include "vault_contents.h"
#include "vault_file.h"

#include <QFileSystemWatcher>
#include <QNetworkAccessManager>
#include <QSet>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <memory>

// --- Version ------------------------------------------------------------
// vault_controller.h 0.0.23

// --- Version history ----------------------------------------------------
// v0.0.23: Unlocking with a fingerprint, and the yearly master password reminder (D-038)
// v0.0.22: Export in the reference program's format, after the master password (D-037)
// v0.0.20: Website icons, asked of each website itself, kept in memory only (D-036)
// v0.0.19: Importing the reference program's export (D-035)
// v0.0.18: Whether a touch screen is present (D-034)
// v0.0.17: Trash and Archive; Hide sensitive fields and Show subtitles (D-033)
// v0.0.16: A change made elsewhere to the vault file noticed, read again, or kept beside it as a conflict copy (D-032)
// v0.0.15: Several vaults, the first the primary, whose master password unlocks the others; a keyfile; changing the
//          master password or keyfile (D-030, D-031)
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
    Q_PROPERTY(QStringList vaults READ vaults NOTIFY vaultsChanged)
    Q_PROPERTY(QString primaryVault READ primaryVault NOTIFY vaultsChanged)
    Q_PROPERTY(QString keyfilePath READ keyfilePath WRITE setKeyfilePath NOTIFY keyfileChanged)
    Q_PROPERTY(bool keyfileNeeded READ keyfileNeeded NOTIFY keyfileChanged)
    Q_PROPERTY(QUrl keyfileUrl READ keyfileUrl NOTIFY keyfileChanged)
    Q_PROPERTY(bool usesKeyfile READ usesKeyfile NOTIFY stateChanged)
    Q_PROPERTY(ItemListModel* items READ items CONSTANT)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(int autoLockMinutes READ autoLockMinutes WRITE setAutoLockMinutes NOTIFY autoLockMinutesChanged)
    // Autolock when (D-040): Chrono Vault idle for autoLockMinutes (when
    // idleLockOn), the main window closed, the system sleeping, the system
    // inactive for systemIdleMinutes.
    Q_PROPERTY(bool idleLockOn READ idleLockOn WRITE setIdleLockOn NOTIFY autoLockMinutesChanged)
    Q_PROPERTY(bool lockWhenClosed READ lockWhenClosed WRITE setLockWhenClosed NOTIFY autoLockMinutesChanged)
    Q_PROPERTY(bool lockOnSleep READ lockOnSleep WRITE setLockOnSleep NOTIFY autoLockMinutesChanged)
    Q_PROPERTY(bool systemIdleLockOn READ systemIdleLockOn WRITE setSystemIdleLockOn NOTIFY autoLockMinutesChanged)
    Q_PROPERTY(int systemIdleMinutes READ systemIdleMinutes WRITE setSystemIdleMinutes NOTIFY autoLockMinutesChanged)
    Q_PROPERTY(bool systemIdleAvailable READ systemIdleAvailable NOTIFY systemIdleAvailableChanged)
    // Seconds before the master password may be tried again for this vault,
    // after wrong ones (D-041); 0 when it may be tried now.
    Q_PROPERTY(int unlockWaitSeconds READ unlockWaitSeconds NOTIFY unlockWaitChanged)
    Q_PROPERTY(bool keepInTray READ keepInTray WRITE setKeepInTray NOTIFY keepInTrayChanged)
    Q_PROPERTY(bool startAtStartup READ startAtStartup WRITE setStartAtStartup NOTIFY startAtStartupChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY startMinimizedChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY trayAvailableChanged)
    Q_PROPERTY(bool hideSensitive READ hideSensitive WRITE setHideSensitive NOTIFY hideSensitiveChanged)
    Q_PROPERTY(bool showSubtitles READ showSubtitles WRITE setShowSubtitles NOTIFY showSubtitlesChanged)
    // The View menu's choices, kept in the settings (D-039).
    Q_PROPERTY(bool showItemCounts READ showItemCounts WRITE setShowItemCounts NOTIFY viewChanged)
    Q_PROPERTY(bool sidebarHidden READ sidebarHidden WRITE setSidebarHidden NOTIFY viewChanged)
    Q_PROPERTY(bool showCategories READ showCategories WRITE setShowCategories NOTIFY viewChanged)
    Q_PROPERTY(bool showTags READ showTags WRITE setShowTags NOTIFY viewChanged)
    Q_PROPERTY(bool showOthers READ showOthers WRITE setShowOthers NOTIFY viewChanged)
    // The sidebar collapsed to its icons, and its groups folded (D-042).
    Q_PROPERTY(bool sidebarCollapsed READ sidebarCollapsed WRITE setSidebarCollapsed NOTIFY viewChanged)
    Q_PROPERTY(bool categoriesFolded READ categoriesFolded WRITE setCategoriesFolded NOTIFY viewChanged)
    Q_PROPERTY(bool tagsFolded READ tagsFolded WRITE setTagsFolded NOTIFY viewChanged)
    Q_PROPERTY(bool othersFolded READ othersFolded WRITE setOthersFolded NOTIFY viewChanged)
    Q_PROPERTY(bool touchScreen READ touchScreen CONSTANT)
    Q_PROPERTY(bool useWebsiteIcons READ useWebsiteIcons WRITE setUseWebsiteIcons NOTIFY useWebsiteIconsChanged)
    Q_PROPERTY(bool quickUnlockAvailable READ quickUnlockAvailable NOTIFY quickUnlockChanged)
    Q_PROPERTY(bool quickUnlockOn READ quickUnlockOn NOTIFY quickUnlockChanged)
    // Turning it on: a finger asked once, and what came of it, for the
    // settings window (D-042).
    Q_PROPERTY(bool quickUnlockBusy READ quickUnlockBusy NOTIFY quickUnlockChanged)
    Q_PROPERTY(QString quickUnlockMessage READ quickUnlockMessage NOTIFY quickUnlockChanged)
    Q_PROPERTY(bool quickUnlockTrouble READ quickUnlockTrouble NOTIFY quickUnlockChanged)
    Q_PROPERTY(QString quickUnlockName READ quickUnlockName CONSTANT)
    Q_PROPERTY(bool reminderDue READ reminderDue NOTIFY quickUnlockChanged)
    Q_PROPERTY(bool masterPasswordReminder READ masterPasswordReminder WRITE setMasterPasswordReminder NOTIFY
                   quickUnlockChanged)
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
    // Changes kept through a lock, saved when their vault is unlocked again;
    // that vault may be another than the one shown (D-030).
    [[nodiscard]] bool hasKeptChanges() const { return !unsavedSealed.isEmpty(); }
    // Every vault the person keeps; the first is the primary, whose master
    // password unlocks the others (D-030).
    [[nodiscard]] QStringList vaults() const { return vaultList; }
    [[nodiscard]] QString primaryVault() const;
    // The keyfile used for the vault shown, remembered per vault (D-031).
    [[nodiscard]] QString keyfilePath() const;
    void setKeyfilePath(const QString& keyfile);
    // The same keyfile as an address the file windows take; empty for none.
    [[nodiscard]] QUrl keyfileUrl() const
    {
        return keyfilePath().isEmpty() ? QUrl() : QUrl::fromLocalFile(keyfilePath());
    }
    [[nodiscard]] bool keyfileNeeded() const { return needKeyfile || !keyfilePath().isEmpty(); }
    [[nodiscard]] bool usesKeyfile() const { return currentState == State::Unlocked && key.usesKeyfile; }
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
    [[nodiscard]] bool idleLockOn() const { return idleLock; }
    void setIdleLockOn(bool on);
    [[nodiscard]] bool lockWhenClosed() const { return lockClosed; }
    void setLockWhenClosed(bool on);
    [[nodiscard]] bool lockOnSleep() const { return lockSleep; }
    void setLockOnSleep(bool on);
    [[nodiscard]] bool systemIdleLockOn() const { return systemIdleLock; }
    void setSystemIdleLockOn(bool on);
    [[nodiscard]] int systemIdleMinutes() const { return systemMinutes; }
    void setSystemIdleMinutes(int minutes);
    [[nodiscard]] bool systemIdleAvailable() const { return systemIdleKnown; }
    [[nodiscard]] int unlockWaitSeconds() const { return waitSeconds; }
    // The wait before an attempt, after this many wrong ones (D-041).
    [[nodiscard]] static int waitBeforeAttempt(int attempt);
    // The main window was closed into the tray: locked when so chosen.
    Q_INVOKABLE void mainWindowClosed();
    // Asks the system how long it has been inactive (every 30 seconds while
    // a vault is unlocked and that choice is on); public for the tests.
    Q_INVOKABLE void checkSystemIdle();
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
    // Settings > Security and Customize (D-033).
    [[nodiscard]] bool hideSensitive() const { return hideSecrets; }
    void setHideSensitive(bool hide);
    [[nodiscard]] bool showSubtitles() const { return subtitlesShown; }
    // A touch screen is present: what shows under the mouse stays shown (D-034).
    [[nodiscard]] bool touchScreen() const;
    // Settings > Customize > Use website icons, off by default (D-036).
    [[nodiscard]] bool useWebsiteIcons() const { return websiteIconsOn; }
    void setUseWebsiteIcons(bool use);
    // Unlocking with a fingerprint (D-038).
    [[nodiscard]] bool quickUnlockAvailable() const { return quickAvailable; }
    [[nodiscard]] bool quickUnlockOn() const { return quick && !path.isEmpty() && quick->has(path); }
    [[nodiscard]] QString quickUnlockName() const { return quick ? quick->name() : QString(); }
    // Once a year the master password is asked though a finger would do (the
    // owner, Q44), unless the reminder is off.
    [[nodiscard]] bool reminderDue() const;
    [[nodiscard]] bool masterPasswordReminder() const { return reminderOn; }
    void setMasterPasswordReminder(bool on);
    Q_INVOKABLE void setQuickUnlock(bool on);
    Q_INVOKABLE void unlockWithQuickUnlock();
    [[nodiscard]] bool quickUnlockBusy() const { return settingUp; }
    [[nodiscard]] QString quickUnlockMessage() const { return setupMessage; }
    [[nodiscard]] bool quickUnlockTrouble() const { return setupTrouble; }
    // The Cancel beside "Touch the fingerprint reader…" (D-042).
    Q_INVOKABLE void cancelQuickUnlockSetup();
    // The unlock page is no longer on screen: the reader is given back.
    Q_INVOKABLE void stopQuickUnlock();
    // "Not now": the reminder waits another year.
    Q_INVOKABLE void dismissReminder();
    // Tests only: a stand-in for the fingerprint reader.
    void setQuickUnlockForTests(std::unique_ptr<QuickUnlock> standIn);
    void setSystemActivityForTests(std::unique_ptr<SystemActivity> standIn);
    // The tests' clock: the vault counts as unlocked this many seconds ago.
    void setUnlockedSecondsAgoForTests(qint64 seconds);
    // The tests' clock for the waits: moved forward this many seconds.
    void moveClockForTests(qint64 seconds);
    // Tests only: where a website's icon is asked for.
    void setIconAddressForTests(std::function<QUrl(const QString&)> address) { iconAddress = std::move(address); }
    void setShowSubtitles(bool show);
    [[nodiscard]] bool showItemCounts() const { return viewOption(QStringLiteral("showItemCounts"), false); }
    void setShowItemCounts(bool on) { setViewOption(QStringLiteral("showItemCounts"), on); }
    [[nodiscard]] bool sidebarHidden() const { return viewOption(QStringLiteral("sidebarHidden"), false); }
    void setSidebarHidden(bool on) { setViewOption(QStringLiteral("sidebarHidden"), on); }
    [[nodiscard]] bool showCategories() const { return viewOption(QStringLiteral("showCategories"), true); }
    void setShowCategories(bool on) { setViewOption(QStringLiteral("showCategories"), on); }
    [[nodiscard]] bool showTags() const { return viewOption(QStringLiteral("showTags"), true); }
    void setShowTags(bool on) { setViewOption(QStringLiteral("showTags"), on); }
    [[nodiscard]] bool showOthers() const { return viewOption(QStringLiteral("showOthers"), true); }
    void setShowOthers(bool on) { setViewOption(QStringLiteral("showOthers"), on); }
    [[nodiscard]] bool sidebarCollapsed() const { return viewOption(QStringLiteral("sidebarCollapsed"), false); }
    void setSidebarCollapsed(bool on) { setViewOption(QStringLiteral("sidebarCollapsed"), on); }
    [[nodiscard]] bool categoriesFolded() const { return viewOption(QStringLiteral("categoriesFolded"), false); }
    void setCategoriesFolded(bool on) { setViewOption(QStringLiteral("categoriesFolded"), on); }
    [[nodiscard]] bool tagsFolded() const { return viewOption(QStringLiteral("tagsFolded"), false); }
    void setTagsFolded(bool on) { setViewOption(QStringLiteral("tagsFolded"), on); }
    [[nodiscard]] bool othersFolded() const { return viewOption(QStringLiteral("othersFolded"), false); }
    void setOthersFolded(bool on) { setViewOption(QStringLiteral("othersFolded"), on); }
    [[nodiscard]] int clipboardClearSeconds() const { return clearSeconds; }
    void setClipboardClearSeconds(int seconds);

    // Creates a new vault at the chosen place and opens it; with a keyfile
    // when one is given (D-031). With savePassword, and the primary vault
    // unlocked, its password is saved in the primary first (D-030).
    Q_INVOKABLE void createVault(const QUrl& file, const QString& password, const QString& repeated,
                                 const QUrl& keyfile = QUrl(), bool savePassword = false);
    // Whether a new vault's password can be saved in the primary vault now.
    [[nodiscard]] Q_INVOKABLE bool canSaveInPrimary() const;
    [[nodiscard]] Q_INVOKABLE QString vaultNameOf(const QString& vaultFile) const;
    // Shows another of the person's vaults: the primary opened with the key
    // kept from this session, one whose password is saved in the primary
    // opened with it, any other locked (D-030).
    Q_INVOKABLE void switchToVault(const QString& vaultFile);
    // Writes a new keyfile where the person chose; answered by keyfileMade.
    Q_INVOKABLE void makeKeyfile(const QUrl& file);
    // Writes the open vault, unencrypted, in the reference program's format
    // where the person chose, once the master password is right (D-037);
    // answered by exportDone. The file is readable by the person alone.
    Q_INVOKABLE void exportVault(const QUrl& file, const QString& password);
    // A new master password, and a keyfile or none, for the unlocked vault;
    // answered by masterPasswordChanged (D-031).
    Q_INVOKABLE void changeMasterPassword(const QString& current, const QString& password, const QString& repeated,
                                          const QUrl& keyfile);
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

    // Importing the reference program's export (D-035): the file is read off
    // the window's thread and answered by importRead with a summary; nothing
    // joins the vault until confirmImport.
    Q_INVOKABLE void readImport(const QUrl& file);
    Q_INVOKABLE bool confirmImport();
    Q_INVOKABLE void cancelImport();
    [[nodiscard]] Q_INVOKABLE QString referenceName() const;
    // Stores the item (new or changed) and saves the vault.
    Q_INVOKABLE bool saveItem(const QVariantMap& item);
    // Deletes an item for good (from the Trash, after asking).
    Q_INVOKABLE void deleteItem(const QString& id);
    // Moves an item into the Trash or out of it, into the Archive or out of
    // it (D-033).
    Q_INVOKABLE void moveToTrash(const QString& id, bool trashed);
    Q_INVOKABLE void archiveItem(const QString& id, bool archived);
    // Reads a picked picture as an item icon (D-018): the PNG in base64, or
    // empty when the file is not used, with a message saying why.
    [[nodiscard]] Q_INVOKABLE QString readItemIcon(const QUrl& file);
    // Saves again after a save failed.
    Q_INVOKABLE void retrySave();

    // Copies text, marked as secret for clipboard history tools, and clears
    // it again after the chosen number of seconds (D-007).
    Q_INVOKABLE void copyToClipboard(const QString& text);
    // Tools > Clear Clipboard: empties it at once, whatever is on it (D-039).
    Q_INVOKABLE void clearClipboard();
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
    void vaultsChanged();
    void keyfileChanged();
    void keyfileMade(const QString& keyfile, bool ok);
    void masterPasswordChanged(bool ok);
    // {ok, items, categories, templates, attachments, message}
    void importRead(const QVariantMap& summary);
    void exportDone(bool ok);
    void autoLockMinutesChanged();
    void systemIdleAvailableChanged();
    void unlockWaitChanged();
    void clipboardClearSecondsChanged();
    void hideSensitiveChanged();
    void showSubtitlesChanged();
    void viewChanged();
    void useWebsiteIconsChanged();
    void quickUnlockChanged();
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
    // A vault just created whose password is being saved in the primary: it
    // is shown once that save is on disk (D-030).
    struct CreatedVault
    {
        QString path;
        VaultKey key;
        QByteArray keyfile;
    };
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
    void setItemPlace(const QString& id, bool Item::*place, bool on);
    [[nodiscard]] qsizetype attachmentBytes() const;
    void addVault(const QString& vaultFile);
    void rememberKeyfile(const QString& vaultFile, const QString& keyfile);
    [[nodiscard]] QString keyfileOf(const QString& vaultFile) const;
    // Unlocks the vault shown with a password, or with a key kept from
    // before; both off the window's thread (OR-5).
    void startUnlock(const QByteArray& password, bool savedPassword = false);
    void startOpenWithKey(const VaultKey& kept);
    void unlocked(const VaultKey& newKey, const VaultContents& read, const QByteArray& keyfile);
    // The primary's key and the passwords saved in it, kept while unlocked.
    void rememberPrimary();
    void forgetPrimary();
    void showCreated(CreatedVault created);
    void connectQuickUnlock();
    void recordMasterPassword();
    void recordMasterPasswordIfNone();
    // Asks each website not asked yet for its icon (D-036).
    void fetchWebsiteIcons();
    // A change made elsewhere (D-032): the stamp of each vault file as this
    // Chrono Vault last read or wrote it, shared with the save thread.
    struct Stamps
    {
        QMutex lock;
        QHash<QString, QByteArray> known;
        void set(const QString& vaultFile, const QByteArray& stamp)
        {
            const QMutexLocker locker(&lock);
            known.insert(vaultFile, stamp);
        }
        [[nodiscard]] QByteArray get(const QString& vaultFile)
        {
            const QMutexLocker locker(&lock);
            return known.value(vaultFile);
        }
    };
    void watch();
    void checkForChangeElsewhere();
    // Reads the vault shown again with its key; a vault that its key no
    // longer opens is locked.
    void readAgain(const QString& said, bool important);
    // Where a version of the vault that is not kept in it is written.
    [[nodiscard]] static QString conflictPathFor(const QString& vaultFile);
    // The password saved in the primary for this vault, changed with it.
    void updateSavedPassword(const QString& vaultFile, const QString& password);

    State currentState = State::NoVault;
    QString path;
    QString currentMessage;
    bool trouble = false;
    QString search;
    QString chosenFilter;
    int contentsRevision = 0;
    int lockMinutes = 5;
    int clearSeconds = 30;
    bool hideSecrets = true;
    bool subtitlesShown = true;
    [[nodiscard]] bool viewOption(const QString& name, bool fallback) const;
    void setViewOption(const QString& name, bool on);
    bool websiteIconsOn = false;
    std::unique_ptr<QuickUnlock> quick;
    std::unique_ptr<SystemActivity> activity;
    void connectSystemActivity();
    bool idleLock = true;
    bool lockClosed = false;
    bool lockSleep = false;
    bool systemIdleLock = false;
    int systemMinutes = 10;
    bool systemIdleKnown = false;
    QTimer systemIdleTimer;
    // When the vault was last unlocked: the system's inactive time counts
    // only from then on (an unlock by finger is no input the system sees).
    qint64 unlockedAtMs = 0;
    // The waits after wrong master passwords (D-041), kept per vault in the
    // settings so a restart does not end them.
    int waitSeconds = 0;
    qint64 clockShiftMs = 0;
    QTimer waitTimer;
    [[nodiscard]] qint64 nowMs() const;
    [[nodiscard]] QString waitKey() const;
    void wrongPasswordTyped();
    void rightPasswordGiven();
    void updateWait();
    void setBool(bool& value, bool on, const QString& settingName);
    bool quickAvailable = false;
    bool reminderOn = true;
    bool replacingQuickKey = false; // storing the new key after a master password change
    bool settingUp = false;   // a finger asked to turn it on
    bool retrieving = false;  // a finger asked to unlock
    QString setupMessage;
    bool setupTrouble = false;
    void tellSetup(const QString& text, bool isTrouble);
    bool openingByFinger = false;   // the vault is being opened with the key kept for the finger
    QNetworkAccessManager iconNetwork;
    QHash<QString, QString> websiteIcons; // host: "data:" address of its PNG, in memory only
    QSet<QString> iconsAsked;
    std::function<QUrl(const QString&)> iconAddress;
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
    QStringList vaultList;
    bool needKeyfile = false;
    QByteArray keyfileSecret;              // the unlocked vault's keyfile fingerprint
    VaultKey primaryKey;                   // kept while unlocked (D-030)
    QByteArray primaryKeyfile;             // the primary's keyfile fingerprint, with its key
    std::optional<CreatedVault> createdVault; // see CreatedVault
    QHash<QString, QString> savedPasswords; // vault file to its password, from the primary
    QString returnTo;                      // the vault shown before the lock
    std::optional<ImportResult> pendingImport; // read, waiting for the person's OK
    std::shared_ptr<Stamps> stamps = std::make_shared<Stamps>();
    QFileSystemWatcher watcher;
    bool checkPending = false;
    bool readingAgain = false; // the vault shown is being read again (D-032)
    QByteArray unsavedStamp;   // the stamp a change kept through a lock was made against
    QByteArray unlockedStamp;  // the stamp of the file as the last unlock read it
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
