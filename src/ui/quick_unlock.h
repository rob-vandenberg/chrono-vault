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

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>

// --- Version ------------------------------------------------------------
// quick_unlock.h 0.0.23

// --- Version history ----------------------------------------------------
// v0.0.23: Initial release (D-038)

namespace chrono_vault {

// Unlocking with a fingerprint (D-038): a secret kept per vault that the
// system gives back only after the person's check. Windows Hello on Windows;
// fprintd and the desktop keyring on Linux; a stand-in in the tests.
class QuickUnlock : public QObject
{
    Q_OBJECT

public:
    // NotKept: the finger was recognized, but the key could not be kept (D-042).
    enum class Result { Done, Cancelled, Failed, NoKey, Unavailable, NotKept };
    Q_ENUM(Result)

    using QObject::QObject;

    // A reader is there and the person has a finger set up (Windows Hello
    // set up); asked off the window's thread by the caller when slow.
    [[nodiscard]] virtual bool available() const = 0;
    // "fingerprint" or "Windows Hello", for the windows' texts.
    [[nodiscard]] virtual QString name() const = 0;
    // What the person is asked to do once the reader is ready (D-042).
    [[nodiscard]] virtual QString touchPrompt() const { return {}; }
    // Why a recognized finger still did not turn it on: the key not kept.
    [[nodiscard]] virtual QString notKeptText() const;
    // Keeps the secret for this vault after the person's check; answered by
    // stored.
    virtual void store(const QString& vault, const QByteArray& secret) = 0;
    // Gives the secret back after the person's check; answered by retrieved.
    virtual void retrieve(const QString& vault) = 0;
    virtual void remove(const QString& vault) = 0;
    // Keeps a new secret for a vault already set up, after a master password
    // change; where the system allows, without asking again.
    virtual void replace(const QString& vault, const QByteArray& secret) { store(vault, secret); }
    // Stops a check still waiting for a finger, so the reader is free again.
    virtual void cancel() {}
    // Whether a secret is kept for this vault (no secret is read for this).
    [[nodiscard]] bool has(const QString& vault) const;
    // The step that failed last, in plain words; empty when none (D-044).
    [[nodiscard]] QString problem() const { return lastProblem; }

signals:
    void stored(chrono_vault::QuickUnlock::Result result);
    // The reader is ready: the person can touch it now (D-042).
    void fingerReady();
    // A finger was not recognized; the reader stays held and asks again.
    void notRecognized();
    void retrieved(chrono_vault::QuickUnlock::Result result, const QByteArray& secret);
    // A kept key could not be removed; where it is, for the person to remove.
    void removeFailed(const QString& where);

protected:
    void remember(const QString& vault, bool kept);
    void setProblem(const QString& text) { lastProblem = text; }

private:
    QString lastProblem;
};

// The one for this system.
[[nodiscard]] std::unique_ptr<QuickUnlock> makeQuickUnlock();

// One step of the fingerprint, with its time, added to
// <data folder>/Chrono Vault/fingerprint.log; never a secret (D-044).
void fingerLog(const QString& step);

} // namespace chrono_vault
