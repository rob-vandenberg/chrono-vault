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


#include "webdav_sync.h"

#include "quick_unlock.h"
#include "settings_files.h"
#include "vault_contents.h"
#include "vault_merge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

// --- Version ------------------------------------------------------------
// webdav_sync.cpp 0.3.61

// --- Version history ----------------------------------------------------
// v0.3.61: A lock's empty placeholder is no version (pCloud); every step in the --debug log (D-057)
// v0.3.60: Initial release (D-057)

namespace chrono_vault {

namespace {

const QString placesKey = QStringLiteral("vaults/webdav");
constexpr int mostRounds = 3;
constexpr qsizetype stampLength = 52; // header and nonce, as vaultFileStamp reads it

std::optional<QByteArray> readWhole(const QString& path)
{
    QFile file(path);
    if (file.size() > largestVaultFileBytes || !file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QByteArray data = file.read(largestVaultFileBytes + 1);
    if (data.size() > largestVaultFileBytes) {
        return std::nullopt;
    }
    return data;
}

std::optional<VaultContents> contentsOf(const OpenResult& opened)
{
    if (opened.status != OpenStatus::Opened) {
        return std::nullopt;
    }
    ContentsResult read = contentsFromJson(opened.contents);
    if (read.status != ContentsStatus::Read) {
        return std::nullopt;
    }
    return std::move(read.contents);
}

// The same contents, whatever the order of the items.
bool sameContents(VaultContents one, VaultContents other)
{
    const auto byId = [](const Item& left, const Item& right) { return left.id.toString() < right.id.toString(); };
    std::sort(one.items.begin(), one.items.end(), byId);
    std::sort(other.items.begin(), other.items.end(), byId);
    QByteArray oneJson = contentsToJson(one);
    QByteArray otherJson = contentsToJson(other);
    const bool same = oneJson == otherJson;
    crypto::wipe(oneJson);
    crypto::wipe(otherJson);
    return same;
}

QString conflictCopyFor(const QString& workingCopy)
{
    const QFileInfo info(workingCopy);
    const QString when = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH.mm"));
    const QString start =
        info.absolutePath() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(" (conflict ") + when;
    QString candidate = start + QStringLiteral(").chronovault");
    for (int number = 2; QFileInfo::exists(candidate); ++number) {
        candidate = start + QStringLiteral(" %1).chronovault").arg(number);
    }
    return candidate;
}

} // namespace

QVariantMap WebDavPlace::toMap() const
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("folder"), folder.toString(QUrl::RemoveUserInfo)},
        {QStringLiteral("file"), fileName},
        {QStringLiteral("user"), user},
        {QStringLiteral("certificate"), QString::fromLatin1(trustedCertificate)},
        {QStringLiteral("tag"), QString::fromLatin1(tag.toBase64())},
        {QStringLiteral("stamp"), QString::fromLatin1(syncedStamp.toBase64())},
        {QStringLiteral("synced"), lastSynced.toUTC().toString(Qt::ISODate)},
    };
}

WebDavPlace WebDavPlace::fromMap(const QVariantMap& map)
{
    WebDavPlace place;
    place.id = map.value(QStringLiteral("id")).toString();
    place.folder = QUrl(map.value(QStringLiteral("folder")).toString());
    place.fileName = map.value(QStringLiteral("file")).toString();
    place.user = map.value(QStringLiteral("user")).toString();
    place.trustedCertificate = map.value(QStringLiteral("certificate")).toString().toLatin1();
    place.tag = QByteArray::fromBase64(map.value(QStringLiteral("tag")).toString().toLatin1());
    place.syncedStamp = QByteArray::fromBase64(map.value(QStringLiteral("stamp")).toString().toLatin1());
    place.lastSynced = QDateTime::fromString(map.value(QStringLiteral("synced")).toString(), Qt::ISODate);
    return place;
}

QString WebDavPlace::shownAddress() const
{
    return folderAddress(folder).resolved(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(fileName))))
        .toDisplayString(QUrl::RemoveUserInfo);
}

std::optional<WebDavPlace> webDavPlaceOf(const QString& workingCopy)
{
    const QVariantMap all = VaultsFile().value(placesKey).toMap();
    if (workingCopy.isEmpty() || !all.contains(workingCopy)) {
        return std::nullopt;
    }
    WebDavPlace place = WebDavPlace::fromMap(all.value(workingCopy).toMap());
    if (place.id.isEmpty() || !place.folder.isValid() || place.fileName.isEmpty()) {
        return std::nullopt; // damaged settings are not a place (OR-4)
    }
    return place;
}

