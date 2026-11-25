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

#include "audit.h"
#include "settings_files.h"
#include "version.h"

#include <QCoreApplication>
#include <QLocale>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QtConcurrent>

#include <algorithm>

// --- Version ------------------------------------------------------------
// vault_controller_audit.cpp 0.6.86

// --- Version history ----------------------------------------------------
// v0.6.86: Initial release (D-065)

// The audit of the unlocked vault's passwords (D-065): what auditPasswords()
// finds, and the two checks with Have I Been Pwned. A password never leaves
// this device: five digits of its hash are sent, and many hashes come back.
namespace chrono_vault {

namespace {

constexpr int mostLeaksAtOnce = 4;
constexpr qint64 largestRangeBytes = 1024 * 1024;
constexpr qint64 largestBreachesBytes = 32 * 1024 * 1024;

QNetworkRequest onlineRequest(const QUrl& address)
{
    QNetworkRequest request(address);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Chrono-Vault/%1").arg(programVersion()));
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

void neverMoreThan(QNetworkReply* reply, qint64 largest)
{
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [reply, largest](qint64 received, qint64) {
        if (received > largest) {
            reply->abort();
        }
    });
}

} // namespace

void VaultController::startAudit()
{
    if (currentState != State::Unlocked) {
        return;
    }
    if (auditRunning) {
        auditAgain = true;
        return;
    }
    auditRunning = true;
    QPointer<VaultController> self(this);
    (void)QtConcurrent::run([self, vault = contents, leaks = leakCounts, breaches = breachList, session = vaultSession]() {
        const QList<AuditedPassword> found = auditPasswords(vault, leaks, breaches, QDateTime::currentDateTimeUtc());
        QMetaObject::invokeMethod(
            qApp,
            [self, found, session]() {
                if (!self) {
                    return;
                }
                self->auditRunning = false;
                const bool again = std::exchange(self->auditAgain, false);
                if (session == self->vaultSession && self->currentState == State::Unlocked) {
                    self->audited = found;
                    self->auditReady = true;
                    self->showAudited();
                    self->askLeaks();
                    self->askBreaches();
                }
                if (again) {
                    self->startAudit();
                }
            },
            Qt::QueuedConnection);
    });
}

// The answers that came in meanwhile, then the lists and their numbers.
void VaultController::showAudited()
{
    QHash<QString, QSet<QUuid>> lists;
    QHash<QUuid, int> groups;
    for (AuditedPassword& one : audited) {
        one.leaked = leakCounts.value(one.sha1, -1);
        if (one.leaked > 0) {
            lists[QStringLiteral("leaked")].insert(one.itemId);
        }
        if (one.group > 0) {
            lists[QStringLiteral("reused")].insert(one.itemId);
            if (!groups.contains(one.itemId)) {
                groups.insert(one.itemId, one.group);
            }
        }
        if (one.weak) {
            lists[QStringLiteral("weak")].insert(one.itemId);
        }
        if (one.old) {
            lists[QStringLiteral("old")].insert(one.itemId);
        }
        if (!one.breaches.isEmpty()) {
            lists[QStringLiteral("breached")].insert(one.itemId);
        }
    }
    listModel.setAudit(lists, groups);
    emit auditChanged();
}

QVariantMap VaultController::audit() const
{
    QSet<QUuid> leaked;
    QSet<QUuid> reused;
    QSet<QUuid> weak;
    QSet<QUuid> old;
    QSet<QUuid> breached;
    int good = 0;
    for (const AuditedPassword& one : audited) {
        bool fine = true;
        const auto count = [&one, &fine](QSet<QUuid>& list, bool bad) {
            if (bad) {
                list.insert(one.itemId);
                fine = false;
            }
        };
        count(leaked, one.leaked > 0);
        count(reused, one.group > 0);
        count(weak, one.weak);
        count(old, one.old);
        count(breached, !one.breaches.isEmpty());
        good += fine ? 1 : 0;
    }
    const int passwords = static_cast<int>(audited.size());
    return {
        {QStringLiteral("ready"), auditReady},
        {QStringLiteral("passwords"), passwords},
        {QStringLiteral("score"), auditReady && passwords > 0 ? good * 100 / passwords : -1},
        {QStringLiteral("leaked"), static_cast<int>(leaked.size())},
        {QStringLiteral("reused"), static_cast<int>(reused.size())},
        {QStringLiteral("weak"), static_cast<int>(weak.size())},
        {QStringLiteral("old"), static_cast<int>(old.size())},
        {QStringLiteral("breached"), static_cast<int>(breached.size())},
        {QStringLiteral("leakState"), leakState},
        {QStringLiteral("breachState"), breachState},
    };
}

QVariantList VaultController::auditRemarks(const QString& itemId, qint64 fieldUid) const
{
    const QUuid id = QUuid::fromString(itemId);
    const auto one = std::find_if(audited.cbegin(), audited.cend(), [&id, fieldUid](const AuditedPassword& other) {
        return other.itemId == id && other.fieldUid == fieldUid;
    });
    if (one == audited.cend()) {
        return {};
    }
    QVariantList remarks;
    const auto remark = [&remarks](const char* kind, const QString& text) {
        remarks.append(QVariantMap{{QStringLiteral("kind"), QString::fromLatin1(kind)}, {QStringLiteral("text"), text}});
    };
    const QLocale words;
    if (one->leaked > 0) {
        remark("leaked", tr("Found in leaks %1 time(s)", nullptr, one->leaked).arg(words.toString(one->leaked)));
    }
    for (const Breach& breach : one->breaches) {
        remark("breached", tr("%1 was breached on %2 and lost passwords; this one has not changed since")
                               .arg(breach.title, words.toString(breach.date, QLocale::ShortFormat)));
    }
    if (!one->sameIn.isEmpty()) {
        QVariantList items;
        for (const Item& other : contents.items) {
            if (one->sameIn.contains(other.id)) {
                items.append(QVariantMap{{QStringLiteral("id"), other.id.toString(QUuid::WithoutBraces)},
                                         {QStringLiteral("title"), other.title}});
            }
        }
        remarks.append(QVariantMap{{QStringLiteral("kind"), QStringLiteral("reused")},
                                   {QStringLiteral("text"), tr("Used in %n other item(s)", nullptr, static_cast<int>(items.size()))},
                                   {QStringLiteral("items"), items}});
    }
    if (one->weak) {
        remark("weak", tr("Weak"));
    }
    if (one->old) {
        remark("old", tr("Not changed since %1").arg(words.toString(one->changed.toLocalTime().date(), QLocale::ShortFormat)));
    }
    return remarks;
}

void VaultController::setCheckLeaks(bool on)
{
    if (leakChecks == on) {
        return;
    }
    leakChecks = on;
    SettingsFile().setValue(QStringLiteral("audit/checkLeaks"), on);
    if (!on) {
        stopOnlineChecks();
    }
    startAudit();
    emit auditChanged();
}

void VaultController::checkLeaksNow()
{
    if (leakState == QLatin1String("failed")) {
        leakState = QStringLiteral("off");
    }
    if (breachState == QLatin1String("failed")) {
        breachState = QStringLiteral("off");
    }
    startAudit();
}

// The hashes not answered yet, each first five digits asked once.
void VaultController::askLeaks()
{
    if (!leakChecks || currentState != State::Unlocked || leaksAsked > 0 || !leakQueue.isEmpty()) {
        return; // the answers on their way call this again
    }
    for (const AuditedPassword& one : std::as_const(audited)) {
        const QByteArray start = one.sha1.left(5);
        if (!leakCounts.contains(one.sha1) && !leakQueue.contains(start)) {
            leakQueue.append(start);
        }
    }
    const QString state = leakQueue.isEmpty() ? QStringLiteral("done") : QStringLiteral("checking");
    if (leakState != state) {
        leakState = state;
        emit auditChanged();
    }
    for (int started = 0; started < mostLeaksAtOnce; ++started) {
        askNextLeak();
    }
}

void VaultController::askNextLeak()
{
    if (leakQueue.isEmpty()) {
        return;
    }
    const QByteArray start = leakQueue.takeFirst();
    QNetworkRequest request = onlineRequest(
        rangeAddress ? rangeAddress(start)
                     : QUrl(QStringLiteral("https://api.pwnedpasswords.com/range/") + QString::fromLatin1(start)));
    // The answers all about as long, so their length tells nothing.
    request.setRawHeader("Add-Padding", "true");
    QNetworkReply* reply = auditNetwork.get(request);
    neverMoreThan(reply, largestRangeBytes);
    ++leaksAsked;
    connect(reply, &QNetworkReply::finished, this, [this, reply, start, round = onlineRound]() {
        reply->deleteLater();
        if (round != onlineRound) {
            return;
        }
        --leaksAsked;
        const QHash<QByteArray, int> answer = reply->error() == QNetworkReply::NoError ? readLeakAnswer(reply->readAll())
                                                                                       : QHash<QByteArray, int>();
        // A real answer always holds hashes: anything else, such as a hotel's
        // sign-in page, tells nothing.
        if (answer.isEmpty()) {
            leakQueue.clear();
            leakState = QStringLiteral("failed");
            showAudited();
            return;
        }
        for (const AuditedPassword& one : std::as_const(audited)) {
            if (one.sha1.startsWith(start)) {
                leakCounts.insert(one.sha1, answer.value(one.sha1.mid(5)));
            }
        }
        if (!leakQueue.isEmpty()) {
            askNextLeak();
        } else if (leaksAsked == 0 && leakState == QLatin1String("checking")) {
            showAudited();
            askLeaks(); // passwords that changed meanwhile, else done
        }
    });
}

void VaultController::askBreaches()
{
    if (!leakChecks || currentState != State::Unlocked || breachState != QLatin1String("off")) {
        return;
    }
    breachState = QStringLiteral("checking");
    emit auditChanged();
    QNetworkReply* reply = auditNetwork.get(onlineRequest(
        breachesAddress.isEmpty() ? QUrl(QStringLiteral("https://haveibeenpwned.com/api/v3/breaches")) : breachesAddress));
    neverMoreThan(reply, largestBreachesBytes);
    connect(reply, &QNetworkReply::finished, this, [this, reply, round = onlineRound]() {
        reply->deleteLater();
        if (round != onlineRound) {
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            breachState = QStringLiteral("failed");
            emit auditChanged();
            return;
        }
        QPointer<VaultController> self(this);
        (void)QtConcurrent::run([self, answer = reply->readAll(), round]() {
            const auto found = readBreaches(answer);
            QMetaObject::invokeMethod(
                qApp,
                [self, found, round]() {
                    if (!self || round != self->onlineRound) {
                        return;
                    }
                    self->breachState = found ? QStringLiteral("done") : QStringLiteral("failed");
                    self->breachList = found.value_or(QList<Breach>());
                    emit self->auditChanged();
                    self->startAudit();
                },
                Qt::QueuedConnection);
        });
    });
}

void VaultController::stopOnlineChecks()
{
    ++onlineRound;
    leakQueue.clear();
    leaksAsked = 0;
    leakCounts.clear();
    breachList.clear();
    leakState = QStringLiteral("off");
    breachState = QStringLiteral("off");
}

void VaultController::forgetAudit()
{
    stopOnlineChecks();
    audited.clear();
    auditReady = false;
    auditAgain = false;
    listModel.setAudit({}, {});
    emit auditChanged();
}

} // namespace chrono_vault
