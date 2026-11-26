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


#include "cloud_sync.h"

#include "quick_unlock.h"
#include "settings_files.h"
#include "vault_contents.h"
#include "vault_merge.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>

// --- Version ------------------------------------------------------------
// cloud_sync.cpp 0.8.115

// --- Version history ----------------------------------------------------
// v0.8.115: A version that came in at the same moment is merged in a new round (D-080)
// v0.8.114: Initial release: WebDavSync of webdav_sync.cpp 0.5.78, for any cloud storage (D-080)

namespace chrono_vault {

namespace {

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

// The --debug log names the storage: "webdav sync: …" (D-057).
QString logPrefix(const CloudPlace& place)
{
    return place.kind + QStringLiteral(" sync: ");
}

} // namespace

QString syncOutcomeText(SyncOutcome outcome, const QString& kind)
{
    if (kind == QLatin1String("googledrive")) {
        switch (outcome) {
        case SyncOutcome::NoConnection:
            return QCoreApplication::translate("WebDavSync", "Google Drive cannot be reached");
        case SyncOutcome::Unauthorized:
            return QCoreApplication::translate("WebDavSync", "the sign-in to Google Drive must be renewed");
        case SyncOutcome::NotFound:
            return QCoreApplication::translate("WebDavSync", "the vault's folder is not found on Google Drive");
        case SyncOutcome::TooLarge:
            return QCoreApplication::translate("WebDavSync", "Google Drive is full, or the vault is too large for it");
        case SyncOutcome::Server:
            return QCoreApplication::translate("WebDavSync", "Google Drive refused it");
        default:
            break;
        }
    }
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

struct CloudSync::Prepared
{
    SyncOutcome outcome = SyncOutcome::Synced;
    bool wroteWorkingCopy = false;
    QByteArray upload;        // what goes to the server; empty when nothing
    QByteArray stamp;         // the working copy's stamp, as uploaded or taken
    QString conflictCopy;
    bool theirsUnchanged = false; // the server's bytes are the base's, under a new tag
};

CloudSync::CloudSync(QString workingCopy, CloudPlace place, std::unique_ptr<CloudStorage> storage,
                     const VaultKey& vaultKey, QThreadPool* savePool, QObject* parent)
    : QObject(parent), copy(std::move(workingCopy)), where(std::move(place)), key(vaultKey), pool(savePool),
      client(std::move(storage))
{
    key.key = QByteArray(vaultKey.key.constData(), vaultKey.key.size()); // its own copy, wiped on its own
}

CloudSync::~CloudSync()
{
    key.wipe();
}

QString CloudSync::uploadName() const
{
    return where.fileName + QStringLiteral(".") + where.id + QStringLiteral(".uploading");
}

bool CloudSync::goOn(SyncStep step)
{
    if (stepCheck && !stepCheck(step)) {
        finish(SyncOutcome::Interrupted);
        return false;
    }
    return true;
}

void CloudSync::start()
{
    if (busy) {
        again = true;
        return;
    }
    busy = true;
    rounds = 0;
    // An upload of this vault left by an earlier sync that was stopped goes
    // first (the owner: clean up after yourself); one of another device's
    // is never touched, it may still be uploading.
    QPointer<CloudSync> self(this);
    client->remove(uploadName(), [self](const CloudReply& answer) {
        if (self) {
            fingerLog(logPrefix(self->where) + QStringLiteral("leftover upload removed: status %1").arg(answer.status));
            self->round();
        }
    });
}

void CloudSync::round()
{
    ++rounds;
    remoteMissing = false;
    theirs.clear();
    theirTag = where.tag;
    uploading.clear();
    QPointer<CloudSync> self(this);
    client->get(where.fileName, where.tag, [self](const CloudReply& answer) {
        if (self) {
            self->downloaded(answer);
        }
    });
}

void CloudSync::downloaded(const CloudReply& answer)
{
    fingerLog(logPrefix(where) + QStringLiteral("round %1 downloaded: status %2, tag '%3', known tag '%4', %5 bytes")
                  .arg(rounds)
                  .arg(answer.status)
                  .arg(QString::fromLatin1(answer.tag), QString::fromLatin1(where.tag))
                  .arg(answer.body.size()));
    if (answer.error == CloudError::NotFound) {
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
    QPointer<CloudSync> self(this);
    VaultKey useKey = key;
    useKey.key = QByteArray(key.key.constData(), key.key.size());
    (void)QtConcurrent::run(pool, [self, workingCopy = copy, base = cloudBasePath(where), their = theirs,
                                   missing = remoteMissing, synced = where.syncedStamp, useKey,
                                   device = thisDeviceId()]() mutable {
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
        if (readWhole(base).value_or(QByteArray()) == their) {
            // The version both last agreed on, under a new tag (pCloud gives
            // one soon after an upload): nothing changed there.
            ready.theirsUnchanged = true;
            ready.stamp = mineBytes->left(stampLength);
            if (ready.stamp != synced) {
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
                                                                 QDateTime::currentDateTimeUtc(), device);
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

void CloudSync::prepared(const Prepared& ready)
{
    fingerLog(logPrefix(where) + QStringLiteral("prepared: outcome %1, wrote working copy %2, upload %3 bytes, missing %4, "
                             "server unchanged under a new tag %5")
                  .arg(static_cast<int>(ready.outcome))
                  .arg(ready.wroteWorkingCopy)
                  .arg(ready.upload.size())
                  .arg(remoteMissing)
                  .arg(ready.theirsUnchanged));
    if (ready.wroteWorkingCopy) {
        emit workingCopyChanged();
    }
    if (ready.outcome != SyncOutcome::Synced) {
        if (ready.outcome == SyncOutcome::OtherPassword) {
            where.tag = theirTag;
            where.syncedStamp = ready.stamp;
            where.lastSynced = QDateTime::currentDateTimeUtc();
            rememberCloudPlace(copy, where);
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

void CloudSync::upload()
{
    QPointer<CloudSync> self(this);
    client->put(uploadName(), uploading, [self](const CloudReply& answer) {
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
        self->client->get(self->uploadName(), {}, [self](const CloudReply& readBack) {
            if (self) {
                self->verifyUpload(readBack);
            }
        });
    });
}

void CloudSync::verifyUpload(const CloudReply& answer)
{
    if (!answer.ok()) {
        finishFrom(answer);
        return;
    }
    if (answer.body != uploading) {
        client->remove(uploadName(), [](const CloudReply&) {});
        finish(SyncOutcome::Server); // the server did not keep what was sent: the vault there is untouched
        return;
    }
    // The copy on the server opens (OR-15.2); decrypting is off the window's
    // thread (OR-5).
    QPointer<CloudSync> self(this);
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
                    self->client->remove(self->uploadName(), [](const CloudReply&) {});
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

void CloudSync::lockForMove()
{
    QPointer<CloudSync> self(this);
    client->lock(where.fileName, [self](const CloudReply& answer) {
        if (!self) {
            return;
        }
        if (answer.error == CloudError::Locked) {
            self->client->remove(self->uploadName(), [](const CloudReply&) {});
            self->finish(SyncOutcome::Busy);
            return;
        }
        if (answer.error == CloudError::NoConnection || answer.error == CloudError::Unauthorized ||
            answer.error == CloudError::Certificate) {
            self->finishFrom(answer);
            return;
        }
        // A server without locks: the tag is checked right before the move.
        const QByteArray token = answer.ok() ? answer.body : QByteArray();
        fingerLog(logPrefix(self->where) + QStringLiteral("lock: error %1, token %2")
                      .arg(static_cast<int>(answer.error))
                      .arg(token.isEmpty() ? QStringLiteral("none") : QStringLiteral("given")));
        if (self->goOn(SyncStep::Locked)) {
            self->checkTag(token);
        }
    });
}

void CloudSync::checkTag(const QByteArray& lockToken)
{
    QPointer<CloudSync> self(this);
    client->tagOf(where.fileName, [self, lockToken](const CloudReply& answer) {
        if (!self) {
            return;
        }
        const bool there = answer.ok();
        if (!there && answer.error != CloudError::NotFound) {
            if (!lockToken.isEmpty()) {
                self->client->unlock(self->where.fileName, lockToken, [](const CloudReply&) {});
            }
            self->finishFrom(answer);
            return;
        }
        if (self->remoteMissing && there) {
            // Listed, though missing when this round began: a lock on a
            // missing file may leave an empty placeholder (pCloud lists one
            // with no tag, date or size, and answers 404 to reading it).
            // Only content read back is another device's version.
            self->client->get(self->where.fileName, {}, [self, lockToken](const CloudReply& again) {
                if (!self) {
                    return;
                }
                const bool aVersion = again.ok() && !again.body.isEmpty();
                fingerLog(logPrefix(self->where) + QStringLiteral("placeholder check: status %1, %2 bytes, a version %3")
                              .arg(again.status)
                              .arg(again.body.size())
                              .arg(aVersion));
                if (!aVersion && !again.ok() && again.error != CloudError::NotFound) {
                    if (!lockToken.isEmpty()) {
                        self->client->unlock(self->where.fileName, lockToken, [](const CloudReply&) {});
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
            self->client->get(self->where.fileName, {}, [self, lockToken](const CloudReply& again) {
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
        fingerLog(logPrefix(self->where) + QStringLiteral("tag check: there %1, missing before %2, size %3, tag '%4', expected '%5', "
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

void CloudSync::changedMeanwhile(const QByteArray& lockToken)
{
    // Another device put a version there meanwhile: never replaced; read and
    // merged first, in a new round (OR-15.3).
    if (!lockToken.isEmpty()) {
        client->unlock(where.fileName, lockToken, [](const CloudReply&) {});
    }
    if (rounds < mostRounds) {
        round();
    } else {
        client->remove(uploadName(), [](const CloudReply&) {});
        finish(SyncOutcome::Busy);
    }
}

void CloudSync::moveIntoPlace(const QByteArray& lockToken)
{
    QPointer<CloudSync> self(this);
    client->move(uploadName(), where.fileName, lockToken, [self, lockToken](const CloudReply& answer) {
        if (!self) {
            return;
        }
        fingerLog(logPrefix(self->where) + QStringLiteral("move: error %1, status %2").arg(static_cast<int>(answer.error)).arg(answer.status));
        if (answer.error == CloudError::Superseded) {
            // Another device's version came in at the same moment; the
            // storage put it back on top, and it is merged now (D-080).
            self->changedMeanwhile(lockToken);
            return;
        }
        if (!answer.ok()) {
            if (!lockToken.isEmpty()) {
                self->client->unlock(self->where.fileName, lockToken, [](const CloudReply&) {});
            }
            self->finishFrom(answer);
            return;
        }
        if (!self->goOn(SyncStep::Moved)) {
            return;
        }
        self->client->tagOf(self->where.fileName, [self, lockToken](const CloudReply& tagged) {
            if (!self) {
                return;
            }
            if (!lockToken.isEmpty()) {
                self->client->unlock(self->where.fileName, lockToken, [](const CloudReply&) {});
            }
            // Without a tag the next sync reads the server's version once
            // more and finds nothing new: nothing is lost either way.
            self->where.syncedStamp = self->uploading.left(stampLength);
            const QByteArray uploaded = self->uploading;
            const QString base = cloudBasePath(self->where);
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

void CloudSync::recordSynced(const QByteArray& newTag)
{
    where.tag = newTag;
    where.lastSynced = QDateTime::currentDateTimeUtc();
    rememberCloudPlace(copy, where);
    finish(SyncOutcome::Synced);
}

void CloudSync::finishFrom(const CloudReply& answer)
{
    switch (answer.error) {
    case CloudError::None:
        finish(SyncOutcome::Synced);
        return;
    case CloudError::NoConnection:
        finish(SyncOutcome::NoConnection);
        return;
    case CloudError::Unauthorized:
        finish(SyncOutcome::Unauthorized);
        return;
    case CloudError::NotFound:
        finish(SyncOutcome::NotFound);
        return;
    case CloudError::Certificate:
        finish(SyncOutcome::Certificate, answer.certificate);
        return;
    case CloudError::Changed:
    case CloudError::Locked:
    case CloudError::Superseded:
        finish(SyncOutcome::Busy);
        return;
    case CloudError::TooLarge:
        finish(SyncOutcome::TooLarge);
        return;
    case CloudError::NotSupported:
    case CloudError::Damaged:
    case CloudError::Server:
        finish(SyncOutcome::Server);
        return;
    }
    finish(SyncOutcome::Server);
}

void CloudSync::finish(SyncOutcome outcome, const QByteArray& certificate, const QString& conflictCopy)
{
    fingerLog(logPrefix(where) + QStringLiteral("finished: outcome %1 after %2 rounds").arg(static_cast<int>(outcome)).arg(rounds));
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