void rememberWebDavPlace(const QString& workingCopy, const WebDavPlace& place)
{
    VaultsFile settings;
    QVariantMap all = settings.value(placesKey).toMap();
    all.insert(workingCopy, place.toMap());
    settings.setValue(placesKey, all);
}

void forgetWebDavPlace(const QString& workingCopy)
{
    VaultsFile settings;
    QVariantMap all = settings.value(placesKey).toMap();
    all.remove(workingCopy);
    settings.setValue(placesKey, all);
}

QString webDavDataFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/chrono-vault/webdav");
}

QString webDavBasePath(const QString& id)
{
    return webDavDataFolder() + QStringLiteral("/") + id + QStringLiteral(".base.chronovault");
}

QString newWebDavId()
{
    return QString::number(QRandomGenerator::system()->generate64(), 16);
}

QString syncOutcomeText(SyncOutcome outcome)
{
    switch (outcome) {
    case SyncOutcome::Synced:
        return QString();
    case SyncOutcome::NoConnection:
        return QCoreApplication::translate("WebDavSync", "the server cannot be reached");
    case SyncOutcome::Unauthorized:
        return QCoreApplication::translate("WebDavSync", "the user name or password is wrong");
    case SyncOutcome::NotFound:
        return QCoreApplication::translate("WebDavSync", "the folder is not found on the server");
    case SyncOutcome::Certificate:
        return QCoreApplication::translate("WebDavSync", "the server's certificate is not trusted");
    case SyncOutcome::OtherPassword:
        return QCoreApplication::translate("WebDavSync", "the vault on the server has another master password");
    case SyncOutcome::Unreadable:
        return QCoreApplication::translate("WebDavSync", "the vault on the server cannot be read");
    case SyncOutcome::Busy:
        return QCoreApplication::translate("WebDavSync", "another device is saving it; tried again soon");
    case SyncOutcome::TooLarge:
        return QCoreApplication::translate("WebDavSync", "the vault is too large for the server");
    case SyncOutcome::Server:
        return QCoreApplication::translate("WebDavSync", "the server refused it");
    case SyncOutcome::LocalTrouble:
        return QCoreApplication::translate("WebDavSync", "the vault on this device cannot be read or written");
    case SyncOutcome::Interrupted:
        return QCoreApplication::translate("WebDavSync", "stopped");
    }
    return QString();
}

struct WebDavSync::Prepared
{
    SyncOutcome outcome = SyncOutcome::Synced;
    bool wroteWorkingCopy = false;
    QByteArray upload;        // what goes to the server; empty when nothing
    QByteArray stamp;         // the working copy's stamp, as uploaded or taken
    QString conflictCopy;
};

WebDavSync::WebDavSync(QString workingCopy, WebDavPlace place, QString password, const VaultKey& vaultKey,
                       QThreadPool* savePool, QObject* parent)
    : QObject(parent), copy(std::move(workingCopy)), where(std::move(place)), key(vaultKey), pool(savePool)
{
    key.key = QByteArray(vaultKey.key.constData(), vaultKey.key.size()); // its own copy, wiped on its own
    client = new WebDavClient(WebDavAccount{where.folder, where.user, password, where.trustedCertificate}, this);
}

WebDavSync::~WebDavSync()
{
    key.wipe();
}

QString WebDavSync::uploadName() const
{
    return where.fileName + QStringLiteral(".") + where.id + QStringLiteral(".uploading");
}

bool WebDavSync::goOn(SyncStep step)
{
    if (stepCheck && !stepCheck(step)) {
        finish(SyncOutcome::Interrupted);
        return false;
    }
    return true;
}

void WebDavSync::start()
{
    if (busy) {
        again = true;
        return;
    }
    busy = true;
    rounds = 0;
    round();
}

void WebDavSync::round()
{
    ++rounds;
    remoteMissing = false;
    theirs.clear();
    theirTag = where.tag;
    uploading.clear();
    QPointer<WebDavSync> self(this);
    client->get(where.fileName, where.tag, [self](const WebDavReply& answer) {
        if (self) {
            self->downloaded(answer);
        }
    });
}

