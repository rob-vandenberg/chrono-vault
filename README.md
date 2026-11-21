# Chrono Vault

A password manager for Windows, Linux, macOS, Android and iOS, in C++ with Qt 6.

It keeps your logins, passwords, one-time codes and other secrets in an encrypted vault, and you choose where that
vault lives: on your own disk or a network share, on a WebDAV server, in pCloud or in Google Drive. Chrono Vault has
no server of its own. When you create a vault, you choose where it is stored, and Chrono Vault keeps it in sync there,
always encrypted. When two of your devices changed the vault, the changes are merged.

It also generates passwords, shows one-time codes, warns about weak, reused and leaked passwords, and imports the
exports of other password managers.

## State

**A first, rudimentary version (0.0.4) for Linux and Windows.** Create a vault, add logins and secure notes, generate
passwords, show one-time codes, copy with the clipboard cleared, lock and auto-lock, run in the system tray. The vault
is a local file for now; WebDAV, pCloud and Google Drive follow.

## Licence

AGPL-3.0-only. See [LICENSE](LICENSE).

The fonts, Noto Sans and Noto Sans Mono, are Google's, under the SIL Open Font License 1.1: see
[fonts/OFL.txt](fonts/OFL.txt).

Copyright (C) 2026 Rob Vandenberg <RobVandenberg64@gmail.com>
