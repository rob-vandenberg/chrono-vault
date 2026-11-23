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

#include "vault_controller.h"

#include "app_setup.h"

#include "autostart.h"
#include "icon_library.h"
#include "item_icons.h"
#include "json_export.h"
#include "password_generator.h"
#include "templates.h"
#include "totp.h"
#include "version.h"

#include <QClipboard>
#include <QDataStream>
#include <QCollator>
#include <QCryptographicHash>
#include <QDateTime>
#include <QKeySequence>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QGuiApplication>
#include <QInputDevice>
#include <QMimeData>
#include <QPointer>
#include "settings_files.h"
#include <QVariantList>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <utility>

// --- Version ------------------------------------------------------------
// vault_controller.cpp 0.3.64

// --- Version history ----------------------------------------------------
// v0.3.64: A lock keeps the vault used last on the unlock page (the owner, D-058)
// v0.3.62: The end of creating a vault a function of its own, for a WebDAV vault's first upload (D-057)
// v0.3.60: The WebDAV sync started at an unlock, stopped at a lock, kept up with a new key (D-057)
// v0.2.56: Editing and resetting every category; the icons' names translated (D-056)
// v0.2.45: A change here and one elsewhere merged against the base; the notes a merge leaves (D-054)
// v0.1.41: The icon sections' loop variable iconPath, no longer hiding a member (Microsoft's compiler)
// v0.0.40: The vault's own icons and the icon sections (D-050); keyName's parameter no longer hides a member
// v0.0.39: The reminder every Day, Month, Quarter, Year or Never; no local key hiding the member (D-049)
// v0.0.38: Settings in chrono-vault.ini, vaults in chrono-vault.conf, no carry-over; the Esc setting (D-026, D-048)
// v0.0.23: Unlocking with a fingerprint, and the yearly master password reminder (D-038)
// v0.0.22: Export in the reference program's format, after the master password, off the window's thread (D-037)
// v0.0.20: Website icons, asked of each website itself, kept in memory only (D-036)
// v0.0.19: Importing the reference program's export, read off the window's thread, added in one save (D-035)
// v0.0.18: Whether a touch screen is present (D-034)
// v0.0.17: The reference program's fields and items for the windows: sections, field types, hide marks, hidden and
//          deleted fields, field history; Trash and Archive; Hide sensitive fields and Show subtitles (D-033)
// v0.0.16: A change made elsewhere to the vault file noticed, read again, or kept beside it as a conflict copy (D-032)
// v0.0.15: Several vaults, the first the primary, whose master password unlocks the others; a keyfile; changing the
//          master password or keyfile (D-030, D-031)
// v0.0.14: Categories and templates, the person's own too; tags; favourites; attachments, read and written off the
//          window's thread; the contents put into words off the window's thread when saved (D-027 to D-029, OR-5)
// v0.0.12: --reset: the settings not read at the start (D-026)
// v0.0.10: The tray and start settings no longer in a group named general, their old values carried over (D-019)
// v0.0.6: The settings' defaults, for Restore defaults (D-022)
// v0.0.4: The program's version, for the window titles; the General settings; item icons
// v0.0.3: Initial release

namespace chrono_vault {

namespace {

const QString lastVaultKey = QStringLiteral("vault/lastPath");
const QString vaultListKey = QStringLiteral("vaults/list");
const QString keyfilesKey = QStringLiteral("vaults/keyfiles");
const QString autoLockKey = QStringLiteral("security/autoLockMinutes");
const QString idleLockKey = QStringLiteral("security/idleLockOn");
const QString lockClosedKey = QStringLiteral("security/lockWhenClosed");
const QString lockSleepKey = QStringLiteral("security/lockOnSleep");
const QString systemIdleLockKey = QStringLiteral("security/systemIdleLockOn");
const QString systemIdleMinutesKey = QStringLiteral("security/systemIdleMinutes");
constexpr int defaultSystemIdleMinutes = 10;
const QString wrongAttemptsGroup = QStringLiteral("security/wrongAttempts/");
// The settings as they are before the person changes them, also what
// Restore defaults puts back (D-022).
constexpr int defaultAutoLockMinutes = 5;
constexpr int defaultClipboardSeconds = 30;
constexpr bool defaultKeepInTray = true;
constexpr bool defaultStartMinimized = false;
constexpr bool defaultEscapeExits = false; // off (the owner, D-048)
constexpr bool defaultStartAtStartup = false;
const QString clipboardKey = QStringLiteral("clipboard/clearSeconds");
const QString hideSensitiveKey = QStringLiteral("view/hideSensitive");
const QString showSubtitlesKey = QStringLiteral("view/showSubtitles");
const QString websiteIconsKey = QStringLiteral("view/websiteIcons");
const QString reminderKey = QStringLiteral("security/masterPasswordReminderEvery");
const QString lastPasswordKey = QStringLiteral("security/lastMasterPassword");
constexpr int reminderNever = 4;
constexpr int defaultReminder = 2; // Quarter (the owner, D-049)

// The vault's key whole, and its keyfile's fingerprint, as kept for the
// finger (D-038).
QByteArray quickSecretOf(const VaultKey& key, const QByteArray& keyfile)
{
    QByteArray secret;
    QDataStream out(&secret, QIODevice::WriteOnly);
    out << quint8(1) << key.key << key.salt << key.parameters.memoryKiB << key.parameters.iterations
        << key.parameters.lanes << key.usesKeyfile << keyfile;
    return secret;
}

std::optional<std::pair<VaultKey, QByteArray>> quickSecretFrom(const QByteArray& secret)
{
    QDataStream in(secret);
    quint8 version = 0;
    VaultKey key;
    QByteArray keyfile;
    in >> version >> key.key >> key.salt >> key.parameters.memoryKiB >> key.parameters.iterations >>
        key.parameters.lanes >> key.usesKeyfile >> keyfile;
    if (in.status() != QDataStream::Ok || version != 1 || !key.isValid()) {
        return std::nullopt;
    }
    return std::pair{key, keyfile};
}
constexpr bool defaultWebsiteIcons = false;
constexpr qint64 largestWebsiteIconBytes = 512 * 1024;
constexpr bool defaultHideSensitive = true;
constexpr bool defaultShowSubtitles = true;
// Never in a group named "general": in a settings file Qt keeps that name
// for its own [General] section and reads the value back under "General", so
// a choice was lost at the next start (D-019).
const QString keepInTrayKey = QStringLiteral("tray/keepRunning");
const QString startMinimizedKey = QStringLiteral("start/minimized");
const QString escapeExitsKey = QStringLiteral("start/escapeExits");

QString kindName(FieldKind kind)
{
    return fieldKindName(kind);
}

FieldKind kindFromName(const QString& name)
{
    return fieldKindFromName(name).value_or(FieldKind::Text);
}

// A moment as the reference program shows it: the long date with the
// weekday, then the time with seconds, without a time zone (D-033).
QString momentText(const QDateTime& moment)
{
    const QDateTime local = moment.toLocalTime();
    return QLocale().toString(local.date(), QLocale::LongFormat) + QLatin1Char(' ') +
           local.time().toString(QStringLiteral("HH:mm:ss"));
}

// A name at the start of a sentence: "fingerprint" becomes "Fingerprint".
QString sentenceStart(QString name)
{
    if (!name.isEmpty()) {
        name[0] = name.at(0).toUpper();
    }
    return name;
}

QVariantList fieldsToList(const QList<Field>& fields, bool withValues)
{
    QVariantList list;
    for (const Field& field : fields) {
        QVariantMap map{
            {QStringLiteral("role"), field.role},
            {QStringLiteral("label"), field.label},
            {QStringLiteral("name"), field.isCustom() ? field.label : roleName(field.role)},
            {QStringLiteral("kind"), kindName(field.kind)},
            {QStringLiteral("sensitive"), field.isHidden()},
            {QStringLiteral("deleted"), field.deleted},
            {QStringLiteral("hiddenType"), field.hiddenType},
            {QStringLiteral("uid"), field.uid},
        };
        if (withValues) {
            map.insert(QStringLiteral("value"), field.value);
            QVariantList history;
            for (auto change = field.history.crbegin(); change != field.history.crend(); ++change) {
                history.append(QVariantMap{
                    {QStringLiteral("value"), change->value},
                    {QStringLiteral("changed"), momentText(change->changed)},
                });
            }
            map.insert(QStringLiteral("history"), history); // newest first
        }
        list.append(map);
    }
    return list;
}

// The fields as the editors give them back; a custom field without a name
// gets one.
QList<Field> fieldsFromList(const QVariantList& list, bool withValues)
{
    QList<Field> fields;
    for (const QVariant& value : list) {
        const QVariantMap fieldMap = value.toMap();
        Field field;
        field.role = fieldMap.value(QStringLiteral("role")).toString();
        field.label = field.role.isEmpty() ? fieldMap.value(QStringLiteral("label")).toString().trimmed() : QString();
        if (field.role.isEmpty() && field.label.isEmpty()) {
            field.label = QCoreApplication::translate("VaultController", "Field");
        }
        field.kind = kindFromName(fieldMap.value(QStringLiteral("kind")).toString());
        field.sensitive = fieldMap.value(QStringLiteral("sensitive")).toBool() || field.kind == FieldKind::Password;
        field.deleted = fieldMap.value(QStringLiteral("deleted")).toBool();
        field.hiddenType = fieldMap.value(QStringLiteral("hiddenType")).toString();
        field.uid = fieldMap.value(QStringLiteral("uid")).toLongLong();
        if (field.kind == FieldKind::Section && field.label.isEmpty()) {
            field.label = QCoreApplication::translate("VaultController", "Section");
        }
        if (withValues) {
            field.value = fieldMap.value(QStringLiteral("value")).toString();
        }
        fields.append(field);
    }
    return fields;
}

QVariantMap toMap(const Item& item, const VaultContents& contents)
{
    QVariantList attachments;
    for (const Attachment& attachment : item.attachments) {
        attachments.append(QVariantMap{
            {QStringLiteral("id"), attachment.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("name"), attachment.name},
            {QStringLiteral("size"), static_cast<qint64>(attachment.data.size())},
        });
    }
    const auto itemTemplate = findTemplate(item.templateId, contents);
    return {
        {QStringLiteral("id"), item.id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("templateId"), item.templateId},
        {QStringLiteral("templateName"),
         itemTemplate ? templateName(*itemTemplate) : categoryName(item.categoryId, contents)},
        {QStringLiteral("categoryId"), item.categoryId},
        {QStringLiteral("isNote"), item.categoryId == QLatin1String("note")},
        {QStringLiteral("title"), item.title},
        {QStringLiteral("subtitle"), item.subtitle},
        {QStringLiteral("note"), item.note},
        {QStringLiteral("trashed"), item.trashed},
        {QStringLiteral("archived"), item.archived},
        {QStringLiteral("autoSubmit"), item.autoSubmit},
        {QStringLiteral("website"), item.website},
        {QStringLiteral("created"), momentText(item.created)},
        {QStringLiteral("modified"), momentText(item.modified)},
        {QStringLiteral("fields"), fieldsToList(item.fields, true)},
        {QStringLiteral("icon"), QString::fromLatin1(item.icon.toBase64())},
        {QStringLiteral("defaultIcon"), categoryIconSource(item.categoryId, contents)},
        {QStringLiteral("tags"), item.tags},
        {QStringLiteral("favourite"), item.favourite},
        {QStringLiteral("attachments"), attachments},
    };
}

QVariantMap templateToMap(const Template& itemTemplate, const VaultContents& contents)
{
    return {
        {QStringLiteral("id"), itemTemplate.id},
        {QStringLiteral("name"), templateName(itemTemplate)},
        {QStringLiteral("categoryId"), itemTemplate.categoryId},
        {QStringLiteral("categoryName"), categoryName(itemTemplate.categoryId, contents)},
        {QStringLiteral("builtIn"), itemTemplate.builtIn},
        {QStringLiteral("fields"), fieldsToList(itemTemplate.fields, false)},
    };
}

// Whether a tag is this one, or nested under it.
bool isUnder(const QString& tag, const QString& top)
{
    return tag == top || tag.startsWith(top + QLatin1Char('/'));
}

QString openStatusText(OpenStatus status)
{
    switch (status) {
    case OpenStatus::Opened: return {};
    case OpenStatus::FileMissing:
        return QCoreApplication::translate("VaultController", "The vault file is no longer where it was.");
    case OpenStatus::CannotRead:
        return QCoreApplication::translate("VaultController", "The vault file could not be read.");
    case OpenStatus::NotAVault:
        return QCoreApplication::translate("VaultController", "This file is not a Chrono Vault vault.");
    case OpenStatus::NewerFormat:
        return QCoreApplication::translate("VaultController",
                                           "This vault was made by a newer Chrono Vault. Update Chrono Vault to open it.");
    case OpenStatus::WrongPasswordOrDamaged:
        return QCoreApplication::translate("VaultController", "That password does not open this vault.");
    case OpenStatus::TooLarge:
        return QCoreApplication::translate("VaultController", "This file is too large to be a vault.");
    case OpenStatus::KeyChanged:
        return QCoreApplication::translate("VaultController",
                                           "This vault now has another master password. Unlock it with that one.");
    case OpenStatus::KeyfileNeeded:
        return QCoreApplication::translate("VaultController",
                                           "This vault also needs its keyfile. Choose it below, then unlock.");
    }
    return {};
}

QString saveStatusText(SaveStatus status)
{
    switch (status) {
    case SaveStatus::Saved: return {};
    case SaveStatus::CannotWrite:
        return QCoreApplication::translate(
            "VaultController", "Your change could not be saved: the folder cannot be written to, or the disk is full. "
                               "The vault on disk is unchanged.");
    case SaveStatus::VerifyFailed:
    case SaveStatus::CannotReplace:
    case SaveStatus::Interrupted:
        return QCoreApplication::translate("VaultController",
                                           "Your change could not be saved. The vault on disk is unchanged.");
    }
    return {};
}

} // namespace

VaultController::VaultController(QObject* parent) : QObject(parent)
{
    savePool.setMaxThreadCount(1);
    unlockPool.setMaxThreadCount(1);
    filePool.setMaxThreadCount(1);

    // WebDAV (D-057): synced every 10 minutes and within 5 seconds of a save.
    secrets = makeSecretStore();
    syncSoonTimer.setSingleShot(true);
    connect(&syncTimer, &QTimer::timeout, this, &VaultController::syncNow);
    connect(&syncSoonTimer, &QTimer::timeout, this, &VaultController::syncNow);
    connect(this, &VaultController::saved, this, [this]() {
        if (syncer) {
            syncSoonTimer.start(5000);
        }
    });

    SettingsFile settings;
    {
        hideSecrets = settings.value(hideSensitiveKey, defaultHideSensitive).toBool();
        subtitlesShown = settings.value(showSubtitlesKey, defaultShowSubtitles).toBool();
        websiteIconsOn = settings.value(websiteIconsKey, defaultWebsiteIcons).toBool();
        reminderEvery = std::clamp(settings.value(reminderKey, defaultReminder).toInt(), 0, reminderNever);
        lockMinutes = std::clamp(settings.value(autoLockKey, defaultAutoLockMinutes).toInt(), 0, 240);
        // Before 0.0.25, 0 minutes meant never: now the tick box says that
        // (D-040).
        idleLock = settings.value(idleLockKey, lockMinutes > 0).toBool();
        if (lockMinutes == 0) {
            lockMinutes = defaultAutoLockMinutes;
        }
        lockClosed = settings.value(lockClosedKey, false).toBool();
        lockSleep = settings.value(lockSleepKey, false).toBool();
        systemIdleLock = settings.value(systemIdleLockKey, false).toBool();
        systemMinutes =
            std::clamp(settings.value(systemIdleMinutesKey, defaultSystemIdleMinutes).toInt(), 1, 240);
        clearSeconds = std::clamp(settings.value(clipboardKey, defaultClipboardSeconds).toInt(), 0, 600);
        keepRunning = settings.value(keepInTrayKey, defaultKeepInTray).toBool();
        minimizedAtStart = settings.value(startMinimizedKey, defaultStartMinimized).toBool();
        escExits = settings.value(escapeExitsKey, defaultEscapeExits).toBool();
    }

    // Quitting gives the fingerprint reader back (D-042).
    if (QCoreApplication::instance()) {
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
            stopQuickUnlock();
            cancelQuickUnlockSetup();
        });
    }

    // What the system says about sleep and inactivity (D-040); whether it
    // tells the inactive time is asked once at the start.
    activity = makeSystemActivity();
    connectSystemActivity();
    activity->askIdleTime();
    // The wait after wrong master passwords counts down each second (D-041).
    waitTimer.setInterval(1000);
    connect(&waitTimer, &QTimer::timeout, this, &VaultController::updateWait);
    systemIdleTimer.setInterval(30 * 1000);
    connect(&systemIdleTimer, &QTimer::timeout, this, &VaultController::checkSystemIdle);

    // Whether a reader is there is asked off the window's thread (D-038).
    quick = makeQuickUnlock();
    connectQuickUnlock();
    {
        QPointer<VaultController> self(this);
        const QuickUnlock* asked = quick.get(); // only compared, never used there
        (void)QtConcurrent::run([self, asked]() {
            // An object of its own, so nothing it uses can go away meanwhile.
            const bool there = makeQuickUnlock()->available();
            QMetaObject::invokeMethod(
                qApp,
                [self, asked, there]() {
                    if (self && self->quick.get() == asked && self->quickAvailable != there) {
                        self->quickAvailable = there;
                        emit self->quickUnlockChanged();
                    }
                },
                Qt::QueuedConnection);
        });
    }

    autoLockTimer.setSingleShot(true);
    connect(&autoLockTimer, &QTimer::timeout, this, &VaultController::lock);
    clipboardTimer.setSingleShot(true);
    connect(&clipboardTimer, &QTimer::timeout, this, [this]() {
        if (QClipboard* clipboard = QGuiApplication::clipboard();
            clipboard && clipboard->text() == copiedText) {
            clipboard->clear();
        }
        copiedText.clear();
    });
    messageTimer.setSingleShot(true);
    messageTimer.setInterval(5000);
    connect(&messageTimer, &QTimer::timeout, this, [this]() {
        if (!trouble) {
            say(QString(), false);
        }
    });
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(this);
    }
    // The system says when the vault file or its folder changed (D-032).
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, &VaultController::checkForChangeElsewhere);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &VaultController::checkForChangeElsewhere);

    VaultsFile vaults;
    const QString last = vaults.value(lastVaultKey).toString();
    vaultList = vaults.value(vaultListKey).toStringList();
    if (vaultList.isEmpty() && !last.isEmpty()) {
        // Before 0.0.15 there was one vault: it is the primary (D-030).
        vaultList = {last};
        vaults.setValue(vaultListKey, vaultList);
    }
    if (!last.isEmpty() && QFileInfo::exists(last)) {
        setPath(last);
        currentState = State::Locked;
    }
}

