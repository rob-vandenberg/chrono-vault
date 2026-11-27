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

#include <atomic>

#include "audit.h"
#include "item_list_model.h"
#include "json_import.h"
#include "quick_unlock.h"
#include "system_activity.h"
#include "vault_contents.h"
#include "vault_merge.h"
#include "vault_file.h"
#include "secret_store.h"
#include "cloud_storage.h"
#include "cloud_sync.h"
#include "google_drive_storage.h"
#include "webdav_client.h"
#include "webdav_sync.h"

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
#include <optional>
#include <utility>

// --- Version ------------------------------------------------------------
// vault_controller.h 0.8.119

// --- Version history ----------------------------------------------------
// v0.8.119: Disconnect deletes nothing on any storage (D-060); Sort By per vault and each item's use (D-081)
// v0.8.117: googleAccount: the Google Drive window's account, kept with new places (D-080 point 20)
// v0.8.116: googleToken holds only the Google Drive window's sign-in (D-080 point 13)
// v0.8.115: Google Drive: sign in, its folders, create, open, move, disconnect; one create, open and move for all (D-080)
// v0.8.114: Any cloud storage: inCloud, storageName, storages; the sync is CloudSync (D-080)
// v0.7.110: A vault that opens without typing is opened before the one shown closes (D-078)
// v0.7.107: A PIN per vault; the lock as a screen lock (D-076)
// v0.6.106: Close and Lock per vault (D-075)
// v0.6.105: Another primary vault; Remember my credentials; Open, Closed, Locked (D-074)
// v0.6.103: One standard dialog for a master password, a PIN or a finger (D-073)
// v0.6.100: Padlocks and Open in Settings > Vaults; round picture buttons (D-071)
// v0.6.86: The audit (D-065)
// v0.5.83: setCategoryOf (D-063)
// v0.5.82: Several items at once (D-062)
// v0.5.81: Retry sync; every field of a conflict's item (D-061)
// v0.5.78: Every difference a conflict; the sides' names; the person's own categories and templates (D-061)
// v0.5.77: Import from another vault; an import's duplicates skipped or made conflicts (D-060)
// v0.5.76: The Sync conflicts dialog: conflicts, conflictOf, settleConflicts, settleConflict (D-060)
// v0.5.75: The vault menu: lockVault, moveVaultTo, moveVaultToWebDav, forgetVault; Disconnect deletes nothing (D-060)
// v0.4.70: PIN unlock (D-059)
// v0.3.67: giveUpKeptChangesOnLeaving, leavingFor
// v0.3.62: The WebDAV folder browser; a new WebDAV vault kept only after its first upload (D-057)
// v0.3.60: A vault kept on a WebDAV server: its working copy synced, connecting, creating, opening, moving and
//          disconnecting (D-057)
// v0.2.56: Editing and resetting every category; the icons' names translated (D-056)
// v0.2.45: A change here and one elsewhere merged against the base; the notes a merge leaves (D-054)
// v0.0.40: The vault's own icons and the icon sections (D-050); keyName's parameter renamed
// v0.0.39: The reminder every Day, Month, Quarter, Year or Never (D-049)
// v0.0.38: The Esc setting (D-048)
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

// The steps of a move, for tests that stop it at each to prove the original
// is never lost (OR-15.4): the vault read, the copy written, the copy checked.
enum class MoveStep { Read, Written, Verified };

