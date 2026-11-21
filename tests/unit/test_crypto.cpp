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

#include "crypto.h"

#include <QtTest>

// --- Version ------------------------------------------------------------
// test_crypto.cpp 0.0.2

// --- Version history ----------------------------------------------------
// v0.0.2: Initial release

using namespace chrono_vault;

namespace {
const crypto::KdfParameters quick{1024, 1, 1};
}

class TestCrypto : public QObject
{
    Q_OBJECT

private slots:
    void the_same_password_and_salt_give_the_same_key();
    void another_password_or_salt_gives_another_key();
    void the_standard_cost_derives_a_key();
    void a_cost_outside_the_limits_is_refused();
    void sealed_data_opens_with_the_same_key();
    void every_seal_uses_a_fresh_nonce();
    void a_wrong_key_does_not_open();
    void one_changed_byte_does_not_open();
    void changed_associated_data_does_not_open();
    void too_short_data_does_not_open();
    void wipe_empties_the_bytes();
};

void TestCrypto::the_same_password_and_salt_give_the_same_key()
{
    const QByteArray salt(crypto::saltLength, 'a');
    const auto first = crypto::deriveKey("correct horse", salt, quick);
    const auto second = crypto::deriveKey("correct horse", salt, quick);
    QVERIFY(first && second);
    QCOMPARE(first->size(), crypto::keyLength);
    QCOMPARE(*first, *second);
}

void TestCrypto::another_password_or_salt_gives_another_key()
{
    const QByteArray salt(crypto::saltLength, 'a');
    const auto key = crypto::deriveKey("correct horse", salt, quick);
    const auto otherPassword = crypto::deriveKey("correct horsE", salt, quick);
    const auto otherSalt = crypto::deriveKey("correct horse", QByteArray(crypto::saltLength, 'b'), quick);
    QVERIFY(key && otherPassword && otherSalt);
    QVERIFY(*key != *otherPassword);
    QVERIFY(*key != *otherSalt);
}

void TestCrypto::the_standard_cost_derives_a_key()
{
    const auto key = crypto::deriveKey("", crypto::randomBytes(crypto::saltLength), crypto::KdfParameters{});
    QVERIFY(key);
    QCOMPARE(key->size(), crypto::keyLength);
}

void TestCrypto::a_cost_outside_the_limits_is_refused()
{
    const QByteArray salt(crypto::saltLength, 'a');
    QVERIFY(!crypto::deriveKey("x", salt, crypto::KdfParameters{4u * 1024 * 1024, 3, 4}));
    QVERIFY(!crypto::deriveKey("x", salt, crypto::KdfParameters{1024, 0, 1}));
    QVERIFY(!crypto::deriveKey("x", salt, crypto::KdfParameters{1024, 1, 0}));
    QVERIFY(!crypto::deriveKey("x", salt, crypto::KdfParameters{1024, 1000, 1}));
}

void TestCrypto::sealed_data_opens_with_the_same_key()
{
    const QByteArray key = crypto::randomBytes(crypto::keyLength);
    const QByteArray sealed = crypto::seal(key, "the secret", "header");
    QCOMPARE(sealed.size(), crypto::nonceLength + 10 + crypto::tagLength);
    const auto opened = crypto::open(key, sealed, "header");
    QVERIFY(opened);
    QCOMPARE(*opened, QByteArray("the secret"));
    const auto emptyOpened = crypto::open(key, crypto::seal(key, QByteArray(), "header"), "header");
    QVERIFY(emptyOpened);
    QVERIFY(emptyOpened->isEmpty());
}

void TestCrypto::every_seal_uses_a_fresh_nonce()
{
    const QByteArray key = crypto::randomBytes(crypto::keyLength);
    QVERIFY(crypto::seal(key, "same", {}) != crypto::seal(key, "same", {}));
}

void TestCrypto::a_wrong_key_does_not_open()
{
    const QByteArray sealed = crypto::seal(crypto::randomBytes(crypto::keyLength), "the secret", {});
    QVERIFY(!crypto::open(crypto::randomBytes(crypto::keyLength), sealed, {}));
}

void TestCrypto::one_changed_byte_does_not_open()
{
    const QByteArray key = crypto::randomBytes(crypto::keyLength);
    const QByteArray sealed = crypto::seal(key, "the secret", {});
    for (qsizetype index = 0; index < sealed.size(); ++index) {
        QByteArray damaged = sealed;
        damaged[index] = static_cast<char>(damaged[index] ^ 0x01);
        QVERIFY2(!crypto::open(key, damaged, {}), qPrintable(QString::number(index)));
    }
}

void TestCrypto::changed_associated_data_does_not_open()
{
    const QByteArray key = crypto::randomBytes(crypto::keyLength);
    const QByteArray sealed = crypto::seal(key, "the secret", "header");
    QVERIFY(!crypto::open(key, sealed, "Header"));
}

void TestCrypto::too_short_data_does_not_open()
{
    const QByteArray key = crypto::randomBytes(crypto::keyLength);
    QVERIFY(!crypto::open(key, QByteArray(), {}));
    QVERIFY(!crypto::open(key, QByteArray(crypto::nonceLength + crypto::tagLength - 1, 'x'), {}));
}

void TestCrypto::wipe_empties_the_bytes()
{
    QByteArray secret("secret");
    crypto::wipe(secret);
    QVERIFY(secret.isEmpty());
}

QTEST_GUILESS_MAIN(TestCrypto)
#include "test_crypto.moc"