void WebDavSync::downloaded(const WebDavReply& answer)
{
    fingerLog(QStringLiteral("webdav sync: round %1 downloaded: status %2, tag '%3', known tag '%4', %5 bytes")
                  .arg(rounds)
                  .arg(answer.status)
                  .arg(QString::fromLatin1(answer.tag), QString::fromLatin1(where.tag))
                  .arg(answer.body.size()));
    if (answer.error == WebDavError::NotFound) {
        remoteMissing = true; // first upload, or removed there: this device's version goes up
    } else if (!answer.ok()) {
        finishFrom(answer);
        return;
    } else if (answer.status != 304 && !(answer.tag == where.tag && !answer.tag.isEmpty())) {
        theirs = answer.body;
        theirTag = answer.tag;
    }
    if (!goOn(SyncStep::Downloaded)) {
        return;
    }
    // Reading, merging and writing the working copy happen on the save
    // thread, after any save of the window's (OR-5, D-032 point 7).
    QPointer<WebDavSync> self(this);
    VaultKey useKey = key;
    useKey.key = QByteArray(key.key.constData(), key.key.size());
    (void)QtConcurrent::run(pool, [self, workingCopy = copy, base = webDavBasePath(where.id), their = theirs,
                                   missing = remoteMissing, synced = where.syncedStamp, useKey]() mutable {
        Prepared ready;
        const auto finishWith = [&](SyncOutcome outcome) {
            ready.outcome = outcome;
            useKey.wipe();
            QMetaObject::invokeMethod(
                qApp, [self, ready]() { if (self) { self->prepared(ready); } }, Qt::QueuedConnection);
        };
        const auto mineBytes = readWhole(workingCopy);
        if (!mineBytes) {
            finishWith(SyncOutcome::LocalTrouble);
            return;
        }
        if (their.isEmpty()) {
            ready.stamp = mineBytes->left(stampLength);
            if (missing || ready.stamp != synced) {
                // Opened once to be sure only a whole vault ever goes up.
                OpenResult check = openVaultDataWithKey(*mineBytes, useKey);
                crypto::wipe(check.contents);
                check.key.wipe();
                if (check.status != OpenStatus::Opened) {
                    finishWith(SyncOutcome::LocalTrouble);
                    return;
                }
                ready.upload = *mineBytes;
            }
            finishWith(SyncOutcome::Synced);
            return;
        }
        OpenResult theirOpened = openVaultDataWithKey(their, useKey);
        theirOpened.key.wipe();
        if (theirOpened.status == OpenStatus::KeyChanged && readWhole(base).value_or(QByteArray()) == their) {
            // The server still holds the version both last agreed on: the
            // master password was changed here, and this version goes up.
            crypto::wipe(theirOpened.contents);
            OpenResult check = openVaultDataWithKey(*mineBytes, useKey);
            crypto::wipe(check.contents);
            check.key.wipe();
            if (check.status != OpenStatus::Opened) {
                finishWith(SyncOutcome::LocalTrouble);
                return;
            }
            ready.upload = *mineBytes;
            ready.stamp = mineBytes->left(stampLength);
            finishWith(SyncOutcome::Synced);
            return;
        }
        if (theirOpened.status == OpenStatus::KeyChanged) {
            // Given another master password elsewhere: this device's version
            // is kept beside it as a conflict copy, the server's becomes the
            // working copy, and the vault locks (D-057, as D-032 point 3).
            crypto::wipe(theirOpened.contents);
            const QString conflict = conflictCopyFor(workingCopy);
            if (saveVaultData(conflict, *mineBytes) != SaveStatus::Saved ||
                saveVaultData(workingCopy, their) != SaveStatus::Saved) {
                finishWith(SyncOutcome::LocalTrouble);
                return;
            }
            (void)saveVaultData(base, their);
            ready.conflictCopy = conflict;
            ready.wroteWorkingCopy = true;
            ready.stamp = their.left(stampLength);
            finishWith(SyncOutcome::OtherPassword);
            return;
        }
        const auto theirContents = contentsOf(theirOpened);
        crypto::wipe(theirOpened.contents);
        if (!theirContents) {
            finishWith(SyncOutcome::Unreadable); // nothing goes over a version not understood (OR-15)
            return;
        }
        OpenResult mineOpened = openVaultDataWithKey(*mineBytes, useKey);
        mineOpened.key.wipe();
        const auto mineContents = contentsOf(mineOpened);
        crypto::wipe(mineOpened.contents);
        if (!mineContents) {
            finishWith(SyncOutcome::LocalTrouble);
            return;
        }
        OpenResult baseOpened = openVaultFileWithKey(base, useKey);
        baseOpened.key.wipe();
        const auto baseContents = contentsOf(baseOpened);
        crypto::wipe(baseOpened.contents);

        // Nothing changed here since the last sync: the server's version is
        // taken as it is. Otherwise merged (D-054), and uploaded only when
        // the merge holds something the server's version lacks.
        const bool unchangedHere = baseContents && sameContents(*mineContents, *baseContents);
        const MergeResult merged = unchangedHere ? MergeResult{*theirContents, 0, 0}
                                                 : mergeContents(baseContents ? &*baseContents : nullptr,
                                                                 *mineContents, *theirContents,
                                                                 QDateTime::currentDateTimeUtc());
        const bool sameAsTheirs = unchangedHere || sameContents(merged.contents, *theirContents);
        QByteArray mergedJson = contentsToJson(merged.contents);
        if (sameAsTheirs) {
            // Nothing here the server lacks: its version, as it is, becomes
            // the working copy and the base; nothing goes up.
            crypto::wipe(mergedJson);
            if (saveVaultData(workingCopy, their) != SaveStatus::Saved) {
                finishWith(SyncOutcome::LocalTrouble);
                return;
            }
            ready.wroteWorkingCopy = true;
            (void)saveVaultData(base, their);
            ready.stamp = their.left(stampLength);
            finishWith(SyncOutcome::Synced);
            return;
        }
        QByteArray written;
        const SaveStatus saved = saveVaultFile(workingCopy, mergedJson, useKey, {}, &written);
        crypto::wipe(mergedJson);
        if (saved != SaveStatus::Saved) {
            finishWith(SyncOutcome::LocalTrouble);
            return;
        }
        ready.wroteWorkingCopy = true;
        const auto mergedBytes = readWhole(workingCopy);
        if (!mergedBytes) {
            finishWith(SyncOutcome::LocalTrouble);
            return;
        }
        ready.upload = *mergedBytes;
        ready.stamp = mergedBytes->left(stampLength);
        finishWith(SyncOutcome::Synced);
    });
}