VaultController::~VaultController()
{
    savePool.waitForDone();
    unlockPool.waitForDone();
    filePool.waitForDone();
    forgetUnlockedVault();
    forgetPrimary();
}

QString VaultController::version() const
{
    return programVersion();
}

QString VaultController::vaultName() const
{
    return QFileInfo(path).completeBaseName();
}

void VaultController::setSearchText(const QString& text)
{
    if (search != text) {
        search = text;
        listModel.setSearchText(text);
        emit searchTextChanged();
    }
}

void VaultController::setBool(bool& value, bool on, const QString& settingName)
{
    if (value != on) {
        value = on;
        SettingsFile().setValue(settingName, on);
        restartAutoLock();
        emit autoLockMinutesChanged();
    }
}

void VaultController::setIdleLockOn(bool on)
{
    setBool(idleLock, on, idleLockKey);
}

void VaultController::setLockWhenClosed(bool on)
{
    setBool(lockClosed, on, lockClosedKey);
}

void VaultController::setLockOnSleep(bool on)
{
    setBool(lockSleep, on, lockSleepKey);
}

void VaultController::setSystemIdleLockOn(bool on)
{
    setBool(systemIdleLock, on, systemIdleLockKey);
}

void VaultController::setSystemIdleMinutes(int minutes)
{
    minutes = std::clamp(minutes, 1, 240);
    if (systemMinutes != minutes) {
        systemMinutes = minutes;
        SettingsFile().setValue(systemIdleMinutesKey, minutes);
        emit autoLockMinutesChanged();
    }
}

void VaultController::mainWindowClosed()
{
    if (lockClosed && currentState == State::Unlocked) {
        lock();
    }
}

void VaultController::checkSystemIdle()
{
    if (activity) {
        activity->askIdleTime();
    }
}

void VaultController::connectSystemActivity()
{
    connect(activity.get(), &SystemActivity::sleeping, this, [this]() {
        if (lockSleep && currentState == State::Unlocked) {
            lock();
        }
    });
    connect(activity.get(), &SystemActivity::idleTime, this, [this](qint64 seconds) {
        if (systemIdleKnown != (seconds >= 0)) {
            systemIdleKnown = seconds >= 0;
            emit systemIdleAvailableChanged();
        }
        const qint64 sinceUnlock = (QDateTime::currentMSecsSinceEpoch() - unlockedAtMs) / 1000;
        if (systemIdleLock && std::min(seconds, sinceUnlock) >= qint64(systemMinutes) * 60 &&
            currentState == State::Unlocked) {
            lock();
        }
    });
}

void VaultController::setSystemActivityForTests(std::unique_ptr<SystemActivity> standIn)
{
    activity = std::move(standIn);
    connectSystemActivity();
    activity->askIdleTime();
}

void VaultController::setUnlockedSecondsAgoForTests(qint64 seconds)
{
    unlockedAtMs = QDateTime::currentMSecsSinceEpoch() - seconds * 1000;
}

void VaultController::setAutoLockMinutes(int minutes)
{
    minutes = std::clamp(minutes, 1, 240);
    if (lockMinutes != minutes) {
        lockMinutes = minutes;
        SettingsFile().setValue(autoLockKey, minutes);
        restartAutoLock();
        emit autoLockMinutesChanged();
    }
}

void VaultController::setClipboardClearSeconds(int seconds)
{
    seconds = std::clamp(seconds, 0, 600);
    if (clearSeconds != seconds) {
        clearSeconds = seconds;
        SettingsFile().setValue(clipboardKey, seconds);
        emit clipboardClearSecondsChanged();
    }
}

void VaultController::setKeepInTray(bool keep)
{
    if (keepRunning != keep) {
        keepRunning = keep;
        SettingsFile().setValue(keepInTrayKey, keep);
        emit keepInTrayChanged();
    }
}

bool VaultController::startAtStartup() const
{
    return autostart::isEnabled();
}

void VaultController::setStartAtStartup(bool start)
{
    if (start == autostart::isEnabled()) {
        return;
    }
    if (!autostart::setEnabled(start, QCoreApplication::applicationFilePath())) {
        say(tr("The setting to start at startup could not be saved."), true);
    }
    emit startAtStartupChanged();
}

void VaultController::setStartMinimized(bool minimized)
{
    if (minimizedAtStart != minimized) {
        minimizedAtStart = minimized;
        SettingsFile().setValue(startMinimizedKey, minimized);
        emit startMinimizedChanged();
    }
}

void VaultController::setEscapeExits(bool exits)
{
    if (escExits != exits) {
        escExits = exits;
        SettingsFile().setValue(escapeExitsKey, exits);
        emit escapeExitsChanged();
    }
}

void VaultController::setTrayAvailable(bool available)
{
    if (trayExists != available) {
        trayExists = available;
        emit trayAvailableChanged();
    }
}

void VaultController::createVault(const QUrl& file, const QString& password, const QString& repeated,
                                  const QUrl& keyfile, bool savePassword)
{
    const QString newPath = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QString keyfileFile = keyfile.isLocalFile() ? keyfile.toLocalFile() : keyfile.toString();
    if (currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        say(tr("Wait until your last change is saved, then try again."), true);
        return;
    }
    const bool saveInPrimary = savePassword && canSaveInPrimary();
    const State before = currentState;
    if (newPath.isEmpty()) {
        say(tr("Choose where to keep the new vault."), true);
        return;
    }
    if (password.isEmpty()) {
        say(tr("Choose a master password."), true);
        return;
    }
    if (password != repeated) {
        say(tr("The two passwords are not the same."), true);
        return;
    }
    if (!canCreateAt(file)) {
        return;
    }

    setState(State::Busy);
    say(tr("Creating the vault…"), false);
    QByteArray passwordBytes = password.toUtf8();
    const crypto::KdfParameters parameters = kdfParameters;
    const quint64 attempt = ++unlockAttempt;
    QPointer<VaultController> self(this);
    const QString savedPassword = saveInPrimary ? password : QString();
    (void)QtConcurrent::run(&unlockPool, [self, newPath, passwordBytes, parameters, attempt, keyfileFile, before,
                                          savedPassword]() mutable {
        std::optional<QByteArray> fingerprint = QByteArray();
        if (!keyfileFile.isEmpty()) {
            fingerprint = keyfileFingerprint(keyfileFile);
        }
        VaultKey newKey;
        SaveStatus status = SaveStatus::CannotWrite;
        QByteArray written;
        if (fingerprint) {
            newKey = makeVaultKey(passwordBytes, parameters, *fingerprint);
            status = newKey.isValid() ? saveVaultFile(newPath, contentsToJson({}), newKey, {}, &written)
                                      : SaveStatus::CannotWrite;
        }
        crypto::wipe(passwordBytes);
        QMetaObject::invokeMethod(
            qApp,
            [self, newPath, newKey, status, attempt, keyfileFile, before, savedPassword, fingerprint,
             written]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    newKey.wipe();
                    return;
                }
                if (!fingerprint) {
                    self->setState(before == State::Unlocked ? State::Unlocked
                                   : self->path.isEmpty()    ? State::NoVault
                                                             : State::Locked);
                    self->say(tr("The keyfile could not be read. The vault was not created."), true);
                    return;
                }
                if (status != SaveStatus::Saved) {
                    newKey.wipe();
                    self->setState(before == State::Unlocked ? State::Unlocked
                                   : self->path.isEmpty()    ? State::NoVault
                                                             : State::Locked);
                    self->say(tr("The new vault could not be created: the folder cannot be written to, or the "
                                 "disk is full."),
                              true);
                    return;
                }
                self->finishCreating(newPath, newKey, *fingerprint, written, keyfileFile, before, savedPassword);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::finishCreating(const QString& newPath, VaultKey newKey, const QByteArray& fingerprint,
                                     const QByteArray& written, const QString& keyfileFile, State before,
                                     const QString& savedPassword)
{
    addVault(newPath);
    rememberKeyfile(newPath, keyfileFile);
    stamps->set(newPath, written);
    if (before == State::Unlocked && !savedPassword.isEmpty() && path == primaryVault()) {
        // Its password is kept in the primary as a Password item
        // that names the vault's file (D-030); the new vault is
        // shown once that is on disk, so a failure is said on the
        // primary, where it can be tried again.
        const auto passwordTemplate = findTemplate(QStringLiteral("password"), contents);
        Item saved = newItem(*passwordTemplate);
        saved.title = tr("%1 vault").arg(QFileInfo(newPath).completeBaseName());
        saved.fields.insert(1, makeField(QStringLiteral("vaultfile"), {}, FieldKind::Text, newPath));
        saved.fields[0].value = savedPassword;
        giveFieldsIds(saved);
        contents.items.append(saved);
        contentsChanged();
        createdVault = CreatedVault{newPath, newKey, fingerprint};
        setState(State::Unlocked);
        saveInBackground(QString());
        rememberPrimary();
        return;
    }
    if (before == State::Unlocked && path == primaryVault()) {
        rememberPrimary();
    }
    showCreated(CreatedVault{newPath, newKey, fingerprint});
}

bool VaultController::canCreateAt(const QUrl& file)
{
    const QString place = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (QFileInfo::exists(place)) {
        // Never over an existing file: it may be another vault (OR-15).
        say(tr("A file with that name already exists. Choose another name."), true);
        return false;
    }
    return true;
}

void VaultController::chooseVault(const QUrl& file)
{
    const QString chosen = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (chosen.isEmpty()) {
        return;
    }
    if (currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        say(tr("Wait until your last change is saved, then try again."), true);
        return;
    }
    if (currentState == State::Unlocked && path == primaryVault()) {
        rememberPrimary();
    }
    forgetUnlockedVault();
    ++unlockAttempt; // a create or unlock still running no longer counts
    addVault(chosen);
    setPath(chosen);
    say(QString(), false);
    setState(State::Locked);
}

int VaultController::waitBeforeAttempt(int attempt)
{
    // The owner's rule (D-041): 1 to 3 at once, 4 and 5 after 10 seconds, 6 to
    // 10 after a minute, then 10 minutes; before 26, 51, 76… once an hour.
    if (attempt <= 3) {
        return 0;
    }
    if (attempt > 25 && (attempt - 1) % 25 == 0) {
        return 3600;
    }
    if (attempt <= 5) {
        return 10;
    }
    if (attempt <= 10) {
        return 60;
    }
    return 600;
}

qint64 VaultController::nowMs() const
{
    return QDateTime::currentMSecsSinceEpoch() + clockShiftMs;
}

void VaultController::moveClockForTests(qint64 seconds)
{
    clockShiftMs += seconds * 1000;
    updateWait();
}

QString VaultController::waitKey() const
{
    return wrongAttemptsGroup +
           QString::fromLatin1(QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
}

void VaultController::wrongPasswordTyped()
{
    VaultsFile settings;
    const QString keyPrefix = waitKey();
    const int wrong = settings.value(keyPrefix + QStringLiteral("/count"), 0).toInt() + 1;
    const int wait = waitBeforeAttempt(wrong + 1);
    settings.setValue(keyPrefix + QStringLiteral("/count"), wrong);
    settings.setValue(keyPrefix + QStringLiteral("/until"), nowMs() + qint64(wait) * 1000);
    settings.setValue(keyPrefix + QStringLiteral("/wait"), wait);
    updateWait();
}

void VaultController::rightPasswordGiven()
{
    VaultsFile().remove(waitKey());
    updateWait();
}

void VaultController::updateWait()
{
    int left = 0;
    if (!path.isEmpty()) {
        VaultsFile settings;
        const QString keyPrefix = waitKey();
        const qint64 until = settings.value(keyPrefix + QStringLiteral("/until"), 0).toLongLong();
        const qint64 wait = settings.value(keyPrefix + QStringLiteral("/wait"), 0).toLongLong();
        // A clock set back never makes the wait longer than the wait itself.
        const qint64 remainingMs = std::min(until - nowMs(), wait * 1000);
        left = remainingMs > 0 ? static_cast<int>((remainingMs + 999) / 1000) : 0;
    }
    if (left > 0) {
        if (!waitTimer.isActive()) {
            waitTimer.start();
        }
    } else {
        waitTimer.stop();
    }
    if (waitSeconds != left) {
        waitSeconds = left;
        emit unlockWaitChanged();
    }
}

void VaultController::unlock(const QString& password)
{
    if (currentState != State::Locked || path.isEmpty()) {
        return;
    }
    // Still waiting after wrong master passwords (D-041).
    updateWait();
    if (waitSeconds > 0) {
        return;
    }
    QByteArray passwordBytes = password.toUtf8();
    startUnlock(passwordBytes);
    crypto::wipe(passwordBytes);
}

void VaultController::startUnlock(const QByteArray& password, bool savedPassword)
{
    setState(State::Busy);
    say(tr("Unlocking…"), false);
    QByteArray passwordBytes = password;
    const QString vaultFile = path;
    const QString keyfileFile = keyfileOf(path);
    const quint64 attempt = ++unlockAttempt;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&unlockPool, [self, vaultFile, passwordBytes, attempt, keyfileFile,
                                          savedPassword]() mutable {
        std::optional<QByteArray> fingerprint = QByteArray();
        if (!keyfileFile.isEmpty()) {
            fingerprint = keyfileFingerprint(keyfileFile);
        }
        OpenResult result;
        if (fingerprint) {
            result = openVaultFile(vaultFile, passwordBytes, *fingerprint);
        }
        crypto::wipe(passwordBytes);
        ContentsResult read;
        if (result.status == OpenStatus::Opened) {
            read = contentsFromJson(result.contents);
            crypto::wipe(result.contents);
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, result, read, attempt, fingerprint, savedPassword]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    result.key.wipe();
                    return;
                }
                if (savedPassword && result.status == OpenStatus::WrongPasswordOrDamaged) {
                    // The copy saved in the primary is out of date: forgotten,
                    // and the person is told which password failed.
                    self->savedPasswords.remove(self->path);
                    self->setState(State::Locked);
                    self->say(tr("The password saved in “%1” for this vault no longer opens it. Type its master "
                                 "password.")
                                  .arg(QFileInfo(self->primaryVault()).completeBaseName()),
                              true);
                    return;
                }
                if (!fingerprint) {
                    self->setState(State::Locked);
                    self->say(tr("The keyfile could not be read. Choose it again below."), true);
                    return;
                }
                if (result.status != OpenStatus::Opened) {
                    // A wrong typed master password counts toward the waits
                    // (D-041); a saved one was handled above.
                    if (result.status == OpenStatus::WrongPasswordOrDamaged) {
                        self->wrongPasswordTyped();
                    }
                    if (result.status == OpenStatus::KeyfileNeeded && !self->needKeyfile) {
                        self->needKeyfile = true;
                        emit self->keyfileChanged();
                    }
                    self->setState(State::Locked);
                    self->say(openStatusText(result.status), true);
                    return;
                }
                if (read.status != ContentsStatus::Read) {
                    result.key.wipe();
                    self->setState(State::Locked);
                    self->say(read.status == ContentsStatus::NewerFormat ? openStatusText(OpenStatus::NewerFormat)
                                                                          : tr("The vault opened, but what it holds is "
                                                                               "damaged. It was not changed."),
                              true);
                    return;
                }
                self->unlockedStamp = result.stamp;
                if (!savedPassword) {
                    self->recordMasterPassword(); // typed: the reminder waits another year
                }
                self->unlocked(result.key, read.contents, *fingerprint);
                self->keepForThisVault(); // its key for the finger, when that is on (D-043)
            },
            Qt::QueuedConnection);
    });
}

