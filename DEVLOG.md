# Development log

Append-only, one entry per completed task, newest last.

## 21 November 2026: the project was started

The folder, the git repository (branch `main`, nothing committed) and the documents were made: `AGENTS.md`,
`REQUIREMENTS.md`, `DECISIONS.md` with D-001 to D-007, `ARCHITECTURE.md`, `ROADMAP.md`, `STATUS.md`, `README.md`,
`LICENSE` (AGPL-3.0), and the private `OWNER-RULES.md`, `HANDOVER.md` and `TODO.md`. The owner rules were copied from
Chrono Viewer and adapted. No code yet.

## 21 November 2026: the owner rules approved, OR-15 added

The owner approved the rules copied from Chrono Viewer and adapted to Chrono Vault, and added OR-15, never lose data:
atomic writes, the old vault replaced only after the new one is verified, no change dropped silently by a merge,
every destructive step tested when interrupted, and protection against a lost master password as part of the design.
`AGENTS.md` carries it in section 0 and as golden rule 4.4.

## 21 November 2026: the owner's requirements written in, the roadmap in steps

D-005 replaced (four storages in order: local vault file, WebDAV, pCloud, Google Drive; the technique for each decided
when reached), and D-008 (translations from the start, Poedit, US English), D-009 (Linux and Windows first) and D-010
(the Windows twin and the four private scripts) added. `REQUIREMENTS.md`, `AGENTS.md`, `ARCHITECTURE.md`, OR-9 and
`.gitignore` brought in line; OR-16 added (questions first answered by the reference program). `ROADMAP.md` now lists
19 steps in the order of development. The machine notes were copied into the private folder. No code yet.

## 21 November 2026: the settings window, and creating a vault

D-011 added: one settings window with a sidebar of topics and a content pane, as in the owner's other programs.
D-005 gained "Creating a vault": the target is chosen when a vault is created, the vault is then synced there, the
device keeps an encrypted working copy. `REQUIREMENTS.md` 5.4 and chapter 11, `ROADMAP.md` step 7, `STATUS.md` and
`README.md` brought in line. No code yet.

## 21 November 2026: version 0.0.1, the skeleton on Linux (`ROADMAP.md` step 1)

`CMakeLists.txt` with version 0.0.1, C++20, Qt 6.8 or newer, warnings treated as errors, and a sanitizer build
(`CHRONO_VAULT_SANITIZE`, ASan and UBSan). The vault core `src/core/` (for now only the version), the windows as the
QML module `qml/` (an empty window titled "Chrono Vault" through the translation call), and `src/app/main.cpp`
(D-012). Two tests, both passing in the normal and the sanitizer build. Started once on the owner's X11 desktop.

## 21 November 2026: version 0.0.2, translations and the vault core (autonomous mode)

Translations with Poedit from the start (D-008); `push-to-development.sh`; the vault core: encryption with OpenSSL,
Argon2id and AES-256-GCM (D-013), the vault file with safe saves (D-014), the contents (D-015), the password generator
and one-time codes (RFC 6238). Eight tests, passing in the normal and the sanitizer build.

## 21 November 2026: version 0.0.3, the first working version (autonomous mode)

The windows in Qt Quick (D-016): start, unlock, the item list with search, the item editor with custom fields, the
password generator, one-time codes, copying with the clipboard cleared and marked secret, lock and auto-lock, and the
settings window with a sidebar (D-011). The controller does every slow step off the window's thread (OR-5) and saves
after every change (OR-15). The Windows build works on the Windows twin (OpenSSL from vcpkg). Ten tests, among them one
that drives the pages themselves; it found a binding loop in the delete dialog, now fixed. Five review rounds
followed; rounds 1 to 4 found faults around unsaved work (changes dropped when switching items, closing or locking;
a failed save hidden by the next message; a false "not saved" warning after Save and Lock), all fixed with tests and
recorded in D-016 points 9 to 22; round 5 found nothing real.

## 21 November 2026: version 0.0.4, the owner's first test answered, scripts and release

