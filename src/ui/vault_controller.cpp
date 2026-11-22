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
#include "item_icons.h"
#include "password_generator.h"
#include "templates.h"
#include "totp.h"
#include "version.h"

#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>
#include <QGuiApplication>
#include <QMimeData>
#include <QPointer>
#include <QSettings>
#include <QVariantList>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller.cpp 0.0.17

// --- Version history ----------------------------------------------------
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
// The settings as they are before the person changes them, also what
// Restore defaults puts back (D-022).
constexpr int defaultAutoLockMinutes = 5;
constexpr int defaultClipboardSeconds = 30;
constexpr bool defaultKeepInTray = true;
constexpr bool defaultStartMinimized = false;
constexpr bool defaultStartAtStartup = false;
const QString clipboardKey = QStringLiteral("clipboard/clearSeconds");
const QString hideSensitiveKey = QStringLiteral("view/hideSensitive");
const QString showSubtitlesKey = QStringLiteral("view/showSubtitles");
constexpr bool defaultHideSensitive = true;
constexpr bool defaultShowSubtitles = true;
// Never in a group named "general": in a settings file Qt keeps that name
// for its own [General] section and reads the value back under "General", so
// a choice was lost at the next start (D-019).
const QString keepInTrayKey = QStringLiteral("tray/keepRunning");
const QString startMinimizedKey = QStringLiteral("start/minimized");

// Once: a choice stored under the old names is carried over and the old names
// are removed. Linux reads them back as "General/...", Windows as written.
void carryOverOldNames(QSettings& settings)
{
    const QList<std::pair<QString, QString>> moves{
        {QStringLiteral("keepRunningInTray"), keepInTrayKey},
        {QStringLiteral("startMinimized"), startMinimizedKey},
    };
    for (const auto& [oldName, newKey] : moves) {
        for (const QString& group : {QStringLiteral("General"), QStringLiteral("general")}) {
            const QString oldKey = group + QLatin1Char('/') + oldName;
            if (!settings.contains(oldKey)) {
                continue;
            }
            if (!settings.contains(newKey)) {
                settings.setValue(newKey, settings.value(oldKey));
            }
            settings.remove(oldKey);
        }
    }
}

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
        {QStringLiteral("defaultIcon"), categoryIconSource(item.categoryId)},
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

    QSettings settings;
    carryOverOldNames(settings);
    if (skipReadingSettings()) {
        // --reset: not read from the settings file at this start (D-026).
        lockMinutes = defaultAutoLockMinutes;
        clearSeconds = defaultClipboardSeconds;
        keepRunning = defaultKeepInTray;
        minimizedAtStart = defaultStartMinimized;
        hideSecrets = defaultHideSensitive;
        subtitlesShown = defaultShowSubtitles;
    } else {
        hideSecrets = settings.value(hideSensitiveKey, defaultHideSensitive).toBool();
        subtitlesShown = settings.value(showSubtitlesKey, defaultShowSubtitles).toBool();
        lockMinutes = std::clamp(settings.value(autoLockKey, defaultAutoLockMinutes).toInt(), 0, 240);
        clearSeconds = std::clamp(settings.value(clipboardKey, defaultClipboardSeconds).toInt(), 0, 600);
        keepRunning = settings.value(keepInTrayKey, defaultKeepInTray).toBool();
        minimizedAtStart = settings.value(startMinimizedKey, defaultStartMinimized).toBool();
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

    const QString last = settings.value(lastVaultKey).toString();
    vaultList = settings.value(vaultListKey).toStringList();
    if (vaultList.isEmpty() && !last.isEmpty()) {
        // Before 0.0.15 there was one vault: it is the primary (D-030).
        vaultList = {last};
        settings.setValue(vaultListKey, vaultList);
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

void VaultController::setAutoLockMinutes(int minutes)
{
    minutes = std::clamp(minutes, 0, 240);
    if (lockMinutes != minutes) {
        lockMinutes = minutes;
        QSettings().setValue(autoLockKey, minutes);
        restartAutoLock();
        emit autoLockMinutesChanged();
    }
}

void VaultController::setClipboardClearSeconds(int seconds)
{
    seconds = std::clamp(seconds, 0, 600);
    if (clearSeconds != seconds) {
        clearSeconds = seconds;
        QSettings().setValue(clipboardKey, seconds);
        emit clipboardClearSecondsChanged();
    }
}

void VaultController::setKeepInTray(bool keep)
{
    if (keepRunning != keep) {
        keepRunning = keep;
        QSettings().setValue(keepInTrayKey, keep);
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
        QSettings().setValue(startMinimizedKey, minimized);
        emit startMinimizedChanged();
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
                self->addVault(newPath);
                self->rememberKeyfile(newPath, keyfileFile);
                self->stamps->set(newPath, written);
                if (before == State::Unlocked && !savedPassword.isEmpty() && self->path == self->primaryVault()) {
                    // Its password is kept in the primary as a Password item
                    // that names the vault's file (D-030); the new vault is
                    // shown once that is on disk, so a failure is said on the
                    // primary, where it can be tried again.
                    const auto passwordTemplate = findTemplate(QStringLiteral("password"), self->contents);
                    Item saved = newItem(*passwordTemplate);
                    saved.title = tr("%1 vault").arg(QFileInfo(newPath).completeBaseName());
                    saved.fields.insert(1, makeField(QStringLiteral("vaultfile"), {}, FieldKind::Text, newPath));
                    saved.fields[0].value = savedPassword;
                    giveFieldsIds(saved);
                    self->contents.items.append(saved);
                    self->contentsChanged();
                    self->createdVault = CreatedVault{newPath, newKey, *fingerprint};
                    self->setState(State::Unlocked);
                    self->saveInBackground(QString());
                    self->rememberPrimary();
                    return;
                }
                if (before == State::Unlocked && self->path == self->primaryVault()) {
                    self->rememberPrimary();
                }
                self->showCreated(CreatedVault{newPath, newKey, *fingerprint});
            },
            Qt::QueuedConnection);
    });
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

