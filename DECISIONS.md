# Decisions

Every design decision of Chrono Vault, one short file each in `decisions/`. A decision is written down only after
the owner approved it with a Proceed (OR-13).

Thirty-eight decisions have been taken, from 20 to 22 November 2026: the platforms, the
language, the window toolkit, the licence, where the vault is stored, what the first version leaves out, how
a password is used, translations, the order of the platforms, how releases are built, the settings window, the source layout, the encryption, the vault file, what a vault holds, the windows of the first version, the look, item icons, the icon, tray and starting, one instance with windows that remember, the update, the settings window's buttons, the theme and UI scaling, the update's windows, the main window's layout, --reset, categories and templates, tags and favourites, attachments, several vaults, the keyfile, a change made elsewhere, an item as the reference program has it, its editor, importing its export, website icons, exporting in its format, and unlocking with a fingerprint.

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
| [D-015](decisions/D-015.md) | What a vault holds: items, templates, built-in and custom fields, JSON inside the seal; contents format 3 since 0.0.14 (autonomous mode) |
| [D-016](decisions/D-016.md) | The windows of the first version: start, unlock, list and editor, saving at once, copying and auto-lock (autonomous mode) |
| [D-017](decisions/D-017.md) | Chrono Vault's own look: Qt's Fusion style, the Noto fonts carried inside, the version in every title |
| [D-018](decisions/D-018.md) | Item icons: picked by the person, stored in the vault as 64-pixel PNG, contents format 2 |
| [D-019](decisions/D-019.md) | The program's icon everywhere, the system tray, and the General settings for starting |
| [D-020](decisions/D-020.md) | One Chrono Vault per person; every window and dialog remembers its size and place, the main window its state |
| [D-021](decisions/D-021.md) | The update check (Daily, Weekly, Monthly, Never), the About box on F1 with every licence that ships, and the update downloaded, checked and installed as in Chrono Mail |
| [D-022](decisions/D-022.md) | The settings window: a framed sidebar, and OK, Cancel and Restore defaults working as in Chrono Viewer |
| [D-023](decisions/D-023.md) | The theme (Light, Dark, Follow the system), the program's own colours on every desktop, and the UI scaling of everything together (50% to 400% in steps of 5%) |
| [D-024](decisions/D-024.md) | The update's windows as in Chrono Mail: the popup with Skip this update, Update downloaded, Restart or Close |
| [D-025](decisions/D-025.md) | The main window after the reference program: menus and buttons on one bar, three columns, the item read-only |
| [D-026](decisions/D-026.md) | Two files: the vaults in `chrono-vault.conf`, everything else in `chrono-vault.ini`; `--reset` deletes the `.ini` |
| [D-027](decisions/D-027.md) | Categories and templates: eight built-in categories with thirteen templates, the person's own kept in the vault, Settings > Customize (autonomous mode) |
| [D-028](decisions/D-028.md) | Tags nested with a slash, shown as a tree; favourites marked with a star (autonomous mode) |
| [D-029](decisions/D-029.md) | Attachments inside the vault, 5 MB each and 150 MB together, only saved where the person chooses (autonomous mode) |
| [D-030](decisions/D-030.md) | Several vaults: the primary's master password unlocks the others, their passwords saved in it; locking locks all (autonomous mode) |
| [D-031](decisions/D-031.md) | The keyfile beside the master password (header format 2), and File > Change master password or keyfile (autonomous mode) |
| [D-032](decisions/D-032.md) | A change made elsewhere: noticed by the file's stamp, read again, or both versions kept with a conflict copy (autonomous mode) |
| [D-033](decisions/D-033.md) | An item as the reference program has it: sections as cards, its field types, hide marks, hidden and deleted fields, field history, subtitle and note, Trash and Archive (autonomous mode) |
| [D-034](decisions/D-034.md) | The editor as the reference program's: Cancel, Add and Save at the top, a row per field with ≡ and "…", Field Options; a brief retry of a save on Windows (autonomous mode; the retry the owner's Q37) |
| [D-035](decisions/D-035.md) | File > Import… of the reference program's export: everything, exactly as it holds it, in one save (autonomous mode) |
| [D-036](decisions/D-036.md) | Website icons: off by default, asked of each website itself, kept in memory only (autonomous mode) |
| [D-037](decisions/D-037.md) | Export in the reference program's format: master password first, a plain warning, the whole vault, readable by the person alone (autonomous mode) |
| [D-038](decisions/D-038.md) | Unlocking with a fingerprint: Windows Hello on Windows, fprintd and the keyring on Linux (weaker), the master password once a year (autonomous mode) |
| [D-039](decisions/D-039.md) | The menus after the reference program's: File, View, Item, Tools, Help, their shortcuts, the one-time code's copy button |
| [D-040](decisions/D-040.md) | Settings > Security: Change the Master Password, and Autolock when the window closes, the system sleeps, Chrono Vault or the system is idle |
| [D-041](decisions/D-041.md) | Waits after wrong master passwords: 3 at once, then 10 seconds, a minute, 10 minutes, an hour after every 25; kept through a restart |
| [D-042](decisions/D-042.md) | The sidebar collapses to its icons and its groups fold; wider splitters; the fingerprint reader held only while a finger is asked, a switch with a one-time touch |
| [D-043](decisions/D-043.md) | The fingerprint one check box for all vaults, its touch in a popup; the Autolock switches no longer mirrored; centered triangles |
| [D-044](decisions/D-044.md) | `--debug` writes the fingerprint log; the settings under the identifier chrono-vault |
| [D-045](decisions/D-045.md) | Keyboard shortcuts set by the person, after Chrono Viewer; Ctrl+B the sidebar, Shift+Ctrl+B Clear Clipboard |
| [D-046](decisions/D-046.md) | No bounce anywhere; the About box's text with a clear scroll bar, scrolled by the arrow, page and Home/End keys |
| [D-047](decisions/D-047.md) | The shortcuts page as Chrono Viewer's; a settings topic as tall as itself |
| [D-048](decisions/D-048.md) | "Use the Esc key to exit the program": Esc in the main window, not in edit mode, closes it |
| [D-049](decisions/D-049.md) | Settings tidied (Autolock tick boxes in a frame, the reminder a choice, the clipboard on one line), normal scroll bars, collapsed entries as tall as expanded, Esc in the About box |
| [D-050](decisions/D-050.md) | The icon picker: the vault's own icons, the shipped icons from `icons/` by `icons.md`, the editor's buttons and icon click |
| [D-051](decisions/D-051.md) | Arrow keys show the item; cards on Base; the one Notes per item; the reference program's twenty field types; our own switch |
| [D-052](decisions/D-052.md) | Esc as Chrono Viewer has it: not a shortcut, and the keyboard back to the content after a menu |
| [D-053](decisions/D-053.md) | The key box as Chrono Viewer's: windows of their own, every key the new key, the question about a key in use |
| [D-054](decisions/D-054.md) | Merging: automatic, three-way; the newest change stands with a note; deletions travel |
| [D-055](decisions/D-055.md) | File dialogs as Chrono Viewer's: Qt's widget dialog, the desktop's own places, a path to type |
| [D-056](decisions/D-056.md) | Editing categories: name, icon and shown for every category; Reset for a built-in one |
| [D-057](decisions/D-057.md) | WebDAV storage: a working copy here, synced and merged; the password in the system's store; a certificate trusted by its fingerprint |
| [D-058](decisions/D-058.md) | The unlock page with a vault selector; a lock keeps the vault used last; new local vaults in Documents/chrono-vault |
| [D-059](decisions/D-059.md) | PIN unlock: only while Chrono Vault runs, the PIN's key on disk only sealed with each vault's own key |
| [D-060](decisions/D-060.md) | The vault menu in Settings > Vaults: Lock, Move…, Forget…, Disconnect…, Import…, Export…; vaults not found listed |
| [D-061](decisions/D-061.md) | The person decides every difference an import or a sync meets; the sides Vault/Import and This device/Other device |
| [D-062](decisions/D-062.md) | What a person guesses is what happens: keys, mouse, several items, Delete asked, dialogs; Settings > Vaults; 5 pixels in every button |
| [D-063](decisions/D-063.md) | Selected and focused; the Item menu on the selected items; Change Category; the empty list; the vault's counts |
| [D-064](decisions/D-064.md) | The tray icon: the owner's blue vault, on every tray |
| [D-065](decisions/D-065.md) | Audit: leaked, reused, weak, old, breached websites; a score; Have I Been Pwned off until ticked; items left out; contents format 10 |
| [D-066](decisions/D-066.md) | The topics of Settings: Categories, Tags and Templates of their own; Customize and Clipboard gone |
| [D-067](decisions/D-067.md) | The main menu's background for every pop-up menu and list |
| [D-068](decisions/D-068.md) | Dialogs open in their text field; Choose Icon by keyboard; a right-click menu for text fields |
| [D-069](decisions/D-069.md) | Exact UI scaling |
| [D-070](decisions/D-070.md) | The main window keeps its size at another scaling; the others scale along |
| [D-071](decisions/D-071.md) | Padlocks and Open in Settings > Vaults; round picture buttons, vertical dots |
