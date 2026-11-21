# Decisions

Every design decision of Chrono Vault, one short file each in `decisions/`. A decision is written down only after
the owner approved it with a Proceed (OR-13).

Twenty-three decisions have been taken, from 20 to 22 November 2026: the platforms, the
language, the window toolkit, the licence, where the vault is stored, what the first version leaves out, how
a password is used, translations, the order of the platforms, how releases are built, the settings window, the source layout, the encryption, the vault file, what a vault holds, the windows of the first version, the look, item icons, the icon, tray and starting, one instance with windows that remember, the update,, the settings window's buttons, and the theme and UI scaling.

| Decision | What it settles |
|---|---|
| [D-001](decisions/D-001.md) | The five platforms: Windows, Linux (Debian and Red Hat, X11 and Wayland), macOS, Android, iOS |
| [D-002](decisions/D-002.md) | C++20 with Qt 6, chosen for its continuity |
| [D-003](decisions/D-003.md) | The windows in Qt Quick (QML) on all five platforms |
| [D-004](decisions/D-004.md) | The licence: AGPL-3.0-only |
| [D-005](decisions/D-005.md) | Four storages in this order: local vault file, WebDAV, pCloud, Google Drive; the target chosen when a vault is created, then synced; the technique for each decided when reached; merging |
| [D-006](decisions/D-006.md) | Left out of the first version: browser integration, autofill on phones, sharing, Wi-Fi sync |
| [D-007](decisions/D-007.md) | A password is used by copy and paste, and the clipboard is cleared after a time |
| [D-008](decisions/D-008.md) | Translations from the start, with Poedit; US English the primary language |
| [D-009](decisions/D-009.md) | Linux and Windows (10 and newer) first; Android, macOS and iOS later, all five kept in mind |
| [D-010](decisions/D-010.md) | Releases and the Windows twin: four private scripts, releases in `dist/`, the Windows setup attached to the release of the same version |
| [D-011](decisions/D-011.md) | One settings window: a sidebar of topics on the left, the topic's settings on the right, as in the owner's other programs |
| [D-012](decisions/D-012.md) | The source layout: the vault core in plain C++ apart from the QML windows; Qt 6.8 or newer; strict warnings and a sanitizer build |
| [D-013](decisions/D-013.md) | The encryption: OpenSSL 3, Argon2id for the key, AES-256-GCM for the vault |
| [D-014](decisions/D-014.md) | The vault file: a sealed header and contents, refused whole when damaged, every save safe (autonomous mode) |
| [D-015](decisions/D-015.md) | What a vault holds: items, Login and Secure note templates, built-in and custom fields, JSON inside the seal (autonomous mode) |
| [D-016](decisions/D-016.md) | The windows of the first version: start, unlock, list and editor, saving at once, copying and auto-lock (autonomous mode) |
| [D-017](decisions/D-017.md) | Chrono Vault's own look: Qt's Fusion style, the Noto fonts carried inside, the version in every title |
| [D-018](decisions/D-018.md) | Item icons: picked by the person, stored in the vault as 64-pixel PNG, contents format 2 |
| [D-019](decisions/D-019.md) | The program's icon everywhere, the system tray, and the General settings for starting |
| [D-020](decisions/D-020.md) | One Chrono Vault per person; every window and dialog remembers its size and place, the main window its state |
| [D-021](decisions/D-021.md) | The update check (Daily, Weekly, Monthly, Never), the About box on F1, and the update downloaded, checked and installed as in Chrono Mail |
| [D-022](decisions/D-022.md) | The settings window: a framed sidebar, and OK, Cancel and Restore defaults working as in Chrono Viewer |
| [D-023](decisions/D-023.md) | The theme (Light, Dark, Follow the system) and the UI scaling (50% to 400%), as in Chrono Viewer |
