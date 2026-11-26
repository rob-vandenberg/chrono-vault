# Chrono Vault privacy policy

Last updated: November 26, 2026

Chrono Vault is a password manager that runs on your own device. This page says what it does with your data.

## The short version

Chrono Vault has no server and no account. It does not collect, sell or share anything about you. Your vault is
encrypted on your device, and it goes only to the storage you choose.

## Your vault

1. Your vault is encrypted on your device before it is written anywhere.
2. Your master password, keyfile and PIN never leave your device.
3. Nobody, including the maker of Chrono Vault, can read your vault without your master password.

## Where your vault goes

Your vault is stored only where you choose: on this device, on a network share, on a WebDAV server, in pCloud or in
Google Drive. Chrono Vault sends it there encrypted, and nowhere else.

## When Chrono Vault uses the internet

Chrono Vault contacts only these places, and only for these reasons:

1. **The storage you chose**, to keep your encrypted vault in sync.
2. **GitHub**, to check for a newer version of Chrono Vault. Only the request for the list of releases is sent. You can
   turn this off in Settings > General > Check for updates.
3. **Have I Been Pwned**, when you run the Audit:
   1. To check whether a password appeared in a known leak, Chrono Vault sends only the first 5 characters of the
      password's SHA-1 hash, never the password itself. The answer is padded, so its size tells nothing.
   2. To check whether a website lost passwords in a known breach, Chrono Vault downloads the public list of breaches.
      Nothing about you or your vault is sent.
4. **The websites in your vault**, only when you turn on Settings > General > Use website icons (off by default). Each
   website is asked for its own icon, so that website can see the request. The icons are kept only while the vault is
   unlocked, never on disk.

There is no tracking, no analytics, no advertising and no crash reporting.

## Google Drive

When you keep a vault in Google Drive:

1. Chrono Vault asks only for the `drive.file` permission: it can see and change only the files it created or the files
   you picked for it. It cannot see the rest of your Google Drive.
2. Chrono Vault uses this access only to store, read and sync your encrypted vault. It does not use the data for any
   other purpose, does not transfer it to anyone, and does not use it for advertising.
3. The sign-in token Google gives Chrono Vault is kept on your device only, in your system's own secure store (the
   keyring on Linux, Credential Manager on Windows).
4. Chrono Vault's use and transfer of information received from Google APIs will adhere to the
   [Google API Services User Data Policy](https://developers.google.com/terms/api-services-user-data-policy), including
   the Limited Use requirements.
5. To remove access, disconnect the vault in Chrono Vault (Settings > Vaults > Disconnect…), or remove Chrono Vault from
   the third-party access page of your Google Account.

## Other storage providers

When you use WebDAV, pCloud or Google Drive, that provider's own privacy policy also applies to the files kept there.

## Changes

When this policy changes, the new version is published here, with a new date at the top.

## Contact

Rob Vandenberg, RobVandenberg64@gmail.com