void VaultController::startOpenWithKey(const VaultKey& kept)
{
    setState(State::Busy);
    say(tr("Unlocking…"), false);
    const QString vaultFile = path;
    const quint64 attempt = ++unlockAttempt;
    const QByteArray fingerprint = keyfileSecret;
    // Whether this attempt is the finger's goes with the attempt itself, so a
    // later one never takes it over.
    const bool byFinger = std::exchange(openingByFinger, false);
    QPointer<VaultController> self(this);
    // On the save thread: after any write to this vault still running.
    (void)QtConcurrent::run(&savePool, [self, vaultFile, kept, attempt, fingerprint, byFinger]() mutable {
        OpenResult result = openVaultFileWithKey(vaultFile, kept);
        ContentsResult read;
        if (result.status == OpenStatus::Opened) {
            read = contentsFromJson(result.contents);
            crypto::wipe(result.contents);
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, result, read, attempt, fingerprint, byFinger]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    result.key.wipe();
                    return;
                }
                if (byFinger && result.status == OpenStatus::KeyChanged) {
                    // The key kept for the finger no longer fits: the master
                    // password was changed on another computer (D-038).
                    result.key.wipe();
                    self->quick->remove(self->path);
                    self->setState(State::Locked);
                    emit self->quickUnlockChanged();
                    self->say(tr("The master password of this vault was changed on another computer. Type it, then "
                                 "turn %1 on again in Settings > Security.")
                                  .arg(self->quickUnlockName()),
                              true);
                    return;
                }
                if (result.status != OpenStatus::Opened || read.status != ContentsStatus::Read) {
                    // Changed elsewhere meanwhile, or gone: the person unlocks it.
                    result.key.wipe();
                    self->setState(State::Locked);
                    self->say(result.status == OpenStatus::FileMissing
                                  ? openStatusText(result.status)
                                  : tr("This vault was changed elsewhere since it was opened. Unlock it with its "
                                       "master password."),
                              true);
                    return;
                }
                self->unlockedStamp = result.stamp;
                self->unlocked(result.key, read.contents, fingerprint);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::unlocked(const VaultKey& newKey, const VaultContents& read, const QByteArray& keyfile)
{
    rightPasswordGiven(); // the count of wrong master passwords starts again (D-041)
    if (quick) {
        quick->cancel(); // a finger still asked for is no longer needed
        retrieving = false;
    }
    stamps->set(path, unlockedStamp);
    key = newKey;
    keyfileSecret = keyfile;
    contents = read;
    base = read;
    if (needKeyfile && !key.usesKeyfile) {
        needKeyfile = false;
        emit keyfileChanged();
    }
    say(QString(), false);
    takeBackUnsaved();
    contentsChanged();
    setState(State::Unlocked);
    fetchWebsiteIcons(); // now that it counts as unlocked (D-036)
    startWebDav();       // every unlock syncs a vault kept on a server (D-057)
    if (path == primaryVault()) {
        rememberPrimary();
        // After a lock, back to the vault that was shown (D-030).
        const QString back = returnTo;
        returnTo.clear();
        if (!back.isEmpty() && back != path && vaultList.contains(back)) {
            switchToVault(back);
        }
    }
}

void VaultController::lock()
{
    if (currentState != State::Unlocked) {
        return;
    }
    emit locking();
    keepUnsavedWhileLocked();
    forgetUnlockedVault();
    forgetPrimary();
    // Locking locks every vault; the unlock page shows the vault used last,
    // whose own master password opens it (the owner, 0.3.64; until then the
    // primary came back, D-030 point 5).
    say(QString(), false);
    setState(State::Locked);
}

void VaultController::closeVault()
{
    forgetUnlockedVault();
    forgetPrimary();
    returnTo.clear();
    ++unlockAttempt;
    // The window has asked the person before this (OR-15).
    if (!unsavedSealed.isEmpty()) {
        unsavedSealed.clear();
        unsavedPath.clear();
        unsavedStamp.clear();
        emit keptChangesChanged();
    }
    setPath(QString());
    VaultsFile().remove(lastVaultKey);
    say(QString(), false);
    setState(State::NoVault);
}

QVariantMap VaultController::item(const QString& id) const
{
    for (const Item& one : contents.items) {
        if (one.id.toString(QUuid::WithoutBraces) == id) {
            QVariantMap map = toMap(one, contents);
            if (const QString site = websiteIcons.value(websiteHostOf(one)); !site.isEmpty()) {
                map.insert(QStringLiteral("defaultIcon"), site);
            }
            return map;
        }
    }
    return {};
}

QVariantMap VaultController::newItemFrom(const QString& templateId) const
{
    const auto itemTemplate = findTemplate(templateId, contents);
    return itemTemplate ? toMap(newItem(*itemTemplate), contents) : QVariantMap();
}

QVariantMap VaultController::newLogin() const
{
    return newItemFrom(QStringLiteral("login"));
}

QVariantMap VaultController::newSecureNote() const
{
    return newItemFrom(QStringLiteral("note"));
}

bool VaultController::saveItem(const QVariantMap& map)
{
    if (currentState != State::Unlocked) {
        return false;
    }
    const QUuid id = QUuid::fromString(map.value(QStringLiteral("id")).toString());
    if (id.isNull()) {
        return false;
    }
    const auto existing = std::find_if(contents.items.begin(), contents.items.end(),
                                       [&id](const Item& one) { return one.id == id; });
    Item changed;
    changed.id = id;
    changed.templateId = map.value(QStringLiteral("templateId")).toString();
    changed.categoryId = map.value(QStringLiteral("categoryId")).toString();
    if (changed.templateId.isEmpty()) {
        changed.templateId = QStringLiteral("login");
    }
    if (changed.categoryId.isEmpty()) {
        const auto itemTemplate = findTemplate(changed.templateId, contents);
        changed.categoryId = itemTemplate ? itemTemplate->categoryId : QStringLiteral("misc");
    }
    changed.title = map.value(QStringLiteral("title")).toString().trimmed();
    if (changed.title.isEmpty()) {
        changed.title = changed.categoryId == QLatin1String("note")    ? tr("Untitled note")
                        : changed.categoryId == QLatin1String("login") ? tr("Untitled login")
                                                                        : tr("Untitled item");
    }
    changed.fields = fieldsFromList(map.value(QStringLiteral("fields")).toList(), true);
    changed.icon = QByteArray::fromBase64(map.value(QStringLiteral("icon")).toString().toLatin1());
    for (const QString& tag : map.value(QStringLiteral("tags")).toStringList()) {
        const QString kept = normalizedTag(tag);
        if (!kept.isEmpty() && !changed.tags.contains(kept)) {
            changed.tags.append(kept);
        }
    }
    changed.favourite = map.value(QStringLiteral("favourite")).toBool();
    changed.subtitle = map.value(QStringLiteral("subtitle")).toString();
    changed.note = map.value(QStringLiteral("note")).toString();
    changed.autoSubmit = map.value(QStringLiteral("autoSubmit")).toBool();
    changed.website = map.value(QStringLiteral("website")).toString();
    if (existing != contents.items.end()) {
        changed.trashed = existing->trashed;
        changed.archived = existing->archived;
        changed.mergeNotes = existing->mergeNotes;
    }
    // Each field keeps its earlier values: a changed value goes into its
    // history with the time it was set (D-033).
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (Field& field : changed.fields) {
        const Field* before = nullptr;
        if (existing != contents.items.end() && field.uid != 0) {
            for (const Field& old : std::as_const(existing->fields)) {
                if (old.uid == field.uid) {
                    before = &old;
                }
            }
        }
        if (before == nullptr) {
            field.changed = now;
            continue;
        }
        field.history = before->history;
        field.changed = before->changed;
        if (before->value != field.value) {
            if (!before->value.isEmpty()) {
                field.history.append(FieldChange{before->value, before->changed.isValid() ? before->changed : now});
            }
            field.changed = now;
        }
    }
    giveFieldsIds(changed);

    // The attachments: those the item had, and files added in the editor.
    QStringList usedPending;
    for (const QVariant& value : map.value(QStringLiteral("attachments")).toList()) {
        const QString attachmentId = value.toMap().value(QStringLiteral("id")).toString();
        if (const auto pending = pendingAttachments.constFind(attachmentId); pending != pendingAttachments.constEnd()) {
            changed.attachments.append(pending.value());
            usedPending.append(attachmentId);
            continue;
        }
        if (existing == contents.items.end()) {
            continue;
        }
        for (const Attachment& attachment : std::as_const(existing->attachments)) {
            if (attachment.id.toString(QUuid::WithoutBraces) == attachmentId) {
                changed.attachments.append(attachment);
            }
        }
    }
    qsizetype total = attachmentBytes();
    if (existing != contents.items.end()) {
        for (const Attachment& attachment : std::as_const(existing->attachments)) {
            total -= attachment.data.size();
        }
    }
    for (const Attachment& attachment : std::as_const(changed.attachments)) {
        total += attachment.data.size();
    }
    if (total > largestAttachmentsTotal) {
        say(tr("The attachments of this vault together may hold at most %1. Remove an attachment first.")
                .arg(sizeText(largestAttachmentsTotal)),
            true);
        return false;
    }
    for (const QString& used : std::as_const(usedPending)) {
        pendingAttachments.remove(used);
    }
    changed.modified = QDateTime::currentDateTimeUtc();

    if (existing != contents.items.end()) {
        changed.created = existing->created;
        *existing = changed;
    } else {
        changed.created = changed.modified;
        contents.items.append(changed);
    }
    contentsChanged();
    saveInBackground(id.toString(QUuid::WithoutBraces));
    return true;
}

void VaultController::deleteItem(const QString& id)
{
    if (currentState != State::Unlocked) {
        return;
    }
    const auto removed = std::remove_if(contents.items.begin(), contents.items.end(), [&id](const Item& one) {
        return one.id.toString(QUuid::WithoutBraces) == id;
    });
    if (removed == contents.items.end()) {
        return;
    }
    contents.items.erase(removed, contents.items.end());
    // Remembered, so the deletion reaches the other devices (D-054).
    contents.deletedItems.append(DeletedItem{QUuid::fromString(id), QDateTime::currentDateTimeUtc()});
    contentsChanged();
    saveInBackground(QString());
}

QVariantList VaultController::mergeNotes(const QString& itemId) const
{
    QVariantList notes;
    for (const Item& item : contents.items) {
        if (item.id.toString(QUuid::WithoutBraces) != itemId) {
            continue;
        }
        for (const MergeNote& note : item.mergeNotes) {
            QString label;
            bool hidden = false;
            if (note.fieldUid != 0) {
                for (const Field& field : item.fields) {
                    if (field.uid == note.fieldUid) {
                        label = field.isCustom() ? field.label : roleName(field.role);
                        hidden = field.isHidden();
                    }
                }
            } else if (note.property == QLatin1String("title")) {
                label = tr("Title");
            } else if (note.property == QLatin1String("subtitle")) {
                label = tr("Subtitle");
            } else if (note.property == QLatin1String("note")) {
                label = tr("Notes");
            } else if (note.property == QLatin1String("website")) {
                label = tr("Website");
            }
            notes.append(QVariantMap{
                {QStringLiteral("kind"),
                 note.kind == MergeNote::Kind::DeletedElsewhere ? QStringLiteral("deleted") : QStringLiteral("both")},
                {QStringLiteral("label"), label},
                {QStringLiteral("otherValue"), note.otherValue},
                {QStringLiteral("hidden"), hidden},
            });
        }
    }
    return notes;
}

void VaultController::useOtherValue(const QString& itemId, int index)
{
    if (currentState != State::Unlocked) {
        return;
    }
    for (Item& item : contents.items) {
        if (item.id.toString(QUuid::WithoutBraces) != itemId || index < 0 || index >= item.mergeNotes.size()) {
            continue;
        }
        const MergeNote note = item.mergeNotes.takeAt(index);
        const QDateTime now = QDateTime::currentDateTimeUtc();
        if (note.fieldUid != 0) {
            for (Field& field : item.fields) {
                if (field.uid == note.fieldUid && field.value != note.otherValue) {
                    if (!field.value.isEmpty()) {
                        field.history.append(FieldChange{field.value, field.changed.isValid() ? field.changed : now});
                    }
                    field.value = note.otherValue;
                    field.changed = now;
                }
            }
        } else if (note.property == QLatin1String("title")) {
            item.title = note.otherValue;
        } else if (note.property == QLatin1String("subtitle")) {
            item.subtitle = note.otherValue;
        } else if (note.property == QLatin1String("note")) {
            item.note = note.otherValue;
        } else if (note.property == QLatin1String("website")) {
            item.website = note.otherValue;
        }
        item.modified = now;
        contentsChanged();
        saveInBackground(itemId);
        return;
    }
}

void VaultController::dismissMergeNote(const QString& itemId, int index)
{
    if (currentState != State::Unlocked) {
        return;
    }
    for (Item& item : contents.items) {
        if (item.id.toString(QUuid::WithoutBraces) == itemId && index >= 0 && index < item.mergeNotes.size()) {
            item.mergeNotes.removeAt(index);
            contentsChanged();
            saveInBackground(itemId);
            return;
        }
    }
}

void VaultController::moveToTrash(const QString& id, bool trashed)
{
    setItemPlace(id, &Item::trashed, trashed);
}

void VaultController::archiveItem(const QString& id, bool archived)
{
    setItemPlace(id, &Item::archived, archived);
}

void VaultController::setItemPlace(const QString& id, bool Item::*place, bool on)
{
    if (currentState != State::Unlocked) {
        return;
    }
    for (Item& one : contents.items) {
        if (one.id.toString(QUuid::WithoutBraces) == id && one.*place != on) {
            one.*place = on;
            one.modified = QDateTime::currentDateTimeUtc();
            contentsChanged();
            saveInBackground(QString());
            return;
        }
    }
}

void VaultController::setHideSensitive(bool hide)
{
    if (hideSecrets != hide) {
        hideSecrets = hide;
        SettingsFile().setValue(hideSensitiveKey, hide);
        emit hideSensitiveChanged();
    }
}

void VaultController::readImport(const QUrl& file)
{
    const QString source = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (currentState != State::Unlocked || source.isEmpty()) {
        return;
    }
    pendingImport.reset();
    const VaultContents snapshot = contents;
    const quint64 session = vaultSession;
    QPointer<VaultController> self(this);
    say(tr("Reading the export…"), false);
    (void)QtConcurrent::run(&filePool, [self, source, snapshot, session]() {
        ImportResult result;
        QFile input(source);
        if (!input.open(QIODevice::ReadOnly)) {
            result.status = ImportStatus::NotTheFormat;
        } else if (input.size() > largestImportBytes) {
            result.status = ImportStatus::TooLarge;
        } else {
            QByteArray json = input.read(largestImportBytes + 1);
            result = readReferenceExport(json, snapshot);
            crypto::wipe(json);
        }
        const bool readable = input.isOpen();
        QMetaObject::invokeMethod(
            qApp,
            [self, result, session, readable]() mutable {
                if (!self || session != self->vaultSession || self->currentState != State::Unlocked) {
                    return;
                }
                QVariantMap summary{{QStringLiteral("ok"), result.status == ImportStatus::Read}};
                if (result.status != ImportStatus::Read) {
                    const QString why = !readable ? tr("The file could not be read.")
                                        : result.status == ImportStatus::TooLarge
                                            ? tr("The file is too large to be an export.")
                                        : result.status == ImportStatus::NotTheFormat
                                            ? tr("This file is not an export of %1.").arg(referenceProgramName())
                                            : tr("The export is damaged. Nothing was imported.");
                    summary.insert(QStringLiteral("message"), why);
                    self->say(why, true);
                    emit self->importRead(summary);
                    return;
                }
                summary.insert(QStringLiteral("items"), static_cast<int>(result.items.size()));
                summary.insert(QStringLiteral("categories"), static_cast<int>(result.categories.size()));
                summary.insert(QStringLiteral("templates"), static_cast<int>(result.templates.size()));
                summary.insert(QStringLiteral("attachments"), result.attachmentCount);
                self->pendingImport = std::move(result);
                self->say(QString(), false);
                emit self->importRead(summary);
            },
            Qt::QueuedConnection);
    });
}

bool VaultController::confirmImport()
{
    if (currentState != State::Unlocked || !pendingImport) {
        if (currentState == State::Unlocked) {
            say(tr("Nothing was imported: choose the export again."), true);
        }
        return false;
    }
    ImportResult result = std::move(*pendingImport);
    pendingImport.reset();
    qsizetype added = 0;
    for (const Item& one : std::as_const(result.items)) {
        for (const Attachment& attachment : one.attachments) {
            added += attachment.data.size();
        }
    }
    if (attachmentBytes() + added > largestAttachmentsTotal) {
        say(tr("The attachments of this vault together may hold at most %1. Nothing was imported.")
                .arg(sizeText(largestAttachmentsTotal)),
            true);
        return false;
    }
    for (const Category& category : std::as_const(result.categories)) {
        if (std::none_of(contents.categories.cbegin(), contents.categories.cend(),
                         [&category](const Category& one) { return one.id == category.id; })) {
            contents.categories.append(category);
        }
    }
    for (const Template& one : std::as_const(result.templates)) {
        if (std::none_of(contents.templates.cbegin(), contents.templates.cend(),
                         [&one](const Template& other) { return other.id == one.id; })) {
            contents.templates.append(one);
        }
    }
    for (Item item : std::as_const(result.items)) {
        // Everything is imported (the owner): an item whose id the vault
        // already has gets a new one, never written over another.
        if (std::any_of(contents.items.cbegin(), contents.items.cend(),
                        [&item](const Item& one) { return one.id == item.id; })) {
            item.id = QUuid::createUuid();
        }
        contents.items.append(item);
    }
    contentsChanged();
    saveInBackground(QString());
    say(tr("%n item(s) imported.", nullptr, static_cast<int>(result.items.size())), false);
    return true;
}

void VaultController::cancelImport()
{
    pendingImport.reset();
}

QString VaultController::referenceName() const
{
    return referenceProgramName();
}

void VaultController::setUseWebsiteIcons(bool use)
{
    if (websiteIconsOn == use) {
        return;
    }
    websiteIconsOn = use;
    SettingsFile().setValue(websiteIconsKey, use);
    if (use) {
        fetchWebsiteIcons();
    } else {
        // Off: every item shows its own or its category's icon again.
        websiteIcons.clear();
        iconsAsked.clear();
        listModel.setWebsiteIcons({});
        ++contentsRevision;
        emit revisionChanged();
    }
    emit useWebsiteIconsChanged();
}

void VaultController::fetchWebsiteIcons()
{
    if (!websiteIconsOn || currentState != State::Unlocked) {
        return;
    }
    if (!iconAddress) {
        // The website itself, never a service in between (Q36): only it
        // learns that its icon was asked for.
        iconAddress = [](const QString& host) { return QUrl(QStringLiteral("https://%1/favicon.ico").arg(host)); };
    }
    for (const Item& one : std::as_const(contents.items)) {
        const QString host = websiteHostOf(one);
        // Not for an item in the Trash: its website is not asked (D-036).
        if (one.trashed || !one.icon.isEmpty() || host.isEmpty() || iconsAsked.contains(host)) {
            continue;
        }
        iconsAsked.insert(host);
        QNetworkRequest request(iconAddress(host));
        request.setTransferTimeout(10000);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setMaximumRedirectsAllowed(5);
        QNetworkReply* reply = iconNetwork.get(request);
        // Broken off as soon as it grows past what an icon may be.
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
            if (received > largestWebsiteIconBytes) {
                reply->abort();
            }
        });
        const quint64 session = vaultSession;
        connect(reply, &QNetworkReply::finished, this, [this, reply, host, session]() {
            reply->deleteLater();
            if (session != vaultSession || !websiteIconsOn || reply->error() != QNetworkReply::NoError) {
                return;
            }
            const QByteArray bytes = reply->read(largestWebsiteIconBytes + 1);
            QImage image;
            if (bytes.size() > largestWebsiteIconBytes || !image.loadFromData(bytes) || image.isNull()) {
                return; // no icon: the category's stays
            }
            QByteArray png;
            QBuffer buffer(&png);
            buffer.open(QIODevice::WriteOnly);
            image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "PNG");
            websiteIcons.insert(host, QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64()));
            listModel.setWebsiteIcons(websiteIcons);
            ++contentsRevision;
            emit revisionChanged();
        });
    }
}