Fusion and the Noto fonts (D-017), the eye in password fields, the version in titles, item icons in the vault (D-018,
contents format 2), the program's icon everywhere and the system tray with three General settings (D-019), and the
scripts: `gitpush.sh` with its own scanner and guard, `build-release.sh` (container build, `.deb` and `.rpm`, four bare
systems), `build-release.bat` (Windows setup, attached to the release of the same version). Four review rounds:
closing that could quit instead of hiding, a cancelled Quit that stayed a Quit, the autostart path, the push argument,
a tick box that lied after a failure and a logout that could be held up were found and fixed; the last round found
nothing real. Twelve tests on Linux, under the sanitizers and on Windows.

## 21 November 2026: version 0.0.4 completed: one instance, windows that remember, the first push

One Chrono Vault per person (a later start brings the running one forward and ends; checked with two real starts on
Linux and Windows), every window and dialog remembering its size and place, the main window its state too (D-020),
the dialogs as movable, resizable windows. The owner's launcher kept out of git and made double-clickable. Thirteen
tests on Linux, under the sanitizers and on Windows. First push to GitHub, and the first release.

## 21 November 2026: version 0.0.5: the update

The update as in Chrono Mail (D-021): the check setting, About on F1, Download or Skip this version, the download with
its SHA-256 check, PackageKit or the silent Windows setup, and the restart. The bare Ubuntu 22.04 could not reach GitHub:
no CA certificates; the packages now need `ca-certificates`. Six review rounds found and fixed: a cancelled Quit that
left an update armed, a disk-full download taken for a stop, a refused install or broken-off download that could not
be tried again, an update lost when the quit waited for the editor, Cancel in the editor's box that left it armed, and
the installer's error reply not read. Fifteen tests on Linux, under the sanitizers and on Windows.

## 22 November 2026: version 0.0.6: the settings window's sidebar and buttons

The framed sidebar, and Restore defaults, OK and Cancel working as in Chrono Viewer (D-022); the defaults named once;
the two icons drawn for Chrono Vault. Four review rounds found and fixed: Enter did nothing; a failed start at startup
closed the window silently; Enter on a focused Cancel did OK; Enter and Esc in the open update list acted on the
window; Restore defaults changed the greyed tray setting; a reopened window kept the focus on Cancel. The two new
icons were kept out of git by `.gitignore`; now let in. Also the release to try the update from 0.0.5.

## 22 November 2026: version 0.0.7: theme and UI scaling

Chrono Viewer's light and dark palettes and Follow the system, and the UI scaling from 50% to 400% (D-023), shown at
once and kept with OK. Found while building: Qt Quick's controls do not take a changed program font by themselves, so
every window takes it from `Appearance`, and the dialogs became application windows (a plain window has no font); the
sidebar measures its names in the font in use. One review round, nothing real. Sixteen tests.

## 22 November 2026: version 0.0.8: the About box with the licences

The About box as in the owner's mockup (D-021): the licences carried inside the program and shown in a read-only,
scrollable text area, each after a line naming what it covers; Check for update at the bottom left. Every dialog got
the settings window's narrow edge and a place for its own buttons at the bottom left. The tray text changed. Found
while building: Qt's rich text made every line of a licence a paragraph of its own, so lines are broken explicitly.
One review round, nothing real.

## 22 November 2026: version 0.0.9: the update popup, the three-column main window

The update's windows as in Chrono Mail (D-024) and the main window after the reference program's layout (D-025); the
About box's top lines against the icon. Found while building: `ItemView` is a name of Qt Quick's own (renamed
`ItemDetails`); a plain item has no font. Review round 1 found and fixed: a cancelled quit after Restart or Close left
no notice, and a deleted item stayed shown. Seventeen tests.

## 22 November 2026: version 0.0.10: settings names, own colours on every desktop, scaling of everything

Autonomous mode after the owner's Proceed. The cause of the wrong blue was found on the real screen: a Qt Quick
window takes the desktop theme's colours when it is made, so a look applied at the start never reached it; every
window now takes the program's colours (`Appearance::dress`). The tray and start settings moved out of Qt's reserved
"general" group, old values carried over. UI scaling now scales everything: Chrono Vault's own style over Fusion
(`ChronoVault.Style`, version 6.0 so Fusion stays its fallback), 117 sizes in the windows times the factor, steps
of 5%. Four sources were missing from the strict warnings; added. One review round, nothing real.