void VaultController::unlock(const QString& password)
{
    if (currentState != State::Locked || path.isEmpty()) {
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
                self->unlocked(result.key, read.contents, *fingerprint);
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
    QPointer<VaultController> self(this);
    // On the save thread: after any write to this vault still running.
    (void)QtConcurrent::run(&savePool, [self, vaultFile, kept, attempt, fingerprint]() mutable {
        OpenResult result = openVaultFileWithKey(vaultFile, kept);
        ContentsResult read;
        if (result.status == OpenStatus::Opened) {
            read = contentsFromJson(result.contents);
            crypto::wipe(result.contents);
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, result, read, attempt, fingerprint]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    result.key.wipe();
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
    stamps->set(path, unlockedStamp);
    key = newKey;
    keyfileSecret = keyfile;
    contents = read;
    if (needKeyfile && !key.usesKeyfile) {
        needKeyfile = false;
        emit keyfileChanged();
    }
    say(QString(), false);
    takeBackUnsaved();
    contentsChanged();
    setState(State::Unlocked);
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
    // Locking locks every vault; the primary's master password unlocks them
    // again, and then the vault shown comes back (D-030).
    const QString primary = primaryVault();
    if (!primary.isEmpty() && primary != path && QFileInfo::exists(primary)) {
        returnTo = path;
        setPath(primary);
    }
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
    QSettings().remove(lastVaultKey);
    say(QString(), false);
    setState(State::NoVault);
}

QVariantMap VaultController::item(const QString& id) const
{
    for (const Item& one : contents.items) {
        if (one.id.toString(QUuid::WithoutBraces) == id) {
            return toMap(one, contents);
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
    contentsChanged();
    saveInBackground(QString());
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
        QSettings().setValue(hideSensitiveKey, hide);
        emit hideSensitiveChanged();
    }
}

void VaultController::setShowSubtitles(bool show)
{
    if (subtitlesShown != show) {
        subtitlesShown = show;
        QSettings().setValue(showSubtitlesKey, show);
        emit showSubtitlesChanged();
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
            {QStringLiteral("icon"), categoryIconSource(id)},
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
        contents.categories.append(Category{keptId, kept});
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
    return QSettings().value(keyfilesKey).toMap().value(vaultFile).toString();
}

void VaultController::rememberKeyfile(const QString& vaultFile, const QString& keyfile)
{
    if (vaultFile.isEmpty() || keyfileOf(vaultFile) == keyfile) {
        return;
    }
    QSettings settings;
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
    QSettings().setValue(vaultListKey, vaultList);
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
                    if (self->path == self->primaryVault()) {
                        self->rememberPrimary();
                    } else if (self->savedPasswords.contains(self->path) && self->primaryKey.isValid()) {
                        self->updateSavedPassword(self->path, newPassword);
                    }
                    self->say(tr("The master password was changed."), false);
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
        {QStringLiteral("clipboardClearSeconds"), defaultClipboardSeconds},
        {QStringLiteral("keepInTray"), defaultKeepInTray},
        {QStringLiteral("startMinimized"), defaultStartMinimized},
        {QStringLiteral("startAtStartup"), defaultStartAtStartup},
        {QStringLiteral("hideSensitive"), defaultHideSensitive},
        {QStringLiteral("showSubtitles"), defaultShowSubtitles},
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
        path = newPath;
        if (!path.isEmpty()) {
            QSettings().setValue(lastVaultKey, path);
        }
        needKeyfile = false;
        watch();
        emit vaultPathChanged();
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
    (void)QtConcurrent::run(&savePool, [self, vaultFile, snapshot, saveKey, itemId, box]() mutable {
        QByteArray json = contentsToJson(snapshot);
        // Changed elsewhere since this Chrono Vault read or wrote it: the
        // other version stays, this one goes into a conflict copy (D-032).
        const QByteArray expected = box->get(vaultFile);
        const QByteArray onDisk = vaultFileStamp(vaultFile);
        QString conflictPath;
        SaveStatus status = SaveStatus::CannotWrite;
        if (!expected.isEmpty() && !onDisk.isEmpty() && onDisk != expected) {
            conflictPath = conflictPathFor(vaultFile);
            // The stamp stays the old one until the other version is read, so
            // no later save goes over it before then (OR-15).
            status = saveVaultFile(conflictPath, json, saveKey);
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
            [self, status, itemId, vaultFile, conflictPath]() {
                if (!self) {
                    return;
                }
                --self->savesRunning;
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
                    self->unsavedPath.clear();
                    emit self->keptChangesChanged();
                }
                if (status == SaveStatus::Saved) {
                    if (self->savesRunning == 0 && self->currentState == State::Unlocked) {
                        self->setDirty(false);
                        self->setFailed(false);
                        if (self->trouble) {
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

void VaultController::restartAutoLock()
{
    if (currentState == State::Unlocked && lockMinutes > 0) {
        autoLockTimer.start(lockMinutes * 60 * 1000);
    } else {
        autoLockTimer.stop();
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
        contents = read.contents;
        contentsChanged();
        saveInBackground(QString());
        return;
    }
    // Changed elsewhere while it was locked: the kept change goes into a
    // conflict copy, never over the other version (D-032, P6).
    const QString vaultFile = path;
    VaultKey saveKey = key;
    ++savesRunning;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&savePool, [self, vaultFile, kept = read.contents, saveKey]() mutable {
        const QString conflictPath = conflictPathFor(vaultFile);
        QByteArray keptJson = contentsToJson(kept);
        const SaveStatus status = saveVaultFile(conflictPath, keptJson, saveKey);
        crypto::wipe(keptJson);
        saveKey.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, vaultFile, conflictPath, status]() {
                if (!self) {
                    return;
                }
                --self->savesRunning;
                if (status != SaveStatus::Saved) {
                    self->say(tr("A change kept through the lock could not be written beside the vault, which was "
                                 "changed elsewhere meanwhile: the folder cannot be written to, or the disk is full."),
                              true);
                    return;
                }
                self->addVault(conflictPath);
                self->say(tr("“%1” was changed elsewhere while it was locked. Your change was kept as “%2”.")
                              .arg(QFileInfo(vaultFile).completeBaseName(), QFileInfo(conflictPath).completeBaseName()),
                          true);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::forgetUnlockedVault()
{
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
    pendingAttachments.clear();
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