void VaultController::connectQuickUnlock()
{
    connect(quick.get(), &QuickUnlock::fingerReady, this, [this]() {
        if (settingUp) {
            tellSetup(quick->touchPrompt(), false);
        } else if (retrieving && currentState == State::Locked) {
            say(quick->touchPrompt(), false);
        }
    });
    connect(quick.get(), &QuickUnlock::notRecognized, this, [this]() {
        if (settingUp) {
            tellSetup(tr("Not recognized. Try again."), true);
        } else if (retrieving && currentState == State::Locked) {
            say(tr("Not recognized. Try again."), true);
        }
    });
    connect(quick.get(), &QuickUnlock::stored, this, [this](QuickUnlock::Result result) {
        const bool ok = result == QuickUnlock::Result::Done;
        if (std::exchange(keepingQuietly, false)) {
            emit quickUnlockChanged(); // kept at an unlock: nothing to say
            return;
        }
        if (std::exchange(replacingQuickKey, false)) {
            if (!ok) {
                quick->remove(path);
                say(tr("The master password was changed, but %1 could not keep it. Turn %1 on again in Settings > "
                       "Security.")
                        .arg(quickUnlockName()),
                    true);
            }
            emit quickUnlockChanged();
            return;
        }
        // The outcome is told in the settings window, where it was asked
        // (D-042); a Cancel is no failure.
        settingUp = false;
        switch (result) {
        case QuickUnlock::Result::Done:
            recordMasterPasswordIfNone();
            SettingsFile().setValue(QStringLiteral("quickUnlock/enabled"), true);
            tellSetup(QString(), false); // the ticked box says it
            break;
        case QuickUnlock::Result::Cancelled: tellSetup(QString(), false); break;
        case QuickUnlock::Result::Failed: tellSetup(tr("That finger was not recognized."), true); break;
        case QuickUnlock::Result::Unavailable:
            tellSetup(quick->problem().isEmpty() ? tr("The %1 is not available now.").arg(quickUnlockName())
                                                 : quick->problem(),
                      true);
            break;
        case QuickUnlock::Result::NotKept:
            tellSetup(quick->notKeptText(), true);
            break;
        default:
            tellSetup(tr("%1 could not be turned on for this vault.").arg(sentenceStart(quickUnlockName())), true);
            break;
        }
        emit quickUnlockChanged();
    });
    connect(quick.get(), &QuickUnlock::removeFailed, this, [this](const QString& where) {
        say(tr("The key could not be removed from the desktop keyring. Remove “%1” there by hand.").arg(where), true);
    });
    connect(quick.get(), &QuickUnlock::retrieved, this, [this](QuickUnlock::Result result, const QByteArray& secret) {
        fingerLog(QStringLiteral("unlock: answer %1").arg(int(result)));
        retrieving = false;
        if (currentState != State::Locked) {
            return;
        }
        const auto kept = result == QuickUnlock::Result::Done ? quickSecretFrom(secret) : std::nullopt;
        if (kept) {
            keyfileSecret = kept->second;
            openingByFinger = true;
            startOpenWithKey(kept->first);
            return;
        }
        switch (result) {
        case QuickUnlock::Result::Cancelled: say(QString(), false); break;
        case QuickUnlock::Result::Failed: say(tr("That finger was not recognized. Try again, or use the master password."), true); break;
        case QuickUnlock::Result::Unavailable:
            // The step that failed, in plain words (D-044).
            say(quick->problem().isEmpty()
                    ? tr("%1 is not available now. Use the master password.").arg(sentenceStart(quickUnlockName()))
                    : tr("%1 Use the master password.").arg(quick->problem()),
                true);
            break;
        default:
            // Its key is gone (fingerprints changed, or the keyring emptied).
            quick->remove(path);
            emit quickUnlockChanged();
            say(tr("%1 can no longer open this vault. Use the master password, then turn it on again in Settings > "
                   "Security.")
                    .arg(sentenceStart(quickUnlockName())),
                true);
            break;
        }
    });
}

void VaultController::setQuickUnlockForTests(std::unique_ptr<QuickUnlock> standIn)
{
    quick = std::move(standIn);
    connectQuickUnlock();
    quickAvailable = quick->available();
    emit quickUnlockChanged();
}

void VaultController::setQuickUnlock(bool on)
{
    if (!quick || path.isEmpty()) {
        return;
    }
    if (!on) {
        // Off for all vaults: every kept key removed (D-043).
        SettingsFile().setValue(QStringLiteral("quickUnlock/enabled"), false);
        for (const QString& vault : VaultsFile().value(QStringLiteral("quickUnlock/vaults")).toStringList()) {
            quick->remove(vault);
        }
        emit quickUnlockChanged();
        return;
    }
    // One finger asked at a time: the switch waits for it (D-042).
    if (currentState == State::Unlocked && quickAvailable && !settingUp) {
        settingUp = true;
        tellSetup(QString(), false);
        quick->store(path, quickSecretOf(key, keyfileSecret));
    }
}

bool VaultController::quickUnlockEnabled() const
{
    // A setting of the settings window: off unless ticked (D-026, D-043).
    return SettingsFile().value(QStringLiteral("quickUnlock/enabled"), false).toBool();
}

void VaultController::keepForThisVault()
{
    if (quick && quickAvailable && quickUnlockEnabled() && !quick->has(path) && !settingUp && !replacingQuickKey &&
        currentState == State::Unlocked) {
        keepingQuietly = true;
        quick->replace(path, quickSecretOf(key, keyfileSecret));
    }
}

void VaultController::tellSetup(const QString& text, bool isTrouble)
{
    setupMessage = text;
    setupTrouble = isTrouble;
    emit quickUnlockChanged();
}

void VaultController::cancelQuickUnlockSetup()
{
    if (settingUp && quick) {
        quick->cancel(); // answered as Cancelled, which ends the waiting
    }
}

void VaultController::stopQuickUnlock()
{
    if (retrieving && quick) {
        quick->cancel();
        retrieving = false;
        if (currentState == State::Locked) {
            say(QString(), false);
        }
    }
}