void WebDavSync::prepared(const Prepared& ready)
{
    fingerLog(QStringLiteral("webdav sync: prepared: outcome %1, wrote working copy %2, upload %3 bytes, missing %4")
                  .arg(static_cast<int>(ready.outcome))
                  .arg(ready.wroteWorkingCopy)
                  .arg(ready.upload.size())
                  .arg(remoteMissing));
    if (ready.wroteWorkingCopy) {
        emit workingCopyChanged();
    }
    if (ready.outcome != SyncOutcome::Synced) {
        if (ready.outcome == SyncOutcome::OtherPassword) {
            where.tag = theirTag;
            where.syncedStamp = ready.stamp;
            where.lastSynced = QDateTime::currentDateTimeUtc();
            rememberWebDavPlace(copy, where);
        }
        finish(ready.outcome, {}, ready.conflictCopy);
        return;
    }
    if (!goOn(SyncStep::Merged)) {
        return;
    }
    if (ready.upload.isEmpty()) {
        where.tag = theirTag;
        where.syncedStamp = ready.stamp;
        recordSynced(theirTag);
        return;
    }
    uploading = ready.upload;
    upload();
}

void WebDavSync::upload()
{
    QPointer<WebDavSync> self(this);
    client->put(uploadName(), uploading, [self](const WebDavReply& answer) {
        if (!self) {
            return;
        }
        if (!answer.ok()) {
            self->finishFrom(answer);
            return;
        }
        if (!self->goOn(SyncStep::Uploaded)) {
            return;
        }
        self->client->get(self->uploadName(), {}, [self](const WebDavReply& readBack) {
            if (self) {
                self->verifyUpload(readBack);
            }
        });
    });
}