// A vault's key and keyfile fingerprint packed into one value, for the finger
// (D-038) and the PIN (D-059).
QByteArray quickSecretOf(const VaultKey& key, const QByteArray& keyfile);
std::optional<std::pair<VaultKey, QByteArray>> quickSecretFrom(const QByteArray& secret);


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
    // Sort By (D-081): what the list is sorted by, in the menu's order from
    // Title (0) to Frequently Used (5), and which way; kept per vault on this
    // computer.
    Q_PROPERTY(int sortBy READ sortBy WRITE setSortBy NOTIFY sortChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortChanged)
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
    // "Use the Esc key to exit the program" (D-048).
    Q_PROPERTY(bool escapeExits READ escapeExits WRITE setEscapeExits NOTIFY escapeExitsChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY trayAvailableChanged)
    Q_PROPERTY(bool hideSensitive READ hideSensitive WRITE setHideSensitive NOTIFY hideSensitiveChanged)
    Q_PROPERTY(bool showSubtitles READ showSubtitles WRITE setShowSubtitles NOTIFY showSubtitlesChanged)
    // The View menu's choices, kept in the settings (D-039).
    // Changes whenever a key is set, so bindings read the keys again.
    Q_PROPERTY(int shortcutsRevision READ shortcutsRevision NOTIFY shortcutsChanged)
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
    // The audit (D-065): for each list its number ("leaked", "reused", "weak",
    // "old", "breached"), "passwords" looked at, "score" (-1 for none),
    // "ready", and the state of the two online checks, "leakState" and
    // "breachState": "off", "checking", "done" or "failed".
    Q_PROPERTY(QVariantMap audit READ audit NOTIFY auditChanged)
    Q_PROPERTY(bool checkLeaks READ checkLeaks WRITE setCheckLeaks NOTIFY auditChanged)
    Q_PROPERTY(bool showAudit READ showAudit WRITE setShowAudit NOTIFY viewChanged)
    Q_PROPERTY(bool auditFolded READ auditFolded WRITE setAuditFolded NOTIFY viewChanged)
    Q_PROPERTY(bool useWebsiteIcons READ useWebsiteIcons WRITE setUseWebsiteIcons NOTIFY useWebsiteIconsChanged)
    Q_PROPERTY(bool quickUnlockAvailable READ quickUnlockAvailable NOTIFY quickUnlockChanged)
    Q_PROPERTY(bool quickUnlockOn READ quickUnlockOn NOTIFY quickUnlockChanged)
    Q_PROPERTY(bool quickUnlockEnabled READ quickUnlockEnabled NOTIFY quickUnlockChanged)
    // Turning it on: a finger asked once, and what came of it, for the
    // settings window (D-042).
    Q_PROPERTY(bool quickUnlockBusy READ quickUnlockBusy NOTIFY quickUnlockChanged)
    Q_PROPERTY(QString quickUnlockMessage READ quickUnlockMessage NOTIFY quickUnlockChanged)
    Q_PROPERTY(bool quickUnlockTrouble READ quickUnlockTrouble NOTIFY quickUnlockChanged)
    Q_PROPERTY(QString quickUnlockName READ quickUnlockName CONSTANT)
    Q_PROPERTY(bool reminderDue READ reminderDue NOTIFY quickUnlockChanged)
    // A PIN per vault (D-076): being set, and able to open the vault shown.
    Q_PROPERTY(int pinRevision READ pinRevision NOTIFY pinChanged)
    Q_PROPERTY(bool pinBusy READ pinBusy NOTIFY pinChanged)
    Q_PROPERTY(bool pinUnlockReady READ pinUnlockReady NOTIFY pinChanged)
    Q_PROPERTY(int pinShortest READ pinShortest CONSTANT)
    // How often the master password is asked though a finger would do:
    // 0 Day, 1 Month, 2 Quarter (the default), 3 Year, 4 Never (D-049).
    Q_PROPERTY(int masterPasswordReminder READ masterPasswordReminder WRITE setMasterPasswordReminder NOTIFY
                   quickUnlockChanged)
    Q_PROPERTY(int clipboardClearSeconds READ clipboardClearSeconds WRITE setClipboardClearSeconds NOTIFY
                   clipboardClearSecondsChanged)
    // WebDAV (D-057): whether the vault shown is kept on a server, why its
    // last sync failed ("" when it did not), whether a sync runs, a
    // certificate to be trusted (its fingerprint, "" for none) and whether
    // the server's password must be typed.
    Q_PROPERTY(bool onWebDav READ onWebDav NOTIFY syncChanged)
    // A cloud storage of any kind (D-080): whether the vault shown is kept in
    // one, that storage's name ("" for this device), and the storages this
    // build has, for every window that names one: kind, name, tile, place.
    Q_PROPERTY(bool inCloud READ inCloud NOTIFY syncChanged)
    Q_PROPERTY(QString storageName READ storageName NOTIFY syncChanged)
    Q_PROPERTY(QVariantList storages READ storages CONSTANT)
    // Google's sign-in of the vault shown must be renewed (taken away, or
    // not on this device): signInToCloudAgain opens the browser (D-080).
    Q_PROPERTY(bool cloudSignInNeeded READ cloudSignInNeeded NOTIFY syncChanged)
    Q_PROPERTY(QString syncProblem READ syncProblem NOTIFY syncChanged)
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncChanged)
    Q_PROPERTY(QString certificateAsked READ certificateAsked NOTIFY syncChanged)
    Q_PROPERTY(bool webDavPasswordNeeded READ webDavPasswordNeeded NOTIFY syncChanged)
    // The vaults whose files were found, the vault used last when it was not
    // (listed struck through), and no vault found at all (D-058, 0.3.65).
    Q_PROPERTY(QStringList foundVaults READ foundVaults NOTIFY vaultsFoundChanged)
    Q_PROPERTY(QString missingVault READ missingVault NOTIFY vaultsFoundChanged)
    Q_PROPERTY(bool noVaultFound READ noVaultFound NOTIFY vaultsFoundChanged)
    // The items of the vault shown with a conflict to settle (D-060).
    Q_PROPERTY(int conflictCount READ conflictCount NOTIFY revisionChanged)

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
    [[nodiscard]] int sortBy() const { return static_cast<int>(listModel.order().by); }
    void setSortBy(int by);
    [[nodiscard]] bool sortDescending() const { return listModel.order().descending; }
    void setSortDescending(bool descending);
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
    [[nodiscard]] bool escapeExits() const { return escExits; }
    void setEscapeExits(bool exits);
    [[nodiscard]] bool trayAvailable() const { return trayExists; }
    void setTrayAvailable(bool available);
    // Settings > Security and General (D-033).
    [[nodiscard]] bool hideSensitive() const { return hideSecrets; }
    void setHideSensitive(bool hide);
    [[nodiscard]] bool showSubtitles() const { return subtitlesShown; }
    // A touch screen is present: what shows under the mouse stays shown (D-034).
    [[nodiscard]] bool touchScreen() const;
    // Settings > General > Use website icons, off by default (D-036).
    [[nodiscard]] bool useWebsiteIcons() const { return websiteIconsOn; }
    void setUseWebsiteIcons(bool use);
    // Unlocking with a fingerprint (D-038).
    [[nodiscard]] bool quickUnlockAvailable() const { return quickAvailable; }
    // A master password, a PIN or a finger asked for any vault, the one shown or
    // another, in the standard dialog: nothing else changes until it was right
    // and the vault is opened, exported or read with it (D-073).
    Q_PROPERTY(QString askedVault READ askedVault NOTIFY askChanged)
    Q_PROPERTY(bool askBusy READ askBusy NOTIFY askChanged)
    Q_PROPERTY(QString askMessage READ askMessage NOTIFY askChanged)
    Q_PROPERTY(bool askTrouble READ askTrouble NOTIFY askChanged)
    Q_PROPERTY(bool askFingerOn READ askFingerOn NOTIFY askChanged)
    Q_PROPERTY(int askWaitSeconds READ askWaitSeconds NOTIFY askChanged)
    Q_PROPERTY(bool askKeyfileNeeded READ askKeyfileNeeded NOTIFY askChanged)
    Q_PROPERTY(QString askKeyfile READ askKeyfile NOTIFY askChanged)
    [[nodiscard]] QString askedVault() const { return askedFor; }
    [[nodiscard]] bool askBusy() const { return askIsBusy; }
    [[nodiscard]] QString askMessage() const { return askText; }
    [[nodiscard]] bool askTrouble() const { return askIsTrouble; }
    [[nodiscard]] bool askFingerOn() const;
    [[nodiscard]] int askWaitSeconds() const { return askWait; }
    [[nodiscard]] bool askKeyfileNeeded() const { return askNeedsKeyfile || !askKeyfile().isEmpty(); }
    [[nodiscard]] QString askKeyfile() const { return keyfileOf(askedFor); }
    Q_INVOKABLE void askFor(const QString& vaultFile);
    Q_INVOKABLE void stopAsking();
    Q_INVOKABLE void setAskKeyfile(const QUrl& keyfile);
    Q_INVOKABLE void proveWithPassword(const QString& password);
    Q_INVOKABLE void proveWithFinger();
    // With what was proven: the vault opened; the one shown exported; another read for an import.
    // With remember, a typed master password is also kept in the primary (D-074).
    Q_INVOKABLE bool openProven(bool remember = false);
    // "Remember my credentials": for a vault that is not the primary; possible
    // while the primary's key is known; a tick taken away is remembered (D-074).
    Q_PROPERTY(bool askRememberShown READ askRememberShown NOTIFY askChanged)
    Q_PROPERTY(bool askRememberPossible READ askRememberPossible NOTIFY askChanged)
    [[nodiscard]] bool askRememberShown() const;
    [[nodiscard]] bool askRememberPossible() const;
    [[nodiscard]] Q_INVOKABLE bool remembersFor(const QString& vaultFile) const;
    Q_INVOKABLE void setRemembersFor(const QString& vaultFile, bool remembers);
    // Another vault the primary: only that changes (the owner, D-074).
    Q_INVOKABLE void makePrimary(const QString& vaultFile);
    // How a vault feels: 2 Open (in it), 1 Closed (opens without a password),
    // 0 Locked (asks one). keptRevision changes when that may have (D-074).
    Q_PROPERTY(int keptRevision READ keptRevision NOTIFY keptChanged)
    [[nodiscard]] int keptRevision() const { return keptCount; }
    [[nodiscard]] Q_INVOKABLE int vaultStateOf(const QString& vaultFile) const;
    Q_INVOKABLE void exportProven(const QUrl& file);
    Q_INVOKABLE void importProven();
    // One setting for all vaults (the owner, D-043); a vault's key is kept at
    // its next master-password unlock.
    [[nodiscard]] bool quickUnlockEnabled() const;
    [[nodiscard]] bool quickUnlockOn() const
    {
        return quick && !path.isEmpty() && quickUnlockEnabled() && quick->has(path);
    }
    Q_INVOKABLE void clearQuickUnlockMessage() { tellSetup(QString(), false); }
    [[nodiscard]] QString quickUnlockName() const { return quick ? quick->name() : QString(); }
    // Once a year the master password is asked though a finger would do (the
    // owner, Q44), unless the reminder is off.
    [[nodiscard]] bool reminderDue() const;
    [[nodiscard]] int masterPasswordReminder() const { return reminderEvery; }
    void setMasterPasswordReminder(int every);
    Q_INVOKABLE void setQuickUnlock(bool on);
    Q_INVOKABLE void unlockWithQuickUnlock();
    // A PIN per vault, of at least pinMinimumLength characters, set with the
    // master password just proven in the standard dialog (D-076).
    Q_INVOKABLE void setPinFor(const QString& vaultFile, const QString& pin);
    Q_INVOKABLE void removePinFor(const QString& vaultFile);
    [[nodiscard]] Q_INVOKABLE bool hasPin(const QString& vaultFile) const;
    [[nodiscard]] int pinRevision() const { return pinCount; }
    Q_INVOKABLE void unlockWithPin(const QString& pin);
    [[nodiscard]] bool pinBusy() const { return settingPin; }
    [[nodiscard]] bool pinUnlockReady() const;
    static constexpr int pinMinimumLength = 4;
    [[nodiscard]] static int pinShortest() { return pinMinimumLength; }
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
    [[nodiscard]] bool showAudit() const { return viewOption(QStringLiteral("showAudit"), true); }
    void setShowAudit(bool on) { setViewOption(QStringLiteral("showAudit"), on); }
    [[nodiscard]] bool auditFolded() const { return viewOption(QStringLiteral("auditFolded"), false); }
    void setAuditFolded(bool on) { setViewOption(QStringLiteral("auditFolded"), on); }
    [[nodiscard]] QVariantMap audit() const;
    [[nodiscard]] bool checkLeaks() const { return leakChecks; }
    void setCheckLeaks(bool on);
    // The online checks that failed, once more.
    Q_INVOKABLE void checkLeaksNow();
    // What the audit says of one password: [{kind, text, address, items:
    // [{id, title}]}].
    [[nodiscard]] Q_INVOKABLE QVariantList auditRemarks(const QString& itemId, qint64 fieldUid) const;
    void setAuditAddressesForTests(std::function<QUrl(const QByteArray&)> range, const QUrl& breaches)
    {
        rangeAddress = std::move(range);
        breachesAddress = breaches;
    }
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
    // Whether switching to this vault opens it without asking its password.
    [[nodiscard]] Q_INVOKABLE bool opensAtOnce(const QString& vaultFile) const;
    // Shows another of the person's vaults: the primary opened with the key
    // kept from this session, one whose password is saved in the primary
    // opened with it, any other locked (D-030).
    Q_INVOKABLE void switchToVault(const QString& vaultFile);
    // Writes a new keyfile where the person chose; answered by keyfileMade.
    Q_INVOKABLE void makeKeyfile(const QUrl& file);
    // Writes the open vault, unencrypted, in the reference program's format
    // where the person chose, once the master password is right (D-037);
    // answered by exportDone. The file is readable by the person alone.
    // A new master password, and a keyfile or none, for the unlocked vault;
    // answered by masterPasswordChanged (D-031).
    Q_INVOKABLE void changeMasterPassword(const QString& current, const QString& password, const QString& repeated,
                                          const QUrl& keyfile);
    // Chooses an existing vault file; it is then unlocked with unlock().
    Q_INVOKABLE void chooseVault(const QUrl& file);
    // False, and says why, when a new vault cannot be made at this place:
    // asked right after the place is chosen, before any password is typed.
    Q_INVOKABLE bool canCreateAt(const QUrl& file);
    Q_INVOKABLE void unlock(const QString& password, bool remember = false);
    Q_INVOKABLE void lock();
    // Back to the start, to create or choose another vault.
    Q_INVOKABLE void closeVault();
    // The person agreed to give up a change kept through a lock: it goes once
    // another vault is opened, created or switched to, not before (OR-15).
    Q_INVOKABLE void giveUpKeptChangesOnLeaving();

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
    // Any category, built-in or the person's own (D-056): its name, its icon
    // (PNG in base64, "" for the program's own) and whether it is shown in
    // the left column. False, with a message, when it is not kept.
    Q_INVOKABLE bool editCategory(const QString& id, const QString& name, const QString& icon, bool shown);
    // A built-in category back as the program has it: its name, its icon,
    // shown in the left column (D-056).
    Q_INVOKABLE void resetCategory(const QString& id);
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
    // Importing another of Chrono Vault's vaults (D-060): opened with its
    // master password (and its keyfile, when one is remembered for it), off
    // the window's thread; answered by importRead like readImport.
    Q_INVOKABLE void readVaultImport(const QUrl& file, const QString& password);
    // The new conflicts the last import made (D-060).
    [[nodiscard]] Q_INVOKABLE int lastImportConflicts() const { return lastConflicts; }
    // A vault's place as the file windows take it, and back.
    [[nodiscard]] Q_INVOKABLE QUrl fileUrl(const QString& file) const { return QUrl::fromLocalFile(file); }
    [[nodiscard]] Q_INVOKABLE QString localPath(const QUrl& file) const
    {
        return file.isLocalFile() ? file.toLocalFile() : file.toString();
    }
    Q_INVOKABLE bool confirmImport();
    Q_INVOKABLE void cancelImport();
    [[nodiscard]] Q_INVOKABLE QString referenceName() const;
    // Stores the item (new or changed) and saves the vault.
    Q_INVOKABLE bool saveItem(const QVariantMap& item);
    // Deletes an item for good (from the Trash, after asking).
    Q_INVOKABLE void deleteItem(const QString& id);
    // What a merge left on an item (D-054): each a map with "kind" ("both" or
    // "deleted"), "label" (the field or text it concerns), "otherValue" and
    // "hidden" (a value shown as dots).
    [[nodiscard]] Q_INVOKABLE QVariantList mergeNotes(const QString& itemId) const;
    // The value that did not stand put in place; the one shown goes into the
    // field's history.
    Q_INVOKABLE void useOtherValue(const QString& itemId, int index);
    Q_INVOKABLE void dismissMergeNote(const QString& itemId, int index);
    // The Sync conflicts dialog (D-060), in vault_controller_conflicts.cpp.
    // Every item, and every category or template of the person's own (its id
    // "vault:…"), with a conflict: {itemId, what, name, fields, where, kind,
    // source, ofVault, canRetry, canKeepHere, canKeepBoth, found} with kind
    // "both" (differs on two devices), "imported" (differs from an import),
    // "deletedThere" (deleted on the other device, changed here) or
    // "deletedHere" (the other way round); source "both", "imported" or
    // "mixed" (D-061).
    [[nodiscard]] int conflictCount() const;
    [[nodiscard]] Q_INVOKABLE QVariantList conflicts() const;
    // One conflict: {itemId, what, name, where, kind, source, ofVault,
    // leftName, rightName, canRetry, canKeepHere, canKeepBoth, found, rows:
    // [{index, label, left, right, leftHidden, rightHidden, differs, canSplit,
    // first, second}], allRows}, the left side the vault or this device, the
    // right the import or the other device; rows what is to be chosen,
    // allRows every field of the item in its order; an empty map when it has
    // none.
    [[nodiscard]] Q_INVOKABLE QVariantMap conflictOf(const QString& itemId) const;
    // The two sides' names for a source of conflictOf.
    [[nodiscard]] Q_INVOKABLE QVariantMap sideNames(const QString& source) const;
    // Settles these whole: "left" takes every left value, "right" every
    // right one (an item deleted elsewhere goes to the Trash), "both" keeps
    // the item and a copy of it with the other values. "now" and "other" are
    // the value standing and the other value; "keep" (Keep here only) keeps
    // an item the other device deleted.
    Q_INVOKABLE void settleConflicts(const QStringList& itemIds, const QString& side);
    // Settles one per row of conflictOf: "left", "right" or "both" for each.
    Q_INVOKABLE void settleConflict(const QString& itemId, const QVariantList& choices);
    // Retry sync (D-061), in vault_controller_retry.cpp: each of these read
    // again from where its other values came from, and compared afresh;
    // answered by retried, or by retryPasswordNeeded for a vault whose key is
    // not at hand, which retryFromVault then reads.
    Q_INVOKABLE void retryConflicts(const QStringList& itemIds);
    Q_INVOKABLE void retryFromVault(const QString& source, const QStringList& itemIds, const QString& password);
    // What a window keeps of its own beside its place and size, such as the
    // widths of its columns.
    [[nodiscard]] Q_INVOKABLE QString windowValue(const QString& name) const;
    Q_INVOKABLE void keepWindowValue(const QString& name, const QString& value);
    // Moves an item into the Trash or out of it, into the Archive or out of
    // it (D-033).
    Q_INVOKABLE void moveToTrash(const QString& id, bool trashed);
    Q_INVOKABLE void archiveItem(const QString& id, bool archived);
    // Several items at once (D-062), in one save and one message: place is
    // "trashed", "archived", "favourite" or "noAudit".
    Q_INVOKABLE void placeItems(const QStringList& ids, const QString& place, bool on);
    // Their category alone: the fields and the template stay (D-063).
    Q_INVOKABLE void setCategoryOf(const QStringList& ids, const QString& categoryId);
    Q_INVOKABLE void deleteItems(const QStringList& ids);
    [[nodiscard]] Q_INVOKABLE QStringList trashedItems() const;
    // Reads a picked picture as an item icon (D-018): the PNG in base64, or
    // empty when the file is not used, with a message saying why.
    [[nodiscard]] Q_INVOKABLE QString readItemIcon(const QUrl& file);
    // The icon picker (D-050). The vault's own icons, PNG in base64, each
    // once.
    [[nodiscard]] Q_INVOKABLE QStringList customIcons() const;
    // Reads a picture file into the vault's own icons; returns it, PNG in
    // base64, or nothing when it cannot be read (and says why).
    Q_INVOKABLE QString addCustomIcon(const QUrl& file);
    // How many items carry this icon, and its removal: those items get their
    // category's icon again.
    [[nodiscard]] Q_INVOKABLE int customIconUses(const QString& icon) const;
    Q_INVOKABLE void deleteCustomIcon(const QString& icon);
    // The sections of the icons that ship with Chrono Vault, as icons.md in
    // the person's icon folder lists them: {heading, icons: [{name, url}]}.
    [[nodiscard]] Q_INVOKABLE QVariantList iconSections() const;
    // Saves again after a save failed.
    Q_INVOKABLE void retrySave();

    // WebDAV (D-057).
    [[nodiscard]] bool inCloud() const { return syncer != nullptr || webDavWaiting; }
    [[nodiscard]] bool onWebDav() const { return inCloud() && cloudKindNow == QLatin1String("webdav"); }
    [[nodiscard]] QString storageName() const;
    [[nodiscard]] QVariantList storages() const { return cloudKindsForWindows(); }
    [[nodiscard]] bool cloudSignInNeeded() const { return signInNeeded; }

    // Google Drive (D-080), in vault_controller_googledrive.cpp. Sign in in
    // the browser, then the "Chrono Vault" folder is shown: googleDriveConnected
    // answers, webDavFolderShown and webDavDone as for a WebDAV folder.
    Q_INVOKABLE void connectGoogleDrive();
    Q_INVOKABLE void cancelGoogleSignIn();
    Q_INVOKABLE void openGoogleDriveFolder(const QString& name);
    Q_INVOKABLE void makeGoogleDriveFolder(const QString& name);
    Q_INVOKABLE void createGoogleDriveVault(const QString& name, const QString& password, const QString& repeated,
                                            const QUrl& keyfile = QUrl(), bool savePassword = false);
    Q_INVOKABLE void openGoogleDriveVault(const QString& fileName);
    Q_INVOKABLE void moveVaultToGoogleDrive(const QString& vaultFile);
    // Disconnect… deletes nothing: the remote copy and this device's copy stay
    // two vaults of their own (D-060).
    Q_INVOKABLE void disconnectVault(const QString& vaultFile);
    Q_INVOKABLE void signInToCloudAgain();
    // Tests: the address the browser would open goes here.
    void setGoogleBrowserForTests(std::function<void(const QUrl&)> open) { googleBrowserForTests = std::move(open); }
    [[nodiscard]] QString syncProblem() const { return syncTrouble; }
    [[nodiscard]] bool syncing() const { return syncer && syncer->running(); }
    [[nodiscard]] QString certificateAsked() const { return shownFingerprint(askedCertificate); }
    [[nodiscard]] bool webDavPasswordNeeded() const { return passwordNeeded; }
    [[nodiscard]] QStringList foundVaults() const { return foundList; }
    [[nodiscard]] QString missingVault() const { return missingLast; }
    [[nodiscard]] bool noVaultFound() const { return !vaultList.isEmpty() && foundList.isEmpty() && !checkingVaults && currentState == State::NoVault; }
    // Looks which vaults are there, in the background; ↻ on the start page.
    Q_INVOKABLE void checkVaults();
    // Tools > Sync Now.
    Q_INVOKABLE void syncNow();
    // The answer to "Trust this certificate?" for the vault shown.
    Q_INVOKABLE void trustCertificate(bool trust);
    // The server's password, when the system's store does not have it.
    Q_INVOKABLE void giveWebDavPassword(const QString& password);
    // Signs in to a WebDAV folder and lists its vaults: webDavConnected
    // answers. trustedCertificate is a fingerprint the person trusted, or "".
    Q_INVOKABLE void connectWebDav(const QString& address, const QString& user, const QString& password,
                                   const QString& trustedCertificate);
    // The folder browser after Connect: a subfolder of the folder shown, ".."
    // for its parent, or a whole address on the same server (Back); a new
    // folder, which is then shown.
    Q_INVOKABLE void openWebDavFolder(const QString& name);
    Q_INVOKABLE void makeWebDavFolder(const QString& name);
    // An address that sends the password readable over the network.
    [[nodiscard]] Q_INVOKABLE bool isPlainHttp(const QString& address) const;
    // With the folder connected: a new vault kept there, one already there
    // copied to this device, or a vault of this device moved there;
    // webDavDone answers the last two.
    Q_INVOKABLE void createWebDavVault(const QString& name, const QString& password, const QString& repeated,
                                       const QUrl& keyfile = QUrl(), bool savePassword = false);
    Q_INVOKABLE void openWebDavVault(const QString& fileName);
    // Stops syncing a vault: it stays on this device as a vault of its own,
    // and the server's copy stays too; nothing is deleted (the owner, D-060).
    Q_INVOKABLE void disconnectWebDav(const QString& vaultFile);

    // Settings > Vaults' menu (D-060). Lock locks the vault when it is the
    // one unlocked.
    Q_INVOKABLE void lockVault(const QString& vaultFile);
    // Closes the open vault only; the key store stays open (D-075).
    Q_INVOKABLE void closeOpenVault(const QString& vaultFile);
    // The primary, or a vault whose password is kept in it.
    [[nodiscard]] Q_INVOKABLE bool isKept(const QString& vaultFile) const;
    // Whether a change kept through a lock is this vault's.
    [[nodiscard]] Q_INVOKABLE bool keepsChangesOf(const QString& vaultFile) const;
    // The kept passwords can be used: the primary was opened and not locked since.
    Q_PROPERTY(bool keyStoreOpen READ keyStoreOpen NOTIFY keptChanged)
    [[nodiscard]] bool keyStoreOpen() const { return primaryKey.isValid(); }
    // Another vault is being opened with what is kept, while the one shown
    // stays until it is open; nothing can be changed meanwhile (D-078).
    Q_PROPERTY(bool switching READ isSwitching NOTIFY switchingChanged)
    [[nodiscard]] bool isSwitching() const { return !switchingTo.isEmpty(); }
    // After a lock: the vault that comes back once the primary shown is unlocked.
    Q_PROPERTY(QString returnVault READ returnVault NOTIFY returnChanged)
    [[nodiscard]] QString returnVault() const { return returnTo; }
    Q_PROPERTY(QString lastUsedVault READ lastUsedVault NOTIFY vaultPathChanged)
    [[nodiscard]] QString lastUsedVault() const { return lastUsed; }
    // Moves a vault to a new file on this device, or into the WebDAV folder
    // connected now: locked first, copied, the copy read back and compared,
    // and only then the original deleted (OR-15). moveDone answers.
    Q_INVOKABLE void moveVaultTo(const QString& vaultFile, const QUrl& file);
    Q_INVOKABLE void moveVaultToWebDav(const QString& vaultFile);
    [[nodiscard]] bool moving() const { return moveRunning; }
    // Takes a vault off the list; a WebDAV vault's working copy is deleted at
    // once, a change not yet on the server with it (the owner, D-060). Never
    // the primary.
    Q_INVOKABLE void forgetVault(const QString& vaultFile);
    void setMoveStepCheckForTests(std::function<bool(MoveStep)> check) { moveStepCheck = std::move(check); }
    // For Settings > Vaults: inCloud, kind, storageName, onWebDav, address,
    // user, lastSynced (as words).
    [[nodiscard]] Q_INVOKABLE QVariantMap vaultPlace(const QString& vaultFile) const;
    // Tests: a stand-in for the system's store, and a sync stopped at a step.
    void setSecretStoreForTests(std::unique_ptr<SecretStore> standIn) { secrets = std::move(standIn); }
    void setSyncStepCheckForTests(std::function<bool(SyncStep)> check) { syncStepCheck = std::move(check); }
    [[nodiscard]] CloudSync* syncForTests() const { return syncer.get(); }

    // Copies text, marked as secret for clipboard history tools, and clears
    // it again after the chosen number of seconds (D-007).
    Q_INVOKABLE void copyToClipboard(const QString& text);
    // An item used: a field of it copied, its website opened, auto-type; kept
    // on this computer only, for Recently Used and Frequently Used (D-081).
    Q_INVOKABLE void usedItem(const QString& itemId);
    // Tools > Clear Clipboard: empties it at once, whatever is on it (D-039).
    Q_INVOKABLE void clearClipboard();
    // The keyboard shortcuts, each action's keys as the person set them, or
    // the ones Chrono Vault starts with (D-045). An empty list: no key.
    [[nodiscard]] Q_INVOKABLE QStringList keysOf(const QString& action) const;
    [[nodiscard]] Q_INVOKABLE QStringList defaultKeysOf(const QString& action) const;
    Q_INVOKABLE void setKeys(const QString& action, const QStringList& keys);
    // A pressed key in words, modifiers first as the menus write them:
    // "Shift+Ctrl+B"; empty for a modifier alone.
    [[nodiscard]] Q_INVOKABLE QString keyName(int keyCode, int modifiers) const;
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
    void switchingChanged();
    // The vault being switched to did not open; the one shown stays (D-078).
    void switchFailed(const QString& vaultFile);
    void vaultPathChanged();
    void messageChanged();
    void searchTextChanged();
    void sortChanged();
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
    void retried(const QString& text, bool trouble);
    // problem: why it is asked again, or empty.
    void retryPasswordNeeded(const QString& source, const QString& vaultName, const QStringList& itemIds,
                             const QString& problem);
    void exportDone(bool ok);
    void autoLockMinutesChanged();
    void systemIdleAvailableChanged();
    void unlockWaitChanged();
    void askChanged();
    void keptChanged();
    void returnChanged();
    // Close or Lock of a vault's menu could not be done; the reason.
    void vaultMenuProblem(const QString& text);
    void proven(const QString& vaultFile);
    void clipboardClearSecondsChanged();
    void hideSensitiveChanged();
    void showSubtitlesChanged();
    void viewChanged();
    void shortcutsChanged();
    void useWebsiteIconsChanged();
    void auditChanged();
    void quickUnlockChanged();
    void pinChanged();
    void keepInTrayChanged();
    void startAtStartupChanged();
    void startMinimizedChanged();
    void escapeExitsChanged();
    void trayAvailableChanged();
    void unsavedChangesChanged();
    void saveFailedChanged();
    void keptChangesChanged();
    // The vault is about to lock: the windows store what is being typed.
    void locking();
    // The vault was saved after a change; the item with this id, if any, is
    // the one to show.
    void saved(const QString& itemId);
    void syncChanged();
    void vaultsFoundChanged();
    // The answer to connectWebDav: the vault files in the folder, or why
    // not; certificate is the fingerprint to ask about, when that is why.
    void webDavConnected(bool ok, const QString& problem, const QStringList& vaultFiles, const QString& certificate);
    void webDavDone(bool ok, const QString& problem);
    // The answer to moveVaultTo and moveVaultToWebDav (D-060).
    void moveDone(bool ok, const QString& problem);
    // A new WebDAV vault that could not be made or put on the server; nothing was kept.
    void webDavCreateFailed(const QString& problem);
    void googleDriveCreateFailed(const QString& problem);
    void googleDriveConnected(bool ok, const QString& problem);
    // A folder of the server, listed, for the WebDAV file dialog: each entry a
    // map of name, folder, size and modified (as words), folders first.
    void webDavFolderShown(const QString& address, const QVariantList& entries, const QStringList& vaultFiles);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // The icon the person gave a category, built-in or their own; empty for none (D-056).
    [[nodiscard]] QByteArray givenCategoryIcon(const QString& id) const;
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
    void setPath(const QString& newPath, bool used = true);
    void saveInBackground(const QString& itemId);
    void restartAutoLock();
    void forgetUnlockedVault();
    void setDirty(bool isDirty);
    void setFailed(bool hasFailed);
    void keepUnsavedWhileLocked();
    void takeBackUnsaved();
    void discardKeptChanges();
    // Called as the person goes to another vault.
    void leavingFor(const QString& vaultFile);
    // The list and every window that shows the contents are told of a change.
    void contentsChanged();
    void setItemPlace(const QString& id, bool Item::*place, bool on);
    [[nodiscard]] qsizetype attachmentBytes() const;
    void addVault(const QString& vaultFile);
    void rememberKeyfile(const QString& vaultFile, const QString& keyfile);
    [[nodiscard]] QString keyfileOf(const QString& vaultFile) const;
    // Unlocks the vault shown with a password, or with a key kept from
    // before; both off the window's thread (OR-5).
    void startUnlock(const QByteArray& password, bool savedPassword = false, bool remember = false);
    void startOpenWithKey(const VaultKey& kept);
    void startSwitch(const QString& vaultFile, bool withPrimaryKey);
    void cancelSwitch();
    void unlocked(const VaultKey& newKey, const VaultContents& read, const QByteArray& keyfile);
    // The primary's key and the passwords saved in it, kept while unlocked.
    void rememberPrimary();
    void forgetPrimary();
    void lockEverything();
    void leaveOpenVault();
    void goBackThroughPrimary(const QByteArray& secret);
    void dropSavedPassword(const QString& vaultFile);
    [[nodiscard]] bool canRememberHere() const;
    void showCreated(CreatedVault created);
    // The end of creating a vault: in the list, its password perhaps in the
    // primary, shown (D-030); a WebDAV vault only after its first upload (D-057).
    void finishCreating(const QString& newPath, VaultKey newKey, const QByteArray& fingerprint,
                        const QByteArray& written, const QString& keyfileFile, State before,
                        const QString& savedPassword);
    void connectQuickUnlock();
    // The lock as a screen lock (D-076): what is open put away under the vault
    // logged in with, taken back with its key or its PIN's.
    void sealSession();
    void dropSession();
    bool takeBackSession(const QByteArray& opener, bool byPin);
    void openTakenBack();
    [[nodiscard]] bool sessionPinReady() const;
    [[nodiscard]] static QString pinGroupOf(const QString& vaultFile);
    static void dropOldPin();
    static void resealPin(const QString& vaultFile, const VaultKey& oldKey, const VaultKey& newKey);
    void recordMasterPassword();
    void recordMasterPasswordIfNone();
    // Asks each website not asked yet for its icon (D-036).
    void fetchWebsiteIcons();
    // A change made elsewhere (D-032): the stamp of each vault file as this
    // Chrono Vault last read or wrote it, shared with the save thread.
    struct Stamps
    {
        QMutex lock;
        // Raised each time a merge is taken in: a save queued before it is
        // passed over (D-054).
        std::atomic<quint64> epoch{0};
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
    void mergeWithOther(const QString& vaultFile, const VaultContents& theirs, const QByteArray& theirStamp,
                        const QString& itemId);
    [[nodiscard]] static QString mergeText(const MergeResult& merged);

    // A vault kept in a cloud storage (D-057, D-080), in vault_controller_webdav.cpp.
    void startWebDav();
    void stopWebDav();
    void makeSyncer(const CloudPlace& place, const QString& secret);
    void syncFinished(SyncOutcome outcome, const QByteArray& certificate, const QString& conflictCopy);
    // A storage's secret (a password, a token), kept in memory and in the
    // system's store under the place's secret name.
    void keepCloudSecret(const CloudPlace& place, const QString& secret);
    void withCloudSecret(const CloudPlace& place, std::function<void(std::optional<QString>)> then);
    void keepWebDavPassword(const WebDavPlace& place, const QString& password)
    {
        keepCloudSecret(place.toCloud(), password);
    }
    void withWebDavPassword(const WebDavPlace& place, std::function<void(std::optional<QString>)> then)
    {
        withCloudSecret(place.toCloud(), std::move(then));
    }
    [[nodiscard]] QString newWorkingCopyPath(const QString& name) const;
    [[nodiscard]] QString newWorkingCopyPath(const QString& kind, const QString& name) const;
    // Shared by every storage (D-080): a new vault, one already there, one
    // of this device moved there.
    void createCloudVault(const std::optional<CloudPlace>& target, const QString& secret, const QStringList& files,
                          const QString& name, const QString& password, const QString& repeated, const QUrl& keyfile,
                          bool savePassword);
    void openCloudVault(const CloudPlace& target, const QString& secret, const QString& fileName,
                        std::function<void(const QString&, CloudStorage::Done)> read, std::function<bool()> stillAsked);
    void moveVaultToCloud(const QString& vaultFile, const std::optional<CloudPlace>& target, const QString& secret,
                          const QStringList& files, const QString& notConnected);
    // A folder's entries for the file dialog, folders first; its vaults into files.
    [[nodiscard]] QVariantList shownEntries(const QList<CloudEntry>& entries, QStringList& files) const;
    [[nodiscard]] static QString cloudProblem(const QString& kind, const CloudReply& answer);
    [[nodiscard]] std::optional<CloudPlace> driveTarget() const;
    void showGoogleDrive();
    void listGoogleDrive(bool connecting);
    [[nodiscard]] static QString shownFingerprint(const QByteArray& fingerprint);
    [[nodiscard]] static QString connectProblem(const WebDavReply& answer);
    [[nodiscard]] static QString webDavNameProblem(const QString& name);
    void showWebDavFolder(const WebDavAccount& account, bool connecting);
    // The password saved in the primary for this vault, changed with it.
    // With add, kept there for the first time when it is not yet (D-074).
    void updateSavedPassword(const QString& vaultFile, const QString& password, bool add = false);

    // An imported item taken in (D-060, D-061), in vault_controller_import.cpp:
    // an item the vault does not have is added; one it deleted for good comes
    // back with a conflict; one it has is left as it is when the same, and
    // otherwise keeps its values with a conflict per difference, its other
    // value the imported one. True when it is a new conflict.
    bool takeImported(const Item& imported, const QDateTime& now, bool* added);
    // A source read off the window's thread: another vault, with its password
    // or a key kept for it, or an export.
    struct SourceRead
    {
        ImportResult result;
        QString why;                // why it could not be read, or empty
        bool passwordNeeded = false; // the password or the key was not the vault's
        VaultKey key;
    };
    [[nodiscard]] static SourceRead readVaultSource(const QString& file, QByteArray password,
                                                    const std::optional<VaultKey>& kept, const QString& keyfile,
                                                    const VaultContents& snapshot);
    [[nodiscard]] static SourceRead readExportSource(const QString& file, const VaultContents& snapshot);
    void keepSourceKey(const QString& file, const VaultKey& sourceKey);
    void retryFrom(const QString& source, const QStringList& itemIds, const QString& password);
    void retryRead(const QString& source, const QStringList& itemIds, const SourceRead& read);
    void retryShared();
    // The Sync conflicts dialog's helpers (D-061), in vault_controller_conflicts.cpp.
    [[nodiscard]] QVariantMap conflictRow(const Item& item, const MergeNote& note) const;
    [[nodiscard]] QVariantList conflictFields(const Item& item, const QVariantList& choices,
                                              const QString& deletedSide) const;
    [[nodiscard]] QVariantMap conflictWays(const QString& kind, const QList<MergeNote>& notes, bool ofVault) const;
    bool settleNote(Item& item, const MergeNote& note, bool takeOther, const QDateTime& now);
    void removeForGood(const QList<QUuid>& ids, const QDateTime& now);

    // The vault menu (D-060), in vault_controller_vaults.cpp.
    // Why this vault cannot be moved now, or "".
    [[nodiscard]] QString moveProblem(const QString& vaultFile) const;
    // The vault's bytes, from its file or, for a WebDAV vault, its server.
    void readForMove(const QString& vaultFile, std::function<void(std::optional<QByteArray>, QString)> then);
    // The copy is in place and checked: the original goes, and the list, the
    // vault's settings and the vault shown follow the new place.
    void finishMove(const QString& vaultFile, const QString& newPath);
    void moveFailed(const QString& problem);
    [[nodiscard]] bool moveGoesOn(MoveStep step) const { return !moveStepCheck || moveStepCheck(step); }
    // A vault's own settings (keyfile, PIN, waits, fingerprint) carried to
    // its new place, or dropped when to is empty.
    void carryVaultSettings(const QString& from, const QString& to);
    // A moved vault's password saved in the primary follows it at the
    // primary's next unlock (D-060).
    void followMovedVaults();

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
    int keysRevision = 0;
    [[nodiscard]] int shortcutsRevision() const { return keysRevision; }
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
    [[nodiscard]] static QString openStatusText(OpenStatus status);
    static void forgetWrongPasswords(const QString& vaultFile);
    void countWrongPassword(const QString& vaultFile);
    [[nodiscard]] int waitLeft(const QString& vaultFile) const;
    [[nodiscard]] bool reminderDueFor(const QString& vaultFile, bool easier) const;
    void readForImport(const QString& source, const QByteArray& password, const std::optional<VaultKey>& kept);
    void tellAsked(const QString& text, bool isTrouble);
    void checkProven(const VaultKey& candidate, const QByteArray& keyfile, int by);
    void provenNow(const VaultKey& right, const QByteArray& keyfile, bool typed, const QByteArray& words = {});
    void askedFingerAnswered(int result, const QByteArray& secret);
    void wipeProof();
    void updateAskWait();
    QString askedFor;
    bool askIsBusy = false;
    bool askingFinger = false;
    bool askNeedsKeyfile = false;
    QString askText;
    bool askIsTrouble = false;
    int askWait = 0;
    quint64 askAttempt = 0;
    QTimer askWaitTimer;
    VaultKey proofKey;
    QByteArray proofKeyfile;
    bool proofTyped = false;
    QByteArray proofWords;
    int keptCount = 0;
    void tellKept();
    bool openingByTyped = false;
    void wrongPasswordTyped();
    void rightPasswordGiven();
    void updateWait();
    void setBool(bool& value, bool on, const QString& settingName);
    bool quickAvailable = false;
    int reminderEvery = 2;
    bool replacingQuickKey = false; // storing the new key after a master password change
    bool settingUp = false;   // a finger asked to turn it on
    bool keepingQuietly = false; // a vault's key kept at its master-password unlock
    void keepForThisVault();
    bool retrieving = false;  // a finger asked to unlock
    QString setupMessage;
    bool setupTrouble = false;
    void tellSetup(const QString& text, bool isTrouble);
    bool openingByFinger = false;   // the vault is being opened with the key kept for the finger
    // The audit, in vault_controller_audit.cpp (D-065). Found again after every
    // change; what the online checks said lives only while unlocked.
    void startAudit();
    void showAudited();
    void askLeaks();
    void askNextLeak();
    void askBreaches();
    void forgetAudit();
    void stopOnlineChecks();
    QList<AuditedPassword> audited;
    bool auditReady = false;
    bool auditRunning = false;
    bool auditAgain = false;
    bool leakChecks = false;
    QString leakState = QStringLiteral("off");
    QString breachState = QStringLiteral("off");
    QHash<QByteArray, int> leakCounts; // by whole hash; 0 is not leaked
    QList<QByteArray> leakQueue;       // first five digits still to be asked
    int leaksAsked = 0;                // on their way
    quint64 onlineRound = 0;           // an answer of an earlier one is dropped
    QList<Breach> breachList;
    QNetworkAccessManager auditNetwork;
    std::function<QUrl(const QByteArray&)> rangeAddress;
    QUrl breachesAddress;
    QNetworkAccessManager iconNetwork;
    QHash<QString, QString> websiteIcons; // host: "data:" address of its PNG, in memory only
    QSet<QString> iconsAsked;
    std::function<QUrl(const QString&)> iconAddress;
    bool keepRunning = true;
    bool minimizedAtStart = false;
    bool escExits = false;
    bool trayExists = false;

    VaultContents contents;
    // The version this Chrono Vault last read from or wrote to the vault file:
    // what both sides agreed on, the base of a merge (D-054).
    VaultContents base;
    QString mergeSaid; // the message of the last merge, kept after the save that follows
    VaultKey key;
    crypto::KdfParameters kdfParameters;
    ItemListModel listModel;
    // The vault whose Sort By and uses the list has (D-081).
    QString orderVault;
    void keepOrder(ItemOrder order);

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
    QString sessionLogin;                  // after a lock, the vault logged in with (D-076)
    QByteArray sessionByKey;               // what was open, sealed with that vault's key
    QByteArray sessionByPin;               // and with its PIN's
    QByteArray returnSecret;               // the key of the vault that was open, taken back
    int pinCount = 0;
    bool settingPin = false;
    bool openingByPin = false;
    VaultKey primaryKey;                   // kept while unlocked (D-030)
    QByteArray primaryKeyfile;             // the primary's keyfile fingerprint, with its key
    std::optional<CreatedVault> createdVault; // see CreatedVault
    QHash<QString, QString> savedPasswords; // vault file to its password, from the primary
    // A saved password found not to open its vault: not used again while the
    // primary still holds it, so the vault's own password is asked (D-078).
    QHash<QString, QString> stalePasswords;
    QString returnTo;                      // the vault shown before the lock, reached through the primary
    QString lastUsed;
    bool onlyKeyStore = false;             // the primary is opened for its kept passwords only
    std::optional<ImportResult> pendingImport; // read, waiting for the person's OK
    QString pendingSource; // where it came from, as a MergeNote's source
    QHash<QString, QByteArray> sourceKeys; // an imported vault's key, sealed, until the lock (D-061)
    bool retryWaiting = false; // Retry sync waits for a sync to end
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
    QByteArray unsavedBaseSealed; // the base of that change, for a merge at the unlock (D-054)
    QString unsavedPath;
    quint64 unlockAttempt = 0;
    QString switchingTo;
    quint64 switchAttempt = 0;

    QTimer autoLockTimer;
    QTimer clipboardTimer;
    QString copiedText;

    // WebDAV (D-057).
    void vaultsChecked(const QStringList& checked, QStringList found);
    QStringList foundList;
    QString missingLast;
    bool checkingVaults = false;
    bool keptChangesGivenUp = false;
    qint64 missingSinceMs = 0;
    QTimer vaultRetry;
    std::unique_ptr<CloudSync> syncer;
    QString cloudKindNow; // the storage of the vault shown, "" for this device
    std::shared_ptr<SecretStore> secrets; // shared with the threads that ask it
    std::function<bool(SyncStep)> syncStepCheck;
    QTimer syncTimer;      // every 10 minutes while unlocked (ref)
    QTimer syncSoonTimer;  // within 5 seconds of a save (ref)
    QString syncTrouble;
    QByteArray askedCertificate;
    bool passwordNeeded = false;
    bool webDavWaiting = false;           // the shown vault's sync waits for its password
    QHash<QString, QString> webDavPasswords; // every storage's secret by place id, in memory while the program runs
    std::unique_ptr<WebDavClient> connection; // the folder shown in the WebDAV window
    std::unique_ptr<WebDavClient> browsing;   // a folder being listed, shown once its list came
    QStringList connectedFiles;
    bool signInNeeded = false;
    QString googleToken; // the Google Drive window's own sign-in, in memory only
    QString googleAccount; // that sign-in's email, kept with the vaults made, added or moved there
    std::unique_ptr<GoogleSignIn> googleSignIn;
    std::unique_ptr<GoogleDriveStorage> driveSigningIn;
    std::unique_ptr<GoogleDriveStorage> drive; // the folder shown in the Google Drive window
    QStringList driveFiles;
    std::function<void(const QUrl&)> googleBrowserForTests;
    int lastConflicts = 0;
    bool moveRunning = false;
    QString movingVault; // never unlocked while it is moved (OR-15)
    [[nodiscard]] bool refusedWhileMoving();
    std::function<bool(MoveStep)> moveStepCheck;
};

} // namespace chrono_vault