void VaultController::unlockWithQuickUnlock()
{
    // Asked once while the question is open: the reader stays held until it
    // is settled (the owner, D-042).
    if (currentState != State::Locked || !quickUnlockOn() || retrieving) {
        fingerLog(QStringLiteral("unlock: not asked (locked: %1, on: %2, already asking: %3)")
                      .arg(currentState == State::Locked ? QStringLiteral("yes") : QStringLiteral("no"),
                           quickUnlockOn() ? QStringLiteral("yes") : QStringLiteral("no"),
                           retrieving ? QStringLiteral("yes") : QStringLiteral("no")));
        return;
    }
    retrieving = true;
    say(tr("Waiting for your %1…").arg(quickUnlockName()), false);
    quick->retrieve(path);
}

void VaultController::recordMasterPassword()
{
    VaultsFile settings;
    QVariantMap times = settings.value(lastPasswordKey).toMap();
    times.insert(path, QDateTime::currentDateTimeUtc());
    settings.setValue(lastPasswordKey, times);
    emit quickUnlockChanged();
}

void VaultController::recordMasterPasswordIfNone()
{
    if (!VaultsFile().value(lastPasswordKey).toMap().contains(path)) {
        recordMasterPassword();
    }
}

bool VaultController::reminderDue() const
{
    if (reminderEvery >= reminderNever || !quickUnlockOn()) {
        return false;
    }
    const QDateTime last = VaultsFile().value(lastPasswordKey).toMap().value(path).toDateTime();
    if (!last.isValid()) {
        return false;
    }
    QDateTime due = last;
    switch (reminderEvery) {
    case 0: due = last.addDays(1); break;
    case 1: due = last.addMonths(1); break;
    case 2: due = last.addMonths(3); break;
    default: due = last.addYears(1); break;
    }
    return QDateTime::currentDateTimeUtc() >= due;
}

void VaultController::setMasterPasswordReminder(int every)
{
    every = std::clamp(every, 0, reminderNever);
    if (reminderEvery != every) {
        reminderEvery = every;
        SettingsFile().setValue(reminderKey, every);
        emit quickUnlockChanged();
    }
}

void VaultController::dismissReminder()
{
    recordMasterPassword();
    unlockWithQuickUnlock();
}

bool VaultController::touchScreen() const
{
    const auto devices = QInputDevice::devices();
    return std::any_of(devices.cbegin(), devices.cend(), [](const QInputDevice* device) {
        return device->type() == QInputDevice::DeviceType::TouchScreen;
    });
}

void VaultController::setShowSubtitles(bool show)
{
    if (subtitlesShown != show) {
        subtitlesShown = show;
        SettingsFile().setValue(showSubtitlesKey, show);
        emit showSubtitlesChanged();
    }
}

bool VaultController::viewOption(const QString& name, bool fallback) const
{
    return SettingsFile().value(QStringLiteral("view/") + name, fallback).toBool();
}

void VaultController::setViewOption(const QString& name, bool on)
{
    SettingsFile settings;
    const QString settingName = QStringLiteral("view/") + name;
    if (!settings.contains(settingName) || settings.value(settingName).toBool() != on) {
        settings.setValue(settingName, on);
        emit viewChanged();
    }
}

void VaultController::setFilter(const QString& filter)
{
    if (chosenFilter != filter) {
        chosenFilter = filter;
        listModel.setFilter(filter);
        emit filterChanged();
    }
}

QByteArray VaultController::givenCategoryIcon(const QString& id) const
{
    if (const Category* changed = builtInCategoryChange(id, contents)) {
        return changed->icon;
    }
    for (const Category& own : contents.categories) {
        if (own.id == id) {
            return own.icon;
        }
    }
    return {};
}

QVariantList VaultController::categories() const
{
    QStringList ids = builtInCategoryIds();
    for (const Category& category : contents.categories) {
        ids.append(category.id);
    }
    QVariantList list;
    for (const QString& id : std::as_const(ids)) {
        const auto count = std::count_if(contents.items.cbegin(), contents.items.cend(),
                                         [&id](const Item& one) { return one.categoryId == id; });
        list.append(QVariantMap{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), categoryName(id, contents)},
            {QStringLiteral("icon"), categoryIconSource(id, contents)},
            // For the Edit dialog (D-056): the icon the person gave it, PNG in
            // base64 or "", and the program's own.
            {QStringLiteral("ownIcon"), QString::fromLatin1(givenCategoryIcon(id).toBase64())},
            {QStringLiteral("defaultIcon"), categoryIconSource(id, VaultContents{})},
            {QStringLiteral("builtIn"), isBuiltInCategory(id)},
            {QStringLiteral("hidden"), contents.hiddenCategories.contains(id)},
            {QStringLiteral("count"), static_cast<int>(count)},
        });
    }
    return list;
}

QVariantList VaultController::templates() const
{
    QVariantList list;
    for (const Template& one : builtInTemplates()) {
        list.append(templateToMap(one, contents));
    }
    for (const Template& one : contents.templates) {
        list.append(templateToMap(one, contents));
    }
    return list;
}

QString VaultController::saveCategory(const QString& id, const QString& name)
{
    if (currentState != State::Unlocked || isBuiltInCategory(id)) {
        return {};
    }
    const QString kept = name.simplified();
    if (kept.isEmpty()) {
        say(tr("Give the category a name."), true);
        return {};
    }
    for (const QString& other : builtInCategoryIds()) {
        if (categoryName(other, contents).compare(kept, Qt::CaseInsensitive) == 0) {
            say(tr("There is already a category with that name."), true);
            return {};
        }
    }
    for (const Category& other : std::as_const(contents.categories)) {
        if (other.id != id && other.name.compare(kept, Qt::CaseInsensitive) == 0) {
            say(tr("There is already a category with that name."), true);
            return {};
        }
    }
    QString keptId = id;
    const auto found = std::find_if(contents.categories.begin(), contents.categories.end(),
                                    [&id](const Category& one) { return one.id == id; });
    if (id.isEmpty() || found == contents.categories.end()) {
        keptId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        contents.categories.append(Category{keptId, kept, {}});
    } else {
        found->name = kept;
    }
    contentsChanged();
    saveInBackground(QString());
    return keptId;
}

bool VaultController::deleteCategory(const QString& id)
{
    if (currentState != State::Unlocked || isBuiltInCategory(id)) {
        return false;
    }
    const bool usedByItem = std::any_of(contents.items.cbegin(), contents.items.cend(),
                                        [&id](const Item& one) { return one.categoryId == id; });
    const bool usedByTemplate = std::any_of(contents.templates.cbegin(), contents.templates.cend(),
                                            [&id](const Template& one) { return one.categoryId == id; });
    if (usedByItem || usedByTemplate) {
        say(tr("This category still holds items or templates. Move or delete them first."), true);
        return false;
    }
    const auto removed = std::remove_if(contents.categories.begin(), contents.categories.end(),
                                        [&id](const Category& one) { return one.id == id; });
    if (removed == contents.categories.end()) {
        return false;
    }
    contents.categories.erase(removed, contents.categories.end());
    contents.hiddenCategories.removeAll(id);
    if (chosenFilter == QLatin1String("category:") + id) {
        setFilter(QString());
    }
    contentsChanged();
    saveInBackground(QString());
    return true;
}

void VaultController::setCategoryHidden(const QString& id, bool hidden)
{
    if (currentState != State::Unlocked || contents.hiddenCategories.contains(id) == hidden) {
        return;
    }
    if (hidden) {
        contents.hiddenCategories.append(id);
        if (chosenFilter == QLatin1String("category:") + id) {
            setFilter(QString());
        }
    } else {
        contents.hiddenCategories.removeAll(id);
    }
    contentsChanged();
    saveInBackground(QString());
}

bool VaultController::editCategory(const QString& id, const QString& name, const QString& icon, bool shown)
{
    const bool builtIn = isBuiltInCategory(id);
    const auto own = std::find_if(contents.categories.begin(), contents.categories.end(),
                                  [&id](const Category& one) { return one.id == id; });
    if (currentState != State::Unlocked || (!builtIn && own == contents.categories.end())) {
        return false;
    }
    const QString kept = name.simplified();
    if (kept.isEmpty()) {
        say(tr("Give the category a name."), true);
        return false;
    }
    for (const QString& other : builtInCategoryIds()) {
        if (other != id && categoryName(other, contents).compare(kept, Qt::CaseInsensitive) == 0) {
            say(tr("There is already a category with that name."), true);
            return false;
        }
    }
    for (const Category& other : std::as_const(contents.categories)) {
        if (other.id != id && other.name.compare(kept, Qt::CaseInsensitive) == 0) {
            say(tr("There is already a category with that name."), true);
            return false;
        }
    }
    const QByteArray png = QByteArray::fromBase64(icon.toLatin1());
    if (builtIn) {
        // Kept only as far as it differs from the program's own: its name
        // left as it is goes on following the translation (D-056).
        Category changed{id, {}, png};
        if (kept != categoryName(id, VaultContents{})) {
            changed.name = kept;
        }
        contents.builtInCategoryChanges.removeIf([&id](const Category& one) { return one.id == id; });
        if (!changed.name.isEmpty() || !changed.icon.isEmpty()) {
            contents.builtInCategoryChanges.append(changed);
        }
    } else {
        own->name = kept;
        own->icon = png;
    }
    contents.hiddenCategories.removeAll(id);
    if (!shown) {
        contents.hiddenCategories.append(id);
        if (chosenFilter == QLatin1String("category:") + id) {
            setFilter(QString());
        }
    }
    contentsChanged();
    saveInBackground(QString());
    return true;
}

void VaultController::resetCategory(const QString& id)
{
    if (currentState != State::Unlocked || !isBuiltInCategory(id)) {
        return;
    }
    const auto changes = contents.builtInCategoryChanges.removeIf([&id](const Category& one) { return one.id == id; });
    const auto hidden = contents.hiddenCategories.removeAll(id);
    if (changes == 0 && hidden == 0) {
        return; // already as the program has it
    }
    contentsChanged();
    saveInBackground(QString());
}

QString VaultController::saveTemplate(const QVariantMap& map)
{
    QString id = map.value(QStringLiteral("id")).toString();
    if (currentState != State::Unlocked) {
        return {};
    }
    for (const Template& builtIn : builtInTemplates()) {
        if (builtIn.id == id) {
            return {}; // a built-in template is never changed (D-027)
        }
    }
    Template changed;
    changed.name = map.value(QStringLiteral("name")).toString().simplified();
    if (changed.name.isEmpty()) {
        say(tr("Give the template a name."), true);
        return {};
    }
    changed.categoryId = map.value(QStringLiteral("categoryId")).toString();
    const bool categoryExists =
        isBuiltInCategory(changed.categoryId) ||
        std::any_of(contents.categories.cbegin(), contents.categories.cend(),
                    [&changed](const Category& one) { return one.id == changed.categoryId; });
    if (!categoryExists) {
        changed.categoryId = QStringLiteral("misc");
    }
    changed.fields = fieldsFromList(map.value(QStringLiteral("fields")).toList(), false);
    const auto found = std::find_if(contents.templates.begin(), contents.templates.end(),
                                    [&id](const Template& one) { return one.id == id; });
    if (id.isEmpty() || found == contents.templates.end()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        changed.id = id;
        contents.templates.append(changed);
    } else {
        changed.id = id;
        *found = changed;
    }
    contentsChanged();
    saveInBackground(QString());
    return id;
}

bool VaultController::deleteTemplate(const QString& id)
{
    if (currentState != State::Unlocked) {
        return false;
    }
    // Items made from it keep their fields; only the template goes (D-027).
    const auto removed = std::remove_if(contents.templates.begin(), contents.templates.end(),
                                        [&id](const Template& one) { return one.id == id; });
    if (removed == contents.templates.end()) {
        return false;
    }
    contents.templates.erase(removed, contents.templates.end());
    contentsChanged();
    saveInBackground(QString());
    return true;
}

QVariantMap VaultController::copyOfTemplate(const QString& id) const
{
    const auto original = findTemplate(id, contents);
    if (!original) {
        return {};
    }
    Template copy = *original;
    copy.id.clear();
    copy.builtIn = false;
    copy.name = tr("Copy of %1").arg(templateName(*original));
    return templateToMap(copy, contents);
}

QVariantList VaultController::tags() const
{
    QHash<QString, int> counts;
    for (const Item& one : contents.items) {
        QStringList itsTags;
        for (const QString& tag : one.tags) {
            // The tag and every tag above it, each counted once per item.
            QStringList parts = tag.split(QLatin1Char('/'));
            while (!parts.isEmpty()) {
                const QString tagPath = parts.join(QLatin1Char('/'));
                if (!itsTags.contains(tagPath)) {
                    itsTags.append(tagPath);
                }
                parts.removeLast();
            }
        }
        for (const QString& tagPath : std::as_const(itsTags)) {
            ++counts[tagPath];
        }
    }
    QStringList paths = counts.keys();
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(paths.begin(), paths.end(), [&collator](const QString& left, const QString& right) {
        const QStringList leftParts = left.split(QLatin1Char('/'));
        const QStringList rightParts = right.split(QLatin1Char('/'));
        for (qsizetype index = 0; index < std::min(leftParts.size(), rightParts.size()); ++index) {
            const int order = collator.compare(leftParts.at(index), rightParts.at(index));
            if (order != 0) {
                return order < 0;
            }
        }
        return leftParts.size() < rightParts.size();
    });
    QVariantList list;
    for (const QString& tagPath : std::as_const(paths)) {
        list.append(QVariantMap{
            {QStringLiteral("tag"), tagPath},
            {QStringLiteral("name"), tagPath.section(QLatin1Char('/'), -1)},
            {QStringLiteral("depth"), static_cast<int>(tagPath.count(QLatin1Char('/')))},
            {QStringLiteral("count"), counts.value(tagPath)},
        });
    }
    return list;
}

bool VaultController::renameTag(const QString& tag, const QString& newName)
{
    if (currentState != State::Unlocked || tag.isEmpty()) {
        return false;
    }
    const QString renamed = normalizedTag(newName);
    if (renamed.isEmpty()) {
        say(tr("Give the tag a name."), true);
        return false;
    }
    if (renamed == tag) {
        return true;
    }
    bool anyChanged = false;
    for (Item& one : contents.items) {
        QStringList kept;
        for (const QString& old : std::as_const(one.tags)) {
            const QString now = isUnder(old, tag) ? renamed + old.mid(tag.size()) : old;
            anyChanged = anyChanged || now != old;
            if (!kept.contains(now)) {
                kept.append(now);
            }
        }
        one.tags = kept;
    }
    if (!anyChanged) {
        return false;
    }
    if (chosenFilter.startsWith(QLatin1String("tag:")) && isUnder(chosenFilter.mid(4), tag)) {
        setFilter(QLatin1String("tag:") + renamed + chosenFilter.mid(4 + tag.size()));
    }
    contentsChanged();
    saveInBackground(QString());
    return true;
}