void WebDavSync::verifyUpload(const WebDavReply& answer)
{
    if (!answer.ok()) {
        finishFrom(answer);
        return;
    }
    if (answer.body != uploading) {
        client->remove(uploadName(), [](const WebDavReply&) {});
        finish(SyncOutcome::Server); // the server did not keep what was sent: the vault there is untouched
        return;
    }
    // The copy on the server opens (OR-15.2); decrypting is off the window's
    // thread (OR-5).
    QPointer<WebDavSync> self(this);
    VaultKey useKey = key;
    useKey.key = QByteArray(key.key.constData(), key.key.size());
    (void)QtConcurrent::run(pool, [self, data = answer.body, useKey]() mutable {
        OpenResult opened = openVaultDataWithKey(data, useKey);
        const bool opens = opened.status == OpenStatus::Opened;
        crypto::wipe(opened.contents);
        opened.key.wipe();
        useKey.wipe();
        QMetaObject::invokeMethod(
            qApp,
            [self, opens]() {
                if (!self) {
                    return;
                }
                if (!opens) {
                    self->client->remove(self->uploadName(), [](const WebDavReply&) {});
                    self->finish(SyncOutcome::LocalTrouble);
                    return;
                }
                if (self->goOn(SyncStep::Verified)) {
                    self->lockForMove();
                }
            },
            Qt::QueuedConnection);
    });
}

void WebDavSync::lockForMove()
{
    QPointer<WebDavSync> self(this);
    client->lock(where.fileName, [self](const WebDavReply& answer) {
        if (!self) {
            return;
        }
        if (answer.error == WebDavError::Locked) {
            self->client->remove(self->uploadName(), [](const WebDavReply&) {});
            self->finish(SyncOutcome::Busy);
            return;
        }
        if (answer.error == WebDavError::NoConnection || answer.error == WebDavError::Unauthorized ||
            answer.error == WebDavError::Certificate) {
            self->finishFrom(answer);
            return;
        }
        // A server without locks: the tag is checked right before the move.
        const QByteArray token = answer.ok() ? answer.body : QByteArray();
        fingerLog(QStringLiteral("webdav sync: lock: error %1, token %2")
                      .arg(static_cast<int>(answer.error))
                      .arg(token.isEmpty() ? QStringLiteral("none") : QStringLiteral("given")));
        if (self->goOn(SyncStep::Locked)) {
            self->checkTag(token);
        }
    });
}

void WebDavSync::checkTag(const QByteArray& lockToken)
{
    QPointer<WebDavSync> self(this);
    client->tagOf(where.fileName, [self, lockToken](const WebDavReply& answer) {
        if (!self) {
            return;
        }
        const bool there = answer.ok();
        if (!there && answer.error != WebDavError::NotFound) {
            if (!lockToken.isEmpty()) {
                self->client->unlock(self->where.fileName, lockToken, [](const WebDavReply&) {});
            }
            self->finishFrom(answer);
            return;
        }
        if (self->remoteMissing && there) {
            // Listed, though missing when this round began: a lock on a
            // missing file may leave an empty placeholder (pCloud lists one
            // with no tag, date or size, and answers 404 to reading it).
            // Only content read back is another device's version.
            self->client->get(self->where.fileName, {}, [self, lockToken](const WebDavReply& again) {
                if (!self) {
                    return;
                }
                const bool aVersion = again.ok() && !again.body.isEmpty();
                fingerLog(QStringLiteral("webdav sync: placeholder check: status %1, %2 bytes, a version %3")
                              .arg(again.status)
                              .arg(again.body.size())
                              .arg(aVersion));
                if (!aVersion && !again.ok() && again.error != WebDavError::NotFound) {
                    if (!lockToken.isEmpty()) {
                        self->client->unlock(self->where.fileName, lockToken, [](const WebDavReply&) {});
                    }
                    self->finishFrom(again);
                    return;
                }
                if (aVersion) {
                    self->changedMeanwhile(lockToken);
                } else {
                    self->moveIntoPlace(lockToken);
                }
            });
            return;
        }
        // The tag must be the one this round started from.
        if (!self->remoteMissing && there && answer.tag.isEmpty() && self->theirTag.isEmpty()) {
            // A server that gives no tags at all: its file is read again and
            // compared with the one this round merged with.
            self->client->get(self->where.fileName, {}, [self, lockToken](const WebDavReply& again) {
                if (!self) {
                    return;
                }
                if (again.ok() && again.body == self->theirs) {
                    self->moveIntoPlace(lockToken);
                    return;
                }
                self->changedMeanwhile(lockToken);
            });
            return;
        }
        const bool changed =
            !self->remoteMissing && (!there || answer.tag.isEmpty() || answer.tag != self->theirTag);
        fingerLog(QStringLiteral("webdav sync: tag check: there %1, missing before %2, size %3, tag '%4', expected '%5', "
                                 "changed %6")
                      .arg(there)
                      .arg(self->remoteMissing)
                      .arg(answer.size)
                      .arg(QString::fromLatin1(answer.tag), QString::fromLatin1(self->theirTag))
                      .arg(changed));
        if (changed) {
            self->changedMeanwhile(lockToken);
            return;
        }
        self->moveIntoPlace(lockToken);
    });
}

