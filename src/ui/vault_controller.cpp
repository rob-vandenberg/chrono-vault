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

#include "autostart.h"
#include "item_icons.h"
#include "password_generator.h"
#include "totp.h"
#include "version.h"

#include <QClipboard>
#include <QDateTime>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QPointer>
#include <QSettings>
#include <QVariantList>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller.cpp 0.0.6

// --- Version history ----------------------------------------------------
// v0.0.6: The settings' defaults, for Restore defaults (D-022)
// v0.0.4: The program's version, for the window titles; the General settings; item icons
// v0.0.3: Initial release

namespace chrono_vault {

namespace {

const QString lastVaultKey = QStringLiteral("vault/lastPath");
const QString autoLockKey = QStringLiteral("security/autoLockMinutes");
// The settings as they are before the person changes them, also what
// Restore defaults puts back (D-022).
constexpr int defaultAutoLockMinutes = 5;
constexpr int defaultClipboardSeconds = 30;
constexpr bool defaultKeepInTray = true;
constexpr bool defaultStartMinimized = false;
constexpr bool defaultStartAtStartup = false;
const QString clipboardKey = QStringLiteral("clipboard/clearSeconds");
const QString keepInTrayKey = QStringLiteral("general/keepRunningInTray");
const QString startMinimizedKey = QStringLiteral("general/startMinimized");

QString kindName(FieldKind kind)
{
    switch (kind) {
    case FieldKind::Text: return QStringLiteral("text");
    case FieldKind::Password: return QStringLiteral("password");
    case FieldKind::WebAddress: return QStringLiteral("url");
    case FieldKind::OneTimeCode: return QStringLiteral("totp");
    case FieldKind::MultiLine: return QStringLiteral("multiline");
    }
    return QStringLiteral("text");
}

FieldKind kindFromName(const QString& name)
{
    if (name == QLatin1String("password")) {
        return FieldKind::Password;
    }
    if (name == QLatin1String("url")) {
        return FieldKind::WebAddress;
    }
    if (name == QLatin1String("totp")) {
        return FieldKind::OneTimeCode;
    }
    if (name == QLatin1String("multiline")) {
        return FieldKind::MultiLine;
    }
    return FieldKind::Text;
}

QVariantMap toMap(const Item& item)
{
    QVariantList fields;
    for (const Field& field : item.fields) {
        fields.append(QVariantMap{
            {QStringLiteral("role"), field.role},
            {QStringLiteral("label"), field.label},
            {QStringLiteral("name"), field.isCustom() ? field.label : roleName(field.role)},
            {QStringLiteral("kind"), kindName(field.kind)},
            {QStringLiteral("value"), field.value},
        });
    }
    return {
        {QStringLiteral("id"), item.id.toString(QUuid::WithoutBraces)},
        {QStringLiteral("isNote"), item.itemTemplate == ItemTemplate::SecureNote},
        {QStringLiteral("title"), item.title},
        {QStringLiteral("fields"), fields},
        {QStringLiteral("icon"), QString::fromLatin1(item.icon.toBase64())},
    };
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

    QSettings settings;
    lockMinutes = std::clamp(settings.value(autoLockKey, defaultAutoLockMinutes).toInt(), 0, 240);
    clearSeconds = std::clamp(settings.value(clipboardKey, defaultClipboardSeconds).toInt(), 0, 600);
    keepRunning = settings.value(keepInTrayKey, defaultKeepInTray).toBool();
    minimizedAtStart = settings.value(startMinimizedKey, defaultStartMinimized).toBool();

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

    const QString last = settings.value(lastVaultKey).toString();
    if (!last.isEmpty() && QFileInfo::exists(last)) {
        setPath(last);
        currentState = State::Locked;
    }
}

VaultController::~VaultController()
{
    savePool.waitForDone();
    unlockPool.waitForDone();
    forgetUnlockedVault();
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

void VaultController::createVault(const QUrl& file, const QString& password, const QString& repeated)
{
    const QString newPath = file.isLocalFile() ? file.toLocalFile() : file.toString();
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
    (void)QtConcurrent::run(&unlockPool, [self, newPath, passwordBytes, parameters, attempt]() mutable {
        VaultKey newKey = makeVaultKey(passwordBytes, parameters);
        crypto::wipe(passwordBytes);
        const SaveStatus status = newKey.isValid() ? saveVaultFile(newPath, contentsToJson({}), newKey)
                                                   : SaveStatus::CannotWrite;
        QMetaObject::invokeMethod(
            qApp,
            [self, newPath, newKey, status, attempt]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    newKey.wipe();
                    return;
                }
                if (status != SaveStatus::Saved) {
                    newKey.wipe();
                    self->setState(self->path.isEmpty() ? State::NoVault : State::Locked);
                    self->say(tr("The new vault could not be created: the folder cannot be written to, or the "
                                 "disk is full."),
                              true);
                    return;
                }
                self->setPath(newPath);
                self->key = newKey;
                self->contents = {};
                self->listModel.setItems({});
                self->say(tr("Your new vault is ready."), false);
                self->setState(State::Unlocked);
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
    forgetUnlockedVault();
    ++unlockAttempt; // a create or unlock still running no longer counts
    setPath(chosen);
    say(QString(), false);
    setState(State::Locked);
}

void VaultController::unlock(const QString& password)
{
    if (currentState != State::Locked || path.isEmpty()) {
        return;
    }
    setState(State::Busy);
    say(tr("Unlocking…"), false);
    QByteArray passwordBytes = password.toUtf8();
    const QString vaultFile = path;
    const quint64 attempt = ++unlockAttempt;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&unlockPool, [self, vaultFile, passwordBytes, attempt]() mutable {
        OpenResult result = openVaultFile(vaultFile, passwordBytes);
        crypto::wipe(passwordBytes);
        ContentsResult read;
        if (result.status == OpenStatus::Opened) {
            read = contentsFromJson(result.contents);
            crypto::wipe(result.contents);
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, result, read, attempt]() mutable {
                if (!self || attempt != self->unlockAttempt) {
                    result.key.wipe();
                    return;
                }
                if (result.status != OpenStatus::Opened) {
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
                self->key = result.key;
                self->contents = read.contents;
                self->say(QString(), false);
                self->takeBackUnsaved();
                self->listModel.setItems(self->contents.items);
                self->setState(State::Unlocked);
            },
            Qt::QueuedConnection);
    });
}

void VaultController::lock()
{
    if (currentState != State::Unlocked) {
        return;
    }
    emit locking();
    keepUnsavedWhileLocked();
    forgetUnlockedVault();
    say(QString(), false);
    setState(State::Locked);
}

void VaultController::closeVault()
{
    forgetUnlockedVault();
    ++unlockAttempt;
    // The window has asked the person before this (OR-15).
    if (!unsavedSealed.isEmpty()) {
        unsavedSealed.clear();
        unsavedPath.clear();
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
            return toMap(one);
        }
    }
    return {};
}

QVariantMap VaultController::newLogin() const
{
    return toMap(newItem(ItemTemplate::Login));
}

QVariantMap VaultController::newSecureNote() const
{
    return toMap(newItem(ItemTemplate::SecureNote));
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
    Item changed;
    changed.id = id;
    changed.itemTemplate =
        map.value(QStringLiteral("isNote")).toBool() ? ItemTemplate::SecureNote : ItemTemplate::Login;
    changed.title = map.value(QStringLiteral("title")).toString().trimmed();
    if (changed.title.isEmpty()) {
        changed.title = changed.itemTemplate == ItemTemplate::SecureNote ? tr("Untitled note") : tr("Untitled login");
    }
    for (const QVariant& value : map.value(QStringLiteral("fields")).toList()) {
        const QVariantMap fieldMap = value.toMap();
        Field field;
        field.role = fieldMap.value(QStringLiteral("role")).toString();
        field.label = field.role.isEmpty() ? fieldMap.value(QStringLiteral("label")).toString().trimmed() : QString();
        if (field.role.isEmpty() && field.label.isEmpty()) {
            field.label = tr("Field");
        }
        field.kind = kindFromName(fieldMap.value(QStringLiteral("kind")).toString());
        field.value = fieldMap.value(QStringLiteral("value")).toString();
        changed.fields.append(field);
    }
    changed.icon = QByteArray::fromBase64(map.value(QStringLiteral("icon")).toString().toLatin1());
    changed.modified = QDateTime::currentDateTimeUtc();

    const auto existing = std::find_if(contents.items.begin(), contents.items.end(),
                                       [&id](const Item& one) { return one.id == id; });
    if (existing != contents.items.end()) {
        changed.created = existing->created;
        *existing = changed;
    } else {
        changed.created = changed.modified;
        contents.items.append(changed);
    }
    listModel.setItems(contents.items);
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
    listModel.setItems(contents.items);
    saveInBackground(QString());
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
        emit vaultPathChanged();
    }
}

void VaultController::saveInBackground(const QString& itemId)
{
    const QByteArray json = contentsToJson(contents);
    const QString vaultFile = path;
    VaultKey saveKey = key;
    ++savesRunning;
    setDirty(true);
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run(&savePool, [self, vaultFile, json, saveKey, itemId]() mutable {
        const SaveStatus status = saveVaultFile(vaultFile, json, saveKey);
        saveKey.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, status, itemId, vaultFile]() {
                if (!self) {
                    return;
                }
                --self->savesRunning;
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
    if (read.status == ContentsStatus::Read) {
        contents = read.contents;
        saveInBackground(QString());
    }
}

void VaultController::forgetUnlockedVault()
{
    setDirty(false);
    setFailed(false);
    key.wipe();
    contents = {};
    listModel.clear();
    if (!search.isEmpty()) {
        search.clear();
        emit searchTextChanged();
    }
}

} // namespace chrono_vault