void VaultController::removeTag(const QString& tag)
{
    if (currentState != State::Unlocked || tag.isEmpty()) {
        return;
    }
    bool anyChanged = false;
    for (Item& one : contents.items) {
        anyChanged = one.tags.removeIf([&tag](const QString& old) { return isUnder(old, tag); }) > 0 || anyChanged;
    }
    if (!anyChanged) {
        return;
    }
    if (chosenFilter.startsWith(QLatin1String("tag:")) && isUnder(chosenFilter.mid(4), tag)) {
        setFilter(QString());
    }
    contentsChanged();
    saveInBackground(QString());
}

void VaultController::setFavourite(const QString& id, bool favourite)
{
    if (currentState != State::Unlocked) {
        return;
    }
    for (Item& one : contents.items) {
        if (one.id.toString(QUuid::WithoutBraces) == id && one.favourite != favourite) {
            one.favourite = favourite;
            contentsChanged();
            saveInBackground(id);
            return;
        }
    }
}

QString VaultController::readAttachment(const QUrl& file)
{
    const QString source = file.isLocalFile() ? file.toLocalFile() : file.toString();
    const QString requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (currentState != State::Unlocked || source.isEmpty()) {
        return {};
    }
    const quint64 session = vaultSession;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&filePool, [self, source, requestId, session]() {
        enum class Outcome { Read, TooLarge, CannotRead };
        Outcome outcome = Outcome::CannotRead;
        QByteArray data;
        QFile input(source);
        if (input.open(QIODevice::ReadOnly)) {
            if (input.size() > largestAttachmentBytes) {
                outcome = Outcome::TooLarge;
            } else {
                data = input.read(largestAttachmentBytes + 1);
                outcome = input.error() != QFileDevice::NoError ? Outcome::CannotRead
                          : data.size() > largestAttachmentBytes ? Outcome::TooLarge
                                                                 : Outcome::Read;
            }
        }
        const QString name = QFileInfo(source).fileName();
        QMetaObject::invokeMethod(
            qApp,
            [self, requestId, session, outcome, data, name]() {
                if (!self || session != self->vaultSession || self->currentState != State::Unlocked) {
                    return;
                }
                if (outcome == Outcome::TooLarge) {
                    self->say(tr("“%1” is larger than %2, the most an attachment may hold.")
                                  .arg(name, self->sizeText(largestAttachmentBytes)),
                              true);
                    emit self->attachmentRead(requestId, name, 0, false);
                    return;
                }
                if (outcome == Outcome::CannotRead) {
                    self->say(tr("“%1” could not be read.").arg(name), true);
                    emit self->attachmentRead(requestId, name, 0, false);
                    return;
                }
                qsizetype total = self->attachmentBytes() + data.size();
                for (const Attachment& pending : std::as_const(self->pendingAttachments)) {
                    total += pending.data.size();
                }
                if (total > largestAttachmentsTotal) {
                    self->say(tr("The attachments of this vault together may hold at most %1. Remove an attachment "
                                 "first.")
                                  .arg(self->sizeText(largestAttachmentsTotal)),
                              true);
                    emit self->attachmentRead(requestId, name, 0, false);
                    return;
                }
                self->pendingAttachments.insert(requestId, Attachment{QUuid::fromString(requestId), name, data});
                emit self->attachmentRead(requestId, name, data.size(), true);
            },
            Qt::QueuedConnection);
    });
    return requestId;
}

void VaultController::saveAttachment(const QString& itemId, const QString& attachmentId, const QUrl& file)
{
    const QString target = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (currentState != State::Unlocked || target.isEmpty()) {
        return;
    }
    for (const Item& one : std::as_const(contents.items)) {
        if (one.id.toString(QUuid::WithoutBraces) != itemId) {
            continue;
        }
        for (const Attachment& attachment : one.attachments) {
            if (attachment.id.toString(QUuid::WithoutBraces) != attachmentId) {
                continue;
            }
            // Written only where the person chose, never anywhere else
            // (REQUIREMENTS.md 4.1, D-029).
            QPointer<VaultController> self(this);
            const QByteArray data = attachment.data;
            const QString name = attachment.name;
            (void)QtConcurrent::run(&filePool, [self, target, data, name]() {
                QSaveFile output(target);
                const bool written = output.open(QIODevice::WriteOnly) && output.write(data) == data.size() &&
                                     output.commit();
                QMetaObject::invokeMethod(
                    qApp,
                    [self, written, name]() {
                        if (!self) {
                            return;
                        }
                        if (written) {
                            self->say(tr("“%1” was saved.").arg(name), false);
                        } else {
                            self->say(tr("“%1” could not be saved: the folder cannot be written to, or the disk is "
                                         "full.")
                                          .arg(name),
                                      true);
                        }
                    },
                    Qt::QueuedConnection);
            });
            return;
        }
    }
}

QString VaultController::sizeText(qint64 bytes) const
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

void VaultController::contentsChanged()
{
    listModel.setContents(contents);
    ++contentsRevision;
    emit revisionChanged();
    fetchWebsiteIcons();
}

void VaultController::watch()
{
    const QStringList watched = watcher.files() + watcher.directories();
    if (!watched.isEmpty()) {
        watcher.removePaths(watched);
    }
    if (path.isEmpty()) {
        return;
    }
    // The folder too: a vault replaced by a save elsewhere is a new file.
    watcher.addPath(QFileInfo(path).absolutePath());
    if (QFileInfo::exists(path)) {
        watcher.addPath(path);
    }
}

void VaultController::checkForChangeElsewhere()
{
    if (!watcher.files().contains(path) && QFileInfo::exists(path)) {
        watcher.addPath(path);
    }
    if (checkPending || readingAgain || currentState != State::Unlocked || dirty || savesRunning > 0) {
        return;
    }
    checkPending = true;
    const QString vaultFile = path;
    const quint64 session = vaultSession;
    const std::shared_ptr<Stamps> box = stamps;
    QPointer<VaultController> self(this);
    // On the save thread: after any save of this Chrono Vault's own.
    (void)QtConcurrent::run(&savePool, [self, vaultFile, session, box]() {
        const QByteArray onDisk = vaultFileStamp(vaultFile);
        const bool changed = !onDisk.isEmpty() && onDisk != box->get(vaultFile);
        QMetaObject::invokeMethod(
            qApp,
            [self, vaultFile, session, changed]() {
                if (!self) {
                    return;
                }
                self->checkPending = false;
                if (changed && !self->readingAgain && session == self->vaultSession && self->path == vaultFile &&
                    self->currentState == State::Unlocked && !self->dirty && self->savesRunning == 0) {
                    self->readAgain(tr("The vault was changed elsewhere and was read again."), false);
                }
            },
            Qt::QueuedConnection);
    });
}

void VaultController::readAgain(const QString& said, bool important)
{
    readingAgain = true;
    const QString vaultFile = path;
    const quint64 session = vaultSession;
    VaultKey kept = key;
    kept.key = QByteArray(key.key.constData(), key.key.size());
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&savePool, [self, vaultFile, session, kept, said, important]() mutable {
        OpenResult opened = openVaultFileWithKey(vaultFile, kept);
        kept.wipe();
        ContentsResult read;
        if (opened.status == OpenStatus::Opened) {
            read = contentsFromJson(opened.contents);
            crypto::wipe(opened.contents);
        }
        opened.key.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, vaultFile, session, opened, read, said, important]() {
                if (self) {
                    self->readingAgain = false;
                }
                if (!self || session != self->vaultSession || self->path != vaultFile ||
                    self->currentState != State::Unlocked) {
                    return;
                }
                if (opened.status == OpenStatus::Opened && read.status == ContentsStatus::Read) {
                    if (self->dirty || self->savesRunning > 0) {
                        return; // changed here meanwhile: the next save keeps both (D-032)
                    }
                    self->stamps->set(vaultFile, opened.stamp);
                    self->contents = read.contents;
                    self->base = read.contents;
                    self->contentsChanged();
                    self->say(said, important);
                    return;
                }
                if (opened.status == OpenStatus::FileMissing) {
                    return; // being replaced; the next change of the folder checks again
                }
                if (opened.status == OpenStatus::KeyChanged) {
                    self->forgetUnlockedVault();
                    self->setState(State::Locked);
                    self->say((important ? said + QLatin1Char(' ') : QString()) +
                                  tr("The vault was changed elsewhere and now has another master password. Unlock it "
                                     "with that one."),
                              true);
                    return;
                }
                // Not readable yet, for example while a cloud folder is still
                // writing it: the vault stays open, and its next change is
                // checked again; its stamp stays the old one, so a save of
                // this version never goes over the other (OR-15).
                self->say((important ? said + QLatin1Char(' ') : QString()) +
                              tr("The vault was changed elsewhere, but that version cannot be read yet. Your vault "
                                 "stays open."),
                          true);
            },
            Qt::QueuedConnection);
    });
}

QString VaultController::conflictPathFor(const QString& vaultFile)
{
    const QFileInfo info(vaultFile);
    const QString when = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm"));
    const QString base = info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(" (conflict ") +
                         when;
    QString candidate = base + QStringLiteral(").chronovault");
    for (int number = 2; QFileInfo::exists(candidate); ++number) {
        candidate = base + QStringLiteral(" %1).chronovault").arg(number);
    }
    return candidate;
}

QString VaultController::primaryVault() const
{
    // Always the first: a primary on a drive that is not there at the moment
    // never hands its place to another vault (D-030).
    return vaultList.isEmpty() ? QString() : vaultList.constFirst();
}

QString VaultController::keyfilePath() const
{
    return keyfileOf(path);
}

void VaultController::setKeyfilePath(const QString& keyfile)
{
    const QUrl asUrl(keyfile);
    rememberKeyfile(path, asUrl.isLocalFile() ? asUrl.toLocalFile() : keyfile);
}

QString VaultController::keyfileOf(const QString& vaultFile) const
{
    return VaultsFile().value(keyfilesKey).toMap().value(vaultFile).toString();
}

void VaultController::rememberKeyfile(const QString& vaultFile, const QString& keyfile)
{
    if (vaultFile.isEmpty() || keyfileOf(vaultFile) == keyfile) {
        return;
    }
    VaultsFile settings;
    QVariantMap keyfiles = settings.value(keyfilesKey).toMap();
    if (keyfile.isEmpty()) {
        keyfiles.remove(vaultFile);
    } else {
        keyfiles.insert(vaultFile, keyfile);
    }
    settings.setValue(keyfilesKey, keyfiles);
    if (vaultFile == path) {
        emit keyfileChanged();
    }
}

void VaultController::addVault(const QString& vaultFile)
{
    if (vaultFile.isEmpty() || vaultList.contains(vaultFile)) {
        return;
    }
    vaultList.append(vaultFile);
    VaultsFile().setValue(vaultListKey, vaultList);
    emit vaultsChanged();
}

bool VaultController::canSaveInPrimary() const
{
    return currentState == State::Unlocked && !path.isEmpty() && path == primaryVault();
}

QString VaultController::vaultNameOf(const QString& vaultFile) const
{
    return QFileInfo(vaultFile).completeBaseName();
}

void VaultController::rememberPrimary()
{
    if (currentState != State::Unlocked || path != primaryVault()) {
        return;
    }
    primaryKey.wipe();
    primaryKey = key;
    primaryKey.key = QByteArray(key.key.constData(), key.key.size()); // its own copy, wiped on its own
    crypto::wipe(primaryKeyfile);
    primaryKeyfile = QByteArray(keyfileSecret.constData(), keyfileSecret.size());
    savedPasswords.clear();
    for (const Item& one : std::as_const(contents.items)) {
        QString vaultFile;
        QString password;
        for (const Field& field : one.fields) {
            if (field.role == QLatin1String("vaultfile")) {
                vaultFile = field.value;
            } else if (field.role == QLatin1String("password")) {
                password = field.value;
            }
        }
        if (!vaultFile.isEmpty() && !password.isEmpty()) {
            savedPasswords.insert(vaultFile, password);
        }
    }
}

void VaultController::forgetPrimary()
{
    primaryKey.wipe();
    primaryKey = {};
    crypto::wipe(primaryKeyfile);
    primaryKeyfile.clear();
    savedPasswords.clear();
}

void VaultController::showCreated(CreatedVault created)
{
    forgetUnlockedVault();
    setPath(created.path);
    keyfileSecret = created.keyfile;
    key = created.key;
    contents = {};
    contentsChanged();
    say(tr("Your new vault is ready."), false);
    setState(State::Unlocked);
    fetchWebsiteIcons();
    startWebDav(); // a new vault kept on a server goes there now (D-057)
    if (primaryVault() == created.path) {
        rememberPrimary();
    }
}

void VaultController::switchToVault(const QString& vaultFile)
{
    if (vaultFile.isEmpty() || vaultFile == path || currentState == State::Busy) {
        return;
    }
    if (currentState == State::Unlocked && (dirty || savesRunning > 0)) {
        say(tr("Wait until your last change is saved, then try again."), true);
        return;
    }
    if (currentState == State::Unlocked && path == primaryVault()) {
        rememberPrimary();
    }
    const bool canUseKept = currentState == State::Unlocked || !savedPasswords.isEmpty() || primaryKey.isValid();
    forgetUnlockedVault();
    ++unlockAttempt;
    addVault(vaultFile);
    setPath(vaultFile);
    needKeyfile = false;
    emit keyfileChanged();
    if (canUseKept && vaultFile == primaryVault() && primaryKey.isValid()) {
        VaultKey kept = primaryKey;
        kept.key = QByteArray(primaryKey.key.constData(), primaryKey.key.size());
        // Its keyfile's fingerprint comes back with it, for a later change of
        // its master password.
        keyfileSecret = QByteArray(primaryKeyfile.constData(), primaryKeyfile.size());
        startOpenWithKey(kept);
        crypto::wipe(kept.key);
        return;
    }
    if (canUseKept && savedPasswords.contains(vaultFile)) {
        QByteArray password = savedPasswords.value(vaultFile).toUtf8();
        setState(State::Locked);
        startUnlock(password, true);
        crypto::wipe(password);
        return;
    }
    say(QString(), false);
    setState(State::Locked);
}