void WebDavSync::changedMeanwhile(const QByteArray& lockToken)
{
    // Another device put a version there meanwhile: never replaced; read and
    // merged first, in a new round (OR-15.3).
    if (!lockToken.isEmpty()) {
        client->unlock(where.fileName, lockToken, [](const WebDavReply&) {});
    }
    if (rounds < mostRounds) {
        round();
    } else {
        client->remove(uploadName(), [](const WebDavReply&) {});
        finish(SyncOutcome::Busy);
    }
}

void WebDavSync::moveIntoPlace(const QByteArray& lockToken)
{
    QPointer<WebDavSync> self(this);
    client->move(uploadName(), where.fileName, lockToken, [self, lockToken](const WebDavReply& answer) {
        if (!self) {
            return;
        }
        fingerLog(QStringLiteral("webdav sync: move: error %1, status %2").arg(static_cast<int>(answer.error)).arg(answer.status));
        if (!answer.ok()) {
            if (!lockToken.isEmpty()) {
                self->client->unlock(self->where.fileName, lockToken, [](const WebDavReply&) {});
            }
            self->finishFrom(answer);
            return;
        }
        if (!self->goOn(SyncStep::Moved)) {
            return;
        }
        self->client->tagOf(self->where.fileName, [self, lockToken](const WebDavReply& tagged) {
            if (!self) {
                return;
            }
            if (!lockToken.isEmpty()) {
                self->client->unlock(self->where.fileName, lockToken, [](const WebDavReply&) {});
            }
            // Without a tag the next sync reads the server's version once
            // more and finds nothing new: nothing is lost either way.
            self->where.syncedStamp = self->uploading.left(stampLength);
            const QByteArray uploaded = self->uploading;
            const QString base = webDavBasePath(self->where.id);
            const QByteArray newTag = tagged.ok() ? tagged.tag : QByteArray();
            (void)QtConcurrent::run(self->pool, [self, uploaded, base, newTag]() {
                (void)saveVaultData(base, uploaded);
                QMetaObject::invokeMethod(
                    qApp,
                    [self, newTag]() {
                        if (self && self->goOn(SyncStep::BaseWritten)) {
                            self->recordSynced(newTag);
                        }
                    },
                    Qt::QueuedConnection);
            });
        });
    });
}

void WebDavSync::recordSynced(const QByteArray& newTag)
{
    where.tag = newTag;
    where.lastSynced = QDateTime::currentDateTimeUtc();
    rememberWebDavPlace(copy, where);
    finish(SyncOutcome::Synced);
}

void WebDavSync::finishFrom(const WebDavReply& answer)
{
    switch (answer.error) {
    case WebDavError::None:
        finish(SyncOutcome::Synced);
        return;
    case WebDavError::NoConnection:
        finish(SyncOutcome::NoConnection);
        return;
    case WebDavError::Unauthorized:
        finish(SyncOutcome::Unauthorized);
        return;
    case WebDavError::NotFound:
        finish(SyncOutcome::NotFound);
        return;
    case WebDavError::Certificate:
        finish(SyncOutcome::Certificate, answer.certificate);
        return;
    case WebDavError::Changed:
    case WebDavError::Locked:
        finish(SyncOutcome::Busy);
        return;
    case WebDavError::TooLarge:
        finish(SyncOutcome::TooLarge);
        return;
    case WebDavError::NotSupported:
    case WebDavError::Damaged:
    case WebDavError::Server:
        finish(SyncOutcome::Server);
        return;
    }
    finish(SyncOutcome::Server);
}

void WebDavSync::finish(SyncOutcome outcome, const QByteArray& certificate, const QString& conflictCopy)
{
    fingerLog(QStringLiteral("webdav sync: finished: outcome %1 after %2 rounds").arg(static_cast<int>(outcome)).arg(rounds));
    busy = false;
    uploading.clear();
    theirs.clear();
    emit finished(outcome, certificate, conflictCopy);
    if (again && outcome != SyncOutcome::Interrupted) {
        again = false;
        start();
    }
}

} // namespace chrono_vault
