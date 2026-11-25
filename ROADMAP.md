# Roadmap

The steps of Chrono Vault in the order of development, from the owner's requirements of 20 and 21 November 2026. The
estimates are the coder's judgement, not a measurement: in person-months without AI; the owner's factor for AI-built
work is 5 to 10 times faster. Every step starts with a changelist and the owner's Proceed (G-4).

| # | Step | What it delivers | Estimate without AI | Status |
|---|---|---|---|---|
| 1 | Skeleton on Linux | `CMakeLists.txt` (version 0.0.1), an empty Qt Quick window, Qt Test under CTest, a sanitizer build, the source layout | 1 week | done, version 0.0.1 |
| 2 | Translations from the start | Every visible text through Qt's translation call, gathered into a Poedit file, US English primary (D-008) | 1 week | done in 0.0.2 |
| 3 | The four scripts | `push-to-development.sh`, `gitpush.sh`, `build-release.sh` (`.deb` and `.rpm` into `dist/`), `build-release.bat` (Windows setup attached to the release of the same version) (D-010) | 2 weeks | done in 0.0.4 |
| 4 | Windows build | The skeleton builds and starts on the Windows twin | 1 week | done in 0.0.3 |
| 5 | Vault core | Encryption and key derivation (method and library chosen by the owner first), the vault file format, atomic writes with verification (OR-15), tests on damaged input and interrupted writes (OR-4, OR-15) | 1.5 to 2 months | done in 0.0.2 |
| 6 | Vault contents | Items, templates, custom fields, categories, nested tags, attachments, several vaults, keyfile | 1 month | done in 0.0.15: items, custom fields (0.0.2); categories, templates, nested tags, attachments (0.0.14); several vaults, keyfile (0.0.15); every category edited, name and icon (0.2.56) |
| 7 | The windows | Unlock, item list, item editor, template editor, the settings window (D-011), auto-lock; nothing slow on the window's thread (OR-5) | 1.5 to 2 months | done in 0.0.14 (template editor in Settings > Customize) |
| 8 | Tools | Password generator, one-time codes (TOTP), copy with the clipboard cleared after a time (D-007) | 2 to 3 weeks | done in 0.0.3 |
| 9 | Storage: local vault file | Open and save on a local disk or network share; detect a change made elsewhere (D-005) | 2 weeks | done in 0.0.16: open and save (0.0.3); a change made elsewhere noticed (0.0.16) |
| 9a | Importing the reference program's export | Everything an item of the reference program holds, the editor, the importer, website icons (the owner, 22 November 2026: before merging) | 3 to 4 weeks | done in 0.0.20: the item (0.0.17), the editor (0.0.18), the importer (0.0.19), website icons (0.0.20) |
| 10 | Merging | Merge changes from two devices; what cannot be merged is shown to the person to settle (D-005, OR-15) | 1 to 1.5 months | done in 0.2.45 (D-054) |
| 11 | Storage: WebDAV | Qt Network, a working copy here, synced and merged (D-057) | 2 to 3 weeks | done (0.3.60) |
| 12 | Audit | Weak, reused and leaked passwords, with the Have I Been Pwned check | 2 weeks | done in 0.6.86 (D-065) |
| 13 | Importers | CSV first, then the formats the owner chooses, each tested with random damaged input (OR-3, OR-4) | about 1 week per format | not started |
| 14 | Storage: pCloud | Technique decided by the owner when this step is reached (D-005) | 2 to 3 weeks | not started |
| 15 | Storage: Google Drive | Technique decided by the owner when this step is reached (D-005) | 2 to 3 weeks | not started |
| 16 | First release, Linux and Windows | The owner tests it; push guard and personal-data scan; first push only after the owner says it works (OR-10) | 2 weeks | not started |
| 17 | Android | Build, phone layout, the storages on the phone (D-009) | 2 to 3 months, shared with 18 and 19 | not started |
| 18 | macOS | Needs a Mac with Xcode, and Apple signing (D-009) | | not started |
| 19 | iOS | Needs a Mac with Xcode, Apple signing and the App Store (D-009) | | not started |

## Backlog: to be built next (the owner, 22 November 2026)

The menus and settings of the reference program that Chrono Vault does not have yet, in no set order:

1. PIN unlock: done in 0.4.70 (D-059).
2. Backup and Restore (File; greyed until built).
3. Print.
4. Audit in sidebar, with old passwords: done in 0.6.86 (D-065).
5. Previous Group and Next Group (Ctrl+Up, Ctrl+Down).
6. Duplicate (Ctrl+D), Change Category, Add to Vault.
7. Sync Now (Ctrl+J), with the storages.
8. Auto Backups: to a folder, keeping the last 60, with a list of earlier backups.
9. Clear Recent Items.
10. App language and data location.
11. A dark tray icon.
12. The compromised-password check: done in 0.6.86 (D-065).
13. Proxy settings, logs, Erase Everything.
14. ~~A keyboard shortcut editor that looks and works as Chrono Viewer's.~~ Done in 0.0.35 (D-045).
15. An "Attachments" box always below Tags in the editor, as the reference program's: "Drag and drop or click here / to
    attach files"; a click opens the file chooser, dropped files are added (the owner, 23 November 2026).
16. The vault menu in Settings > Vaults (D-060, the owner, 24 November 2026): Lock, Move…, Forget…, Disconnect…
    (**done in 0.5.75**); the Sync conflicts dialog after Chrono Mail's, for syncs and imports (**done in 0.5.76**);
    Import… from another vault or an export, and Export… of any vault (**done in 0.5.77**). "Merge with…" became Import….
17. The person decides every difference an import or a sync meets (D-061, the owner, 24 November 2026): **done in
    0.5.78**; the windows as Chrono Mail's, with Retry sync and Keep here only: **done in 0.5.81**; what a person guesses
    is what happens (D-062): **done in 0.5.82**; selected and focused, the Item menu, the empty list and the counts (D-063): **done in
    0.5.83**; the tray icon (D-064): **done in 0.5.85**; the topics of Settings (D-066): **done in 0.6.88**; the pop-ups' background (D-067): **done in 0.6.90**; the keyboard in dialogs (D-068): **done in 0.6.91**; exact UI scaling (D-069): **done in 0.6.92**; the windows and the scaling (D-070): **done in 0.6.94**; padlocks and round buttons (D-071): **done in 0.6.100**; menus (D-072): **done in 0.6.101**; the standard password dialog (D-073): **done in 0.6.103**.

**In all: about 15 to 20 person-months without AI, or about 1.5 to 4 months with AI.** The owner's own testing on five
platforms and the store reviews push the real time toward the upper end.

The order is the owner's where his requirements set it (storage: local file, WebDAV, pCloud, Google Drive; Linux and
Windows first). Elsewhere it is the coder's proposal, approved with the changelist: the scripts and the Windows build
come early so every later step is built on both systems; merging comes right after the local file, before WebDAV;
the audit and the importers sit between WebDAV and pCloud.