void VaultController::updateSavedPassword(const QString& vaultFile, const QString& password)
{
    // The primary is not shown: it is read with its kept key, its saved
    // password for this vault changed, and written as safely as every save,
    // on the save thread (OR-5, OR-15).
    VaultKey kept = primaryKey;
    kept.key = QByteArray(primaryKey.key.constData(), primaryKey.key.size());
    const QString primary = primaryVault();
    QPointer<VaultController> self(this);
    const std::shared_ptr<Stamps> box = stamps;
    (void)QtConcurrent::run(&savePool, [self, kept, primary, vaultFile, password, box]() mutable {
        bool updated = false;
        OpenResult opened = openVaultFileWithKey(primary, kept);
        if (opened.status == OpenStatus::Opened) {
            ContentsResult read = contentsFromJson(opened.contents);
            crypto::wipe(opened.contents);
            if (read.status == ContentsStatus::Read) {
                bool found = false;
                for (Item& one : read.contents.items) {
                    const bool names = std::any_of(one.fields.cbegin(), one.fields.cend(), [&vaultFile](const Field& f) {
                        return f.role == QLatin1String("vaultfile") && f.value == vaultFile;
                    });
                    if (!names) {
                        continue;
                    }
                    for (Field& field : one.fields) {
                        if (field.role == QLatin1String("password")) {
                            field.value = password;
                            found = true;
                        }
                    }
                    one.modified = QDateTime::currentDateTimeUtc();
                }
                QByteArray json = contentsToJson(read.contents);
                QByteArray written;
                updated = found && saveVaultFile(primary, json, opened.key, {}, &written) == SaveStatus::Saved;
                if (updated) {
                    box->set(primary, written);
                }
                crypto::wipe(json);
            }
        }
        opened.key.wipe();
        kept.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, vaultFile, password, primary, updated]() {
                if (!self) {
                    return;
                }
                if (updated) {
                    if (self->primaryKey.isValid()) {
                        self->savedPasswords.insert(vaultFile, password);
                    }
                    return;
                }
                self->savedPasswords.remove(vaultFile);
                self->say(tr("The master password was changed, but the copy saved in “%1” could not be changed. "
                             "Change it there by hand.")
                              .arg(QFileInfo(primary).completeBaseName()),
                          true);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::makeKeyfile(const QUrl& file)
{
    const QString target = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (target.isEmpty()) {
        return;
    }
    if (QFileInfo::exists(target)) {
        // Never over a file: it may be a vault's keyfile (OR-15).
        say(tr("A file with that name already exists. Choose another name."), true);
        emit keyfileMade(target, false);
        return;
    }
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&filePool, [self, target]() {
        const bool made = chrono_vault::makeKeyfile(target);
        QMetaObject::invokeMethod(
            qApp,
            [self, target, made]() {
                if (!self) {
                    return;
                }
                if (!made) {
                    self->say(tr("The keyfile could not be written: the folder cannot be written to, or the disk "
                                 "is full."),
                              true);
                }
                emit self->keyfileMade(target, made);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::exportVault(const QUrl& file, const QString& password)
{
    const QString target = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (currentState != State::Unlocked || target.isEmpty()) {
        emit exportDone(false);
        return;
    }
    QByteArray typed = password.toUtf8();
    const QByteArray secret = keyfileSecret;
    const VaultKey current = key;
    const VaultContents snapshot = contents;
    const quint64 session = vaultSession;
    say(tr("Exporting…"), false);
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&filePool, [self, target, typed, secret, current, snapshot, session]() mutable {
        enum class Outcome { Written, WrongPassword, CannotWrite };
        Outcome outcome = Outcome::CannotWrite;
        // The master password checked as the vault itself would (D-037).
        QByteArray checked = current.usesKeyfile ? typed + secret : typed;
        const auto derived = crypto::deriveKey(checked, current.salt, current.parameters);
        crypto::wipe(checked);
        crypto::wipe(typed);
        if (!derived || *derived != current.key) {
            outcome = Outcome::WrongPassword;
        } else {
            QByteArray json = writeReferenceExport(snapshot);
            QSaveFile output(target);
            // Readable by the person alone: it holds every password unencrypted.
            outcome = output.open(QIODevice::WriteOnly) &&
                              output.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) &&
                              output.write(json) == json.size() && output.commit()
                          ? Outcome::Written
                          : Outcome::CannotWrite;
            crypto::wipe(json);
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, outcome, target, session]() {
                if (!self || session != self->vaultSession) {
                    return;
                }
                switch (outcome) {
                case Outcome::Written:
                    self->say(tr("Exported to “%1”. It holds every password unencrypted: keep it safe and delete it "
                                 "when done.")
                                  .arg(QFileInfo(target).fileName()),
                              false);
                    emit self->exportDone(true);
                    return;
                case Outcome::WrongPassword:
                    self->say(tr("That is not the master password. Nothing was exported."), true);
                    break;
                case Outcome::CannotWrite:
                    self->say(tr("The export could not be written: the folder cannot be written to, or the disk is "
                                 "full."),
                              true);
                    break;
                }
                emit self->exportDone(false);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::changeMasterPassword(const QString& current, const QString& password, const QString& repeated,
                                           const QUrl& keyfile)
{
    if (currentState != State::Unlocked) {
        return;
    }
    if (password.isEmpty()) {
        say(tr("Choose a master password."), true);
        emit masterPasswordChanged(false);
        return;
    }
    if (password != repeated) {
        say(tr("The two passwords are not the same."), true);
        emit masterPasswordChanged(false);
        return;
    }
    if (dirty || savesRunning > 0) {
        say(tr("Wait until your last change is saved, then try again."), true);
        emit masterPasswordChanged(false);
        return;
    }
    const QString keyfileFile = keyfile.isLocalFile() ? keyfile.toLocalFile() : keyfile.toString();
    const QString newPassword = password;
    QByteArray currentBytes = current.toUtf8();
    QByteArray newBytes = password.toUtf8();
    const QByteArray oldSecret = keyfileSecret;
    const VaultKey oldKey = key;
    const crypto::KdfParameters parameters = kdfParameters;
    const VaultContents snapshot = contents;
    const QString vaultFile = path;
    const quint64 session = vaultSession;
    setState(State::Busy);
    say(tr("Changing the master password…"), false);
    QPointer<VaultController> self(this);
    const std::shared_ptr<Stamps> box = stamps;
    // On the save thread, as every write of an open vault, so no check of a
    // change made elsewhere reads it while it is replaced (D-032).
    (void)QtConcurrent::run(&savePool, [self, currentBytes, newBytes, oldSecret, oldKey, parameters, snapshot,
                                          vaultFile, keyfileFile, session, newPassword, box]() mutable {
        enum class Outcome { Changed, WrongPassword, KeyfileUnreadable, CannotWrite };
        Outcome outcome = Outcome::CannotWrite;
        VaultKey newKey;
        std::optional<QByteArray> fingerprint = QByteArray();
        // The current password is checked as the vault itself would.
        QByteArray secret = oldKey.usesKeyfile ? currentBytes + oldSecret : currentBytes;
        const auto check = crypto::deriveKey(secret, oldKey.salt, oldKey.parameters);
        crypto::wipe(secret);
        if (!check || *check != oldKey.key) {
            outcome = Outcome::WrongPassword;
        } else {
            if (!keyfileFile.isEmpty()) {
                fingerprint = keyfileFingerprint(keyfileFile);
            }
            if (!fingerprint) {
                outcome = Outcome::KeyfileUnreadable;
            } else {
                newKey = makeVaultKey(newBytes, parameters, *fingerprint);
                QByteArray json = contentsToJson(snapshot);
                // Safe as every save: the old vault stays until the new one is
                // verified (OR-15).
                QByteArray written;
                outcome = newKey.isValid() && saveVaultFile(vaultFile, json, newKey, {}, &written) == SaveStatus::Saved
                              ? Outcome::Changed
                              : Outcome::CannotWrite;
                if (outcome == Outcome::Changed) {
                    box->set(vaultFile, written);
                }
                crypto::wipe(json);
            }
        }
        crypto::wipe(currentBytes);
        crypto::wipe(newBytes);
        QMetaObject::invokeMethod(
            qApp,
            [self, outcome, newKey, keyfileFile, fingerprint, session, newPassword]() mutable {
                if (!self || session != self->vaultSession) {
                    newKey.wipe();
                    return;
                }
                self->setState(State::Unlocked);
                switch (outcome) {
                case Outcome::Changed:
                    self->key.wipe();
                    self->key = newKey;
                    self->keyfileSecret = *fingerprint;
                    self->rememberKeyfile(self->path, keyfileFile);
                    if (self->syncer) {
                        self->startWebDav(); // the sync goes on with the new key (D-057)
                    }
                    if (self->path == self->primaryVault()) {
                        self->rememberPrimary();
                    } else if (self->savedPasswords.contains(self->path) && self->primaryKey.isValid()) {
                        self->updateSavedPassword(self->path, newPassword);
                    }
                    self->recordMasterPassword(); // just typed: the reminder waits a year
                    if (self->quickUnlockOn()) {
                        // The finger's key follows the new master password (D-038);
                        // Windows Hello asks once more for that, and says so.
                        // Said first, so a failure to keep it is the last word.
                        self->say(tr("The master password was changed. %1 keeps the new one; it may ask you once.")
                                      .arg(self->quickUnlockName()),
                                  false);
                        self->replacingQuickKey = true;
                        self->quick->replace(self->path, quickSecretOf(self->key, self->keyfileSecret));
                    } else {
                        self->say(tr("The master password was changed."), false);
                    }
                    emit self->masterPasswordChanged(true);
                    return;
                case Outcome::WrongPassword:
                    self->say(tr("That is not the current master password."), true);
                    break;
                case Outcome::KeyfileUnreadable:
                    self->say(tr("The keyfile could not be read. Nothing was changed."), true);
                    break;
                case Outcome::CannotWrite:
                    newKey.wipe();
                    self->say(tr("The master password could not be changed: the folder cannot be written to, or "
                                 "the disk is full. The vault is unchanged."),
                              true);
                    break;
                }
                emit self->masterPasswordChanged(false);
            },
            Qt::QueuedConnection);
    });
}

qsizetype VaultController::attachmentBytes() const
{
    qsizetype total = 0;
    for (const Item& one : contents.items) {
        for (const Attachment& attachment : one.attachments) {
            total += attachment.data.size();
        }
    }
    return total;
}

QString VaultController::readItemIcon(const QUrl& file)
{
    const IconResult result = readIcon(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (result.status != IconStatus::Read) {
        say(iconStatusText(result.status), true);
        return {};
    }
    return QString::fromLatin1(result.png.toBase64());
}

QStringList VaultController::customIcons() const
{
    QStringList icons;
    const auto add = [&icons](const QByteArray& png) {
        const QString text = QString::fromLatin1(png.toBase64());
        if (!png.isEmpty() && !icons.contains(text)) {
            icons.append(text);
        }
    };
    for (const QByteArray& png : contents.customIcons) {
        add(png);
    }
    return icons;
}

QString VaultController::addCustomIcon(const QUrl& file)
{
    if (currentState != State::Unlocked) {
        return {};
    }
    const QString icon = readItemIcon(file);
    if (icon.isEmpty()) {
        return {};
    }
    const QByteArray png = QByteArray::fromBase64(icon.toLatin1());
    if (!contents.customIcons.contains(png)) {
        contents.customIcons.append(png);
        contentsChanged();
        saveInBackground(QString());
    }
    return icon;
}

int VaultController::customIconUses(const QString& icon) const
{
    const QByteArray png = QByteArray::fromBase64(icon.toLatin1());
    return static_cast<int>(std::count_if(contents.items.cbegin(), contents.items.cend(),
                                          [&png](const Item& one) { return !png.isEmpty() && one.icon == png; }));
}

void VaultController::deleteCustomIcon(const QString& icon)
{
    const QByteArray png = QByteArray::fromBase64(icon.toLatin1());
    if (currentState != State::Unlocked || png.isEmpty()) {
        return;
    }
    bool changed = contents.customIcons.removeAll(png) > 0;
    for (Item& one : contents.items) {
        if (one.icon == png) {
            one.icon.clear(); // its category's icon again
            changed = true;
        }
    }
    if (changed) {
        contentsChanged();
        saveInBackground(QString());
    }
}

QVariantList VaultController::iconSections() const
{
    QVariantList sections;
    for (const IconSection& section : readIconSections(personalIconsFolder())) {
        QVariantList icons;
        for (const QString& iconPath : section.files) {
            // Its file name is its name, translated where a translation exists (the owner).
            const QString name = QFileInfo(iconPath).completeBaseName();
            icons.append(QVariantMap{{QStringLiteral("name"), QCoreApplication::translate("IconNames", name.toUtf8().constData())},
                                     {QStringLiteral("url"), QUrl::fromLocalFile(iconPath)}});
        }
        // A heading without a translation is shown as written (the owner).
        sections.append(QVariantMap{
            {QStringLiteral("heading"), QCoreApplication::translate("IconSections", section.heading.toUtf8().constData())},
            {QStringLiteral("icons"), icons}});
    }
    return sections;
}

void VaultController::retrySave()
{
    if (currentState == State::Unlocked && dirty) {
        saveInBackground(QString());
    }
}

void VaultController::copyToClipboard(const QString& text)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (!clipboard || text.isEmpty()) {
        return;
    }
    auto* data = new QMimeData;
    data->setText(text);
    // Clipboard history tools of KDE and Windows leave such content out.
    data->setData(QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret"));
    data->setData(QStringLiteral("application/x-qt-windows-mime;value=\"ExcludeClipboardContentFromMonitorProcessing\""),
                  QByteArray(4, '\0'));
    clipboard->setMimeData(data);
    copiedText = text;
    if (clearSeconds > 0) {
        clipboardTimer.start(clearSeconds * 1000);
        say(tr("Copied. The clipboard is cleared in %n second(s).", nullptr, clearSeconds), false);
    } else {
        clipboardTimer.stop();
        say(tr("Copied."), false);
    }
}

QStringList VaultController::defaultKeysOf(const QString& action) const
{
    // The keys Chrono Vault starts with: the reference program's (D-039),
    // Ctrl+B for the sidebar and Shift+Ctrl+B to clear the clipboard (D-045).
    static const QHash<QString, QStringList> defaults{
        {QStringLiteral("quit"), {QStringLiteral("Ctrl+Q")}},
        {QStringLiteral("archive"), {QStringLiteral("Shift+Backspace")}},
        {QStringLiteral("moveToTrash"), {QStringLiteral("Backspace")}},
        {QStringLiteral("copyUsername"), {QStringLiteral("Shift+Ctrl+U")}},
        {QStringLiteral("copyEmail"), {QStringLiteral("Shift+Ctrl+E")}},
        {QStringLiteral("copyPassword"), {QStringLiteral("Shift+Ctrl+P")}},
        {QStringLiteral("showPasswords"), {QStringLiteral("Shift+Ctrl+H")}},
        {QStringLiteral("copyUrl"), {QStringLiteral("Shift+Ctrl+R")}},
        {QStringLiteral("copyOtp"), {QStringLiteral("Shift+Ctrl+T")}},
        {QStringLiteral("launchUrl"), {QStringLiteral("Shift+Ctrl+L")}},
        {QStringLiteral("lock"), {QStringLiteral("Ctrl+L")}},
        {QStringLiteral("syncNow"), {QStringLiteral("Ctrl+J")}},
        {QStringLiteral("clearClipboard"), {QStringLiteral("Shift+Ctrl+B")}},
        {QStringLiteral("generatePassword"), {QStringLiteral("Ctrl+G")}},
        {QStringLiteral("about"), {QStringLiteral("F1")}},
        {QStringLiteral("sidebar"), {QStringLiteral("Ctrl+B")}},
    };
    return defaults.value(action);
}

QStringList VaultController::keysOf(const QString& action) const
{
    SettingsFile settings;
    const QString settingName = QStringLiteral("shortcuts/") + action;
    return settings.contains(settingName) ? settings.value(settingName).toStringList() : defaultKeysOf(action);
}

void VaultController::setKeys(const QString& action, const QStringList& keys)
{
    SettingsFile settings;
    const QString settingName = QStringLiteral("shortcuts/") + action;
    if (keys == defaultKeysOf(action)) {
        settings.remove(settingName);
    } else {
        // An empty list is kept as a list, so "no key" is remembered.
        settings.setValue(settingName, keys.isEmpty() ? QVariant(QStringList{}) : QVariant(keys));
    }
    ++keysRevision;
    emit shortcutsChanged();
}

QString VaultController::keyName(int keyCode, int modifiers) const
{
    if (keyCode == Qt::Key_Shift || keyCode == Qt::Key_Control || keyCode == Qt::Key_Alt || keyCode == Qt::Key_Meta ||
        keyCode == Qt::Key_unknown || keyCode == 0) {
        return {};
    }
    QString name;
    if (modifiers & Qt::ShiftModifier) {
        name += QStringLiteral("Shift+");
    }
    if (modifiers & Qt::ControlModifier) {
        name += QStringLiteral("Ctrl+");
    }
    if (modifiers & Qt::AltModifier) {
        name += QStringLiteral("Alt+");
    }
    if (modifiers & Qt::MetaModifier) {
        name += QStringLiteral("Meta+");
    }
    return name + QKeySequence(keyCode).toString(QKeySequence::PortableText);
}

void VaultController::clearClipboard()
{
    clipboardTimer.stop();
    copiedText.clear();
    if (QClipboard* clipboard = QGuiApplication::clipboard()) {
        clipboard->clear();
    }
    say(tr("The clipboard is cleared."), false);
}

QString VaultController::generatePassword(int length, bool uppercase, bool lowercase, bool digits,
                                          bool symbols) const
{
    return chrono_vault::generatePassword(GeneratorOptions{length, uppercase, lowercase, digits, symbols});
}

QString VaultController::oneTimeCode(const QString& secret) const
{
    const auto settings = parseTotp(secret);
    return settings ? totpCode(*settings, QDateTime::currentSecsSinceEpoch()) : QString();
}

int VaultController::oneTimeCodeSecondsLeft(const QString& secret) const
{
    const auto settings = parseTotp(secret);
    return settings ? totpSecondsLeft(*settings, QDateTime::currentSecsSinceEpoch()) : 0;
}

QString VaultController::fieldName(const QString& role) const
{
    return roleName(role);
}

QVariantMap VaultController::settingsDefaults() const
{
    return {
        {QStringLiteral("autoLockMinutes"), defaultAutoLockMinutes},
        {QStringLiteral("idleLockOn"), true},
        {QStringLiteral("lockWhenClosed"), false},
        {QStringLiteral("lockOnSleep"), false},
        {QStringLiteral("systemIdleLockOn"), false},
        {QStringLiteral("systemIdleMinutes"), defaultSystemIdleMinutes},
        {QStringLiteral("clipboardClearSeconds"), defaultClipboardSeconds},
        {QStringLiteral("keepInTray"), defaultKeepInTray},
        {QStringLiteral("startMinimized"), defaultStartMinimized},
        {QStringLiteral("escapeExits"), defaultEscapeExits},
        {QStringLiteral("startAtStartup"), defaultStartAtStartup},
        {QStringLiteral("hideSensitive"), defaultHideSensitive},
        {QStringLiteral("showSubtitles"), defaultShowSubtitles},
        {QStringLiteral("useWebsiteIcons"), defaultWebsiteIcons},
        {QStringLiteral("masterPasswordReminder"), defaultReminder},
    };
}

bool VaultController::isSessionEnding() const
{
    const auto* application = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    return application && application->isSavingSession();
}

bool VaultController::isIdle() const
{
    return savesRunning == 0 && currentState != State::Busy;
}

bool VaultController::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
    case QEvent::TouchBegin:
        restartAutoLock();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

void VaultController::setState(State state)
{
    if (currentState != state) {
        currentState = state;
        if (state == State::Unlocked) {
            unlockedAtMs = QDateTime::currentMSecsSinceEpoch();
        }
        restartAutoLock();
        emit stateChanged();
    }
}

void VaultController::say(const QString& text, bool isTrouble)
{
    if (currentMessage != text || trouble != isTrouble) {
        currentMessage = text;
        trouble = isTrouble;
        emit messageChanged();
    }
    if (!isTrouble && !text.isEmpty()) {
        messageTimer.start();
    } else {
        messageTimer.stop();
    }
}

void VaultController::setPath(const QString& newPath)
{
    if (path != newPath) {
        stopQuickUnlock(); // a finger asked for another vault is no longer the question
        path = newPath;
        if (!path.isEmpty()) {
            VaultsFile().setValue(lastVaultKey, path);
        }
        needKeyfile = false;
        watch();
        updateWait(); // the wait of this vault, if one is running (D-041)
        emit vaultPathChanged();
        emit quickUnlockChanged();
        emit keyfileChanged();
    }
}

void VaultController::saveInBackground(const QString& itemId)
{
    // A copy that shares its data: put into words on the save's thread, since
    // attachments make that slow (OR-5).
    const VaultContents snapshot = contents;
    const QString vaultFile = path;
    VaultKey saveKey = key;
    ++savesRunning;
    setDirty(true);
    QPointer<VaultController> self(this);
    const std::shared_ptr<Stamps> box = stamps;
    const quint64 epoch = box->epoch.load();
    (void)QtConcurrent::run(&savePool, [self, vaultFile, snapshot, saveKey, itemId, box, epoch]() mutable {
        // A save queued before a merge was taken in: the merged save that
        // follows holds this change too, and this one would undo the merge.
        if (epoch != box->epoch.load()) {
            saveKey.wipe();
            QMetaObject::invokeMethod(
                qApp,
                [self]() {
                    if (self) {
                        --self->savesRunning;
                    }
                },
                Qt::QueuedConnection);
            return;
        }
        QByteArray json = contentsToJson(snapshot);
        // Changed elsewhere since this Chrono Vault read or wrote it: the
        // other version is read and merged with this one (D-054); only when
        // it cannot be read with this key does this one go into a conflict
        // copy, the other staying (D-032).
        const QByteArray expected = box->get(vaultFile);
        const QByteArray onDisk = vaultFileStamp(vaultFile);
        QString conflictPath;
        std::optional<VaultContents> theirs;
        QByteArray theirStamp;
        SaveStatus status = SaveStatus::CannotWrite;
        if (!expected.isEmpty() && !onDisk.isEmpty() && onDisk != expected) {
            OpenResult opened = openVaultFileWithKey(vaultFile, saveKey);
            if (opened.status == OpenStatus::Opened) {
                ContentsResult read = contentsFromJson(opened.contents);
                crypto::wipe(opened.contents);
                if (read.status == ContentsStatus::Read) {
                    theirs = std::move(read.contents);
                    theirStamp = opened.stamp;
                }
            }
            opened.key.wipe();
            if (!theirs) {
                conflictPath = conflictPathFor(vaultFile);
                // The stamp stays the old one until the other version is
                // read, so no later save goes over it before then (OR-15).
                status = saveVaultFile(conflictPath, json, saveKey);
            }
        } else {
            QByteArray written;
            status = saveVaultFile(vaultFile, json, saveKey, {}, &written);
            if (status == SaveStatus::Saved) {
                box->set(vaultFile, written);
            }
        }
        crypto::wipe(json);
        saveKey.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, status, itemId, vaultFile, conflictPath, theirs, theirStamp, snapshot]() {
                if (!self) {
                    return;
                }
                --self->savesRunning;
                if (theirs) {
                    self->mergeWithOther(vaultFile, *theirs, theirStamp, itemId);
                    return;
                }
                // A new vault waits for its password to be saved in the
                // primary before it is shown (D-030).
                std::optional<CreatedVault> created;
                if (self->createdVault && self->savesRunning == 0) {
                    created = std::move(self->createdVault);
                    self->createdVault.reset();
                }
                if (status == SaveStatus::Saved && self->savesRunning == 0 && self->currentState != State::Unlocked &&
                    self->unsavedPath == vaultFile && !self->unsavedSealed.isEmpty()) {
                    // The save that was running at the lock succeeded: the
                    // copy kept through the lock is on disk now.
                    self->unsavedSealed.clear();
                    self->unsavedBaseSealed.clear();
                    self->unsavedPath.clear();
                    emit self->keptChangesChanged();
                }
                if (status == SaveStatus::Saved && conflictPath.isEmpty() && self->path == vaultFile) {
                    self->base = snapshot; // what both sides now agree on (D-054)
                }
                if (status == SaveStatus::Saved) {
                    if (self->savesRunning == 0 && self->currentState == State::Unlocked) {
                        self->setDirty(false);
                        self->setFailed(false);
                        // A merge's own message stays: it says what the person
                        // should look at (D-054).
                        if (self->trouble && self->message() != self->mergeSaid) {
                            self->say(QString(), false);
                        }
                    }
                    emit self->saved(itemId);
                } else {
                    // The change stays here, unsaved, and the vault on disk
                    // is whole (OR-15): the person is told, never left guessing.
                    self->say(saveStatusText(status), true);
                    if (self->currentState == State::Unlocked) {
                        self->setFailed(true);
                    }
                }
                if (!conflictPath.isEmpty() && status == SaveStatus::Saved) {
                    self->addVault(conflictPath);
                    if (self->currentState == State::Unlocked && self->path == vaultFile) {
                        self->readAgain(tr("“%1” was changed elsewhere. Your version was kept as “%2”; the vault now "
                                           "shows the other version.")
                                            .arg(QFileInfo(vaultFile).completeBaseName(),
                                                 QFileInfo(conflictPath).completeBaseName()),
                                        true);
                    }
                }
                if (created) {
                    if (status == SaveStatus::Saved && self->currentState == State::Unlocked) {
                        self->showCreated(*created);
                    } else {
                        created->key.wipe();
                        self->say(tr("The vault “%1” was created, but its password could not be saved in “%2”. "
                                     "Open it from the list of vaults with its own password.")
                                      .arg(QFileInfo(created->path).completeBaseName(),
                                           QFileInfo(self->primaryVault()).completeBaseName()),
                                  true);
                    }
                }
            },
            Qt::QueuedConnection);
    });
}

void VaultController::mergeWithOther(const QString& vaultFile, const VaultContents& theirs,
                                     const QByteArray& theirStamp, const QString& itemId)
{
    if (currentState != State::Unlocked || path != vaultFile) {
        return; // locked meanwhile: the change kept through the lock is merged at the unlock
    }
    // Saves queued before this are passed over: the save below holds them.
    ++stamps->epoch;
    stamps->set(vaultFile, theirStamp);
    const MergeResult merged = mergeContents(&base, contents, theirs, QDateTime::currentDateTimeUtc());
    base = theirs;
    contents = merged.contents;
    contentsChanged();
    mergeSaid = mergeText(merged);
    say(mergeSaid, merged.itemsChangedOnBoth > 0 || merged.itemsBack > 0);
    saveInBackground(itemId);
}

QString VaultController::mergeText(const MergeResult& merged)
{
    QString text = tr("The vault was changed elsewhere; both versions were merged.");
    if (merged.itemsChangedOnBoth > 0) {
        text += QLatin1Char(' ') + tr("%n item(s) had a value changed on both devices: the newest change stands, "
                                      "the other is kept in the item.",
                                      nullptr, merged.itemsChangedOnBoth);
    }
    if (merged.itemsBack > 0) {
        text += QLatin1Char(' ') +
                tr("%n item(s) deleted on one device and changed on the other were kept.", nullptr, merged.itemsBack);
    }
    return text;
}

void VaultController::restartAutoLock()
{
    if (currentState == State::Unlocked && idleLock && lockMinutes > 0) {
        autoLockTimer.start(lockMinutes * 60 * 1000);
    } else {
        autoLockTimer.stop();
    }
    // The system's inactive time is asked only while it can lock something.
    if (currentState == State::Unlocked && systemIdleLock) {
        if (!systemIdleTimer.isActive()) {
            systemIdleTimer.start();
        }
    } else {
        systemIdleTimer.stop();
    }
}

void VaultController::setDirty(bool isDirty)
{
    if (dirty != isDirty) {
        dirty = isDirty;
        emit unsavedChangesChanged();
    }
}

void VaultController::setFailed(bool hasFailed)
{
    if (failed != hasFailed) {
        failed = hasFailed;
        emit saveFailedChanged();
    }
}

void VaultController::keepUnsavedWhileLocked()
{
    // A save that failed, or one still running that may yet fail: the
    // newest contents are kept, sealed with the vault's own key, until the
    // vault is unlocked again and they can be saved.
    if (!dirty && savesRunning == 0) {
        return;
    }
    QByteArray json = contentsToJson(contents);
    unsavedSealed = crypto::seal(key.key, json, QByteArrayLiteral("unsaved"));
    crypto::wipe(json);
    json = contentsToJson(base);
    unsavedBaseSealed = crypto::seal(key.key, json, QByteArrayLiteral("unsaved base"));
    unsavedPath = path;
    unsavedStamp = stamps->get(path);
    crypto::wipe(json);
    emit keptChangesChanged();
}

void VaultController::takeBackUnsaved()
{
    if (unsavedSealed.isEmpty() || unsavedPath != path) {
        return;
    }
    auto json = crypto::open(key.key, unsavedSealed, QByteArrayLiteral("unsaved"));
    unsavedSealed.clear();
    unsavedPath.clear();
    emit keptChangesChanged();
    if (!json) {
        unsavedBaseSealed.clear();
        return;
    }
    const ContentsResult read = contentsFromJson(*json);
    crypto::wipe(*json);
    if (read.status != ContentsStatus::Read) {
        return;
    }
    const QByteArray madeAgainst = unsavedStamp;
    unsavedStamp.clear();
    if (madeAgainst.isEmpty() || madeAgainst == unlockedStamp) {
        unsavedBaseSealed.clear();
        contents = read.contents;
        contentsChanged();
        saveInBackground(QString());
        return;
    }
    // Changed elsewhere while it was locked: the kept change is merged with
    // the version read at the unlock, against the base it was made on
    // (D-054, P6).
    std::optional<VaultContents> keptBase;
    if (auto baseJson = crypto::open(key.key, unsavedBaseSealed, QByteArrayLiteral("unsaved base"))) {
        const ContentsResult readBase = contentsFromJson(*baseJson);
        crypto::wipe(*baseJson);
        if (readBase.status == ContentsStatus::Read) {
            keptBase = readBase.contents;
        }
    }
    unsavedBaseSealed.clear();
    const MergeResult merged =
        mergeContents(keptBase ? &*keptBase : nullptr, read.contents, contents, QDateTime::currentDateTimeUtc());
    contents = merged.contents;
    contentsChanged();
    mergeSaid = mergeText(merged);
    say(mergeSaid, merged.itemsChangedOnBoth > 0 || merged.itemsBack > 0);
    saveInBackground(QString());
}

void VaultController::forgetUnlockedVault()
{
    stopWebDav();
    setDirty(false);
    setFailed(false);
    key.wipe();
    crypto::wipe(keyfileSecret);
    keyfileSecret.clear();
    if (createdVault) {
        createdVault->key.wipe();
        createdVault.reset();
    }
    contents = {};
    base = {};
    pendingAttachments.clear();
    pendingImport.reset();
    // Website icons are kept in memory only, and only while unlocked (D-036).
    websiteIcons.clear();
    iconsAsked.clear();
    listModel.setWebsiteIcons({});
    ++vaultSession;
    listModel.clear();
    if (!search.isEmpty()) {
        search.clear();
        emit searchTextChanged();
    }
    if (!chosenFilter.isEmpty()) {
        chosenFilter.clear();
        emit filterChanged();
    }
    ++contentsRevision;
    emit revisionChanged();
}

} // namespace chrono_vault
