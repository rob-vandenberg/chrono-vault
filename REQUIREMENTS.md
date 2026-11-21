# Chrono Vault: what the program must do

Version of this document: 1, 21 November 2026. Written by the coder from the owner's decisions of 20 and 21 November
2026; it is the owner's (OR-8). It says **what the program does**, never how it is built inside.

Where this document leaves something open, it says so in chapter 9, and the builder asks the owner, never chooses.

---

## 1. What Chrono Vault is

1.1 A password manager: it keeps logins, passwords and other secrets in an encrypted vault, and gives them back to the
person when they unlock it.

1.2 Its functions are those of the reference program (OR-7), minus what chapter 8 leaves out.

1.3 The vault lives where the person chooses: on their own disk, on a network share, on a WebDAV server, in pCloud or
in Google Drive (chapter 5). Chrono Vault has no server of its own, and sends the vault only to the storage the person
chose, always encrypted.

## 2. Platforms (D-001)

2.1 Windows 10 and newer.

2.2 Linux: the Debian family and the Red Hat family, under X11 and under Wayland.

2.3 macOS.

2.4 Android.

2.5 iOS.

2.6 No platform is an afterthought (OR-1): every function of this document works on all five, unless this document
says otherwise for one.

2.7 **Linux and Windows are developed first**; Android, macOS and iOS come later (D-009). Every design decision keeps
all five in mind.

## 3. Technology (D-002, D-003, D-004)

3.1 C++20, Qt 6, CMake.

3.2 The windows are built with Qt Quick (QML) on all five platforms: one window design for touch and mouse.

3.3 AGPL-3.0-only, the owner's copyright alone.

## 4. The vault

4.1 The vault is encrypted on disk. Nothing of its contents is ever written unencrypted.

4.2 It is unlocked with a master password, and optionally a keyfile as well.

4.3 It holds items. An item is made from a template: a login, a credit card, an identity, a note, and others.

4.4 The person can make their own templates and categories, and add, remove and reorder fields, including fields of
several lines.

4.5 Items can carry tags, and tags can be nested.

4.6 Items can carry attachments (for example a scan of an ID card). Attachments are encrypted like everything else.

4.7 The person can keep more than one vault.

4.8 The vault locks itself after a time the person chooses.

4.9 Every item can have an icon of its own, picked by the person: JPG, PNG, BMP, ICO or SVG. It is shown in the item
and in the list (D-018).

## 5. Storage and keeping devices the same (D-005)

5.1 Four kinds of storage are required, built in this order:

5.1.1 a local vault file, on the device's own disk or on a network share;

5.1.2 WebDAV;

5.1.3 pCloud;

5.1.4 Google Drive.

5.2 The technique for each is decided when development reaches it.

5.3 When two devices changed the same vault, the changes are merged. A change is never lost silently: what cannot be
merged is shown to the person to settle.

5.4 Creating a vault, as in the reference program:

5.4.1 When the person creates a new vault, they choose where it is stored: this device only, or one of the storages
of 5.1.

5.4.2 Chrono Vault creates the vault on that target and syncs with it from then on.

5.4.3 The device keeps its own encrypted working copy, synced with the target, so the vault also works offline.

5.4.4 A vault on this device only can be moved to one of the other storages later.

## 6. Tools

6.1 A password generator.

6.2 One-time codes (TOTP): the item holds the secret and Chrono Vault shows the current code.

6.3 An audit that shows weak, reused and leaked passwords. The leaked check asks Have I Been Pwned without the
password ever leaving the device.

6.4 Importers for the exports of other password managers and for CSV (OR-3).

## 7. Using a password (D-007)

7.1 The person copies a field (user name, password, one-time code) and pastes it where it is needed.

7.2 What was copied is cleared from the clipboard after a time.

## 8. What the first version does not have (D-006)

8.1 No browser integration: no browser extension and no filling of web pages.

8.2 No autofill on phones.

8.3 No sharing of items or vaults with other people.

8.4 No syncing over Wi-Fi between devices.

## 9. Open, to be decided by the owner

9.1 The encryption method and the way the key is derived from the master password.

9.2 The format of the vault file.

9.3 Which other password managers' exports are imported.

9.4 Whether a lost master password can be recovered in any way.

9.5 Unlocking with a fingerprint or face on phones and computers that have it.

9.6 Exporting a vault.

9.7 How a release is built and packaged for Android, macOS and iOS. Settled for Linux and Windows by D-010: the `.deb`
and the `.rpm` built here, the Windows setup built on the Windows twin and attached to the release of the same
version.

9.8 The technique for each kind of storage (5.2).

## 10. Languages (D-008)

10.1 Chrono Vault speaks more than one language, built in from the start.

10.2 US English is the primary language; every new text is written in it.

10.3 Translations are made with Poedit.

## 11. The settings window (D-011)

11.1 One settings window holds every setting.

11.2 A sidebar on the left picks the topic; a content pane on the right holds the settings of that topic. It looks
like the settings windows of the owner's other programs, with Chrono Viewer's as the example, and differs from the
reference program on purpose.

11.3 How it looks on a phone screen is decided when the phones are reached.

## 12. The program itself (D-017, D-019)

12.1 The program's icon is `art/chrono-vault.svg`, everywhere the program shows its icon and in the start menu.

12.2 Chrono Vault runs in the system tray. Settings: "Keep running in the system tray when the window is closed",
"Start Chrono Vault at startup", "Start Chrono Vault minimized".

12.3 All text in Noto Sans, passwords and codes in Noto Sans Mono, carried inside the program with their licence.

12.4 Every window title shows the version.
