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

## 22 November 2026: version 0.0.11: window sizes, scale slider marks

The About box 555 by 440 and the settings window 730 by 650 (times the scaling), never smaller; the scaling slider's
marks every 25% above it, with room around it (the owner's changelist). Found while building: Qt does not raise a
remembered size to a window's minimum, so `WindowMemory` now does, and moves a window raised past its screen's edge
back inside. Two review rounds; the first found the edge case, the second nothing real.

## 22 November 2026: version 0.0.12: --reset, sizes give way to a small screen

`--reset` skips reading the settings window's settings at the start (D-026); the two windows' sizes give way to a
screen too small for them (D-023). Related tests only.

## 22 November 2026: version 0.0.13: sizes include the title bar

The owner's 650 is the whole window: the desktop's title bar and frame are taken off the inside. Related tests only.

## 22 November 2026: version 0.0.14: categories, templates, tags, favourites, attachments (autonomous mode)

`ROADMAP.md` step 6 in part and step 7: eight built-in categories with thirteen templates, the person's own
categories and templates in Settings > Customize with a template editor, nested tags shown as a tree, favourites,
attachments inside the vault (5 MB each, 150 MB together, only saved where the person chooses); fields moved up and
down and removed in the editor; contents format 3 (D-027 to D-029). The save now puts the contents into JSON off the
window's thread. Found along the way: items a Repeater makes are not in the QObject tree, so the page test looks in
the tree of items; Microsoft's compiler refused two names that hid others. Round 1: an attachment still being read
was lost when the person saved or left the editor; now Save waits and leaving says so, with a test.

## 22 November 2026: version 0.0.15: several vaults, the keyfile (autonomous mode)

`ROADMAP.md` step 6 done: several vaults with the primary's master password unlocking the others and their passwords
saved in it, locking locking all (D-030); a keyfile beside the master password, header format 2, and changing the
master password or keyfile (D-031). Found along the way: the state before creating a vault was read after the
controller had gone busy, so the password was not saved in the primary; caught by the new test.
Three review rounds: a keyfile path with "#" or "%" misread by the password window; a primary with a keyfile refusing
its right password after a switch; a failed save of a new vault's password into the primary said on the new vault;
another vault's password changed but not the copy saved in the primary; "Make a new keyfile" able to write over a
vault's keyfile. All fixed, each with a test.
Round 3: the primary is now always the first vault, even while its drive is away; a primary changed elsewhere and a
stale saved password are said; opening the primary waits for a write to it still running. Each with a test.

## 22 November 2026: version 0.0.16: a change made elsewhere, the focus mark on New Item (autonomous mode)

`ROADMAP.md` step 9 done: every save gives the vault file a new stamp (its header and nonce); a change made elsewhere
is noticed by the system's watcher and right before every save; with nothing waiting here the vault is read again,
with a change waiting both versions are kept, the person's own in a conflict copy that joins the list of vaults; a
change kept through a lock gets the same treatment at the next unlock (P6 in part) (D-032). New Item shows a ring
when it has the keyboard focus (P12). Found along the way: the watcher's check could overwrite the conflict message;
the other version's stamp is now recorded when the copy is written.
Review rounds for 0.0.16: a conflict copy that could not be written let the next save go over the other version; a
half-synced vault locked the person out with a wrong message; the other version's stamp was recorded before it was
read. On Windows the tests showed a real fault: a file held open for reading cannot be replaced there, so a check
reading the vault could make another program's save, or a password change on another thread, fail; vault files are
now read so they can be replaced meanwhile, and every write of an open vault happens on the save thread. Each with a
test.

## 22 November 2026: version 0.0.17: an item as the reference program has it (autonomous mode)

The owner's rule for the importer: identical to the reference program, what is missing built first. The export's
format was learnt from the owner's own export only through scripts that wrote shapes, never values, to files the
owner checked first. Built: contents format 4 with sections, field types, hide marks, hidden and deleted fields,
field history and ids, an item's subtitle and note, Trash and Archive, autosubmit; the item's view as the reference
program draws it, checked in pictures; Hide sensitive fields and Show subtitles (D-033). The editor carries every new
part through an edit; it is redrawn in 0.0.18.
Two review rounds: a new item's fields had no ids until their first save (now given at once); Move to Trash on an
item already in the Trash opened the editor (now it moves straight to the Trash and is disabled there). On Windows,
`test_vaults` now and then saw a save fail with "access denied" at the last step, putting the new file in place; the
same with the file watcher off, so the cause is outside Chrono Vault ([Speculation] the virus scanner holding the
fresh file). Not new in 0.0.17; nothing is lost when it happens. Left to the owner (P16).

## 22 November 2026: version 0.0.18: the editor as the reference program's (autonomous mode)

The editor redrawn after the owner's pictures (D-034): Cancel, "Add ▾" and a blue Save; the icon with a pencil
badge; a row per field with the drag handle and a blue "…" under the mouse; Field Options with name, type, Sensitive
and Delete Field; sections as headings. On Windows, putting a saved vault in place is tried again briefly when another
program holds it (the owner's Q37, P16).
Review rounds for 0.0.18: Delete Field gave its window's settings to the next field; on a touch screen the drag handle
and "…" could not be seen (now always shown where a touch screen is present); a field added and closed without Save
left a false unsaved change; Cancel discarded unsaved edits without asking (now it asks, OR-15). Each with a test.
On Windows the save's brief retry ended the intermittent failure of P16: three runs in a row clean.

## 22 November 2026: version 0.0.19: File > Import… of the reference program's export (autonomous mode)

The importer (D-035): every item, field, section, hidden and deleted field, history, attachment, custom icon,
category and template of the person's own, read off the window's thread, added in one save; a damaged export refused
whole. Tested on a made-up export in the same format, and once on the owner's real export with only counts written
for the owner to check. Two review rounds: the import window stayed open after a lock, and its Import button could
lose its name; both fixed with a test.

## 22 November 2026: version 0.0.20: website icons (autonomous mode)

Settings > Customize > Use website icons, off by default: each website asked for its own icon, kept in memory only
and forgotten at the lock (D-036). Found along the way: the icons were not asked for again after an unlock, because
the list is refreshed just before the vault counts as unlocked; caught by the new test against a local server.
Two review rounds: each arriving icon reset the whole list, the size limit applied only after the download, icons
were re-encoded on every redraw, and the Trash's websites were asked too; all fixed, the last with a test.

## 22 November 2026: version 0.0.21: File > Import… opens its file chooser

The owner: File > Import… did nothing. Cause: the import window's file chooser had no parent window, because the
import window is still hidden when it opens, and the desktop did not show it. It now belongs to the main window; the
keyfile choosers of the master password window got their window too. The page test checks the chooser opens with a
window. The import of the owner's real export was checked by counts only, written for the owner first: every one of
45 items, 600 fields, 87 deleted, 70 sections, 75 earlier values, 25 custom icons and 2 attachments read as the export
holds them.

## 22 November 2026: version 0.0.22: export in the reference program's format (autonomous mode)

File > Export…, after the master password and a plain warning (D-037); export and import again give back what a person
sees, item for item. Found along the way: the rule greying a dialog's OK button never took hold in four windows (set
too early, before the button existed); `test_pages` grew past 20 seconds under ASan and was split (OR-11). Review round
1: the export and a new keyfile are now readable by the person alone; problems are said in the export window too.

## 22 November 2026: version 0.0.23: unlocking with a fingerprint (autonomous mode)

Windows Hello through C++/WinRT (it compiled on the twin at the first try), fprintd and the Secret Service over D-Bus
on Linux, a stand-in for the tests; the yearly master password reminder (D-038). Found along the way: the check for a
reader ran on a thread with a pointer to an object that could go away first; it now uses an object of its own.
Review round 1: on Linux the finger scan was never stopped (the reader stayed claimed after a typed password); a
locked keyring whose prompt was dismissed counted as "key gone" and turned it off; a master password change asked for
the finger unannounced and a cancel turned it off silently. Now: the scan stops at an unlock or a new scan; "keyring
not reachable" is apart from "no key"; on Linux the new key is kept without a finger, on Windows the message says
Windows Hello may ask once, and a failure says to turn it on again. A master password change restarts the reminder's
year. Each with a test.
Review round 2: removing the key on Linux could freeze the window while the keyring is slow (now off its thread); a
kept key no longer fitting after a master password change on another computer gave a misleading message each time
(now it turns the fingerprint off for that vault and says why); a disconnected reader now says "not available".
Review round 3: turning the fingerprint off with the keyring locked could leave the key there unnoticed (now the
keyring is opened first, the removal checked, and a failure said with the entry's name); the finger check spoke to
fprintd from the window's thread (now every call is sent without waiting there).
Review round 4: on Windows a Windows Hello prompt stayed open after the master password unlocked the vault (now
closed); a finger unlock overtaken by another could leave its mark for a later opening, which on a "changed elsewhere"
would have turned the fingerprint off wrongly (now the mark goes with its own attempt); on Linux a second click while
the reader was still being claimed left it claimed until the program quit (each check now has its number, a late
claim is given back); a reader's own error said nothing (now "not available"); Settings said "fingerprint reader" on
Windows (now "Windows Hello is not set up"). These five need a real reader or a race in time, so no test reaches them.

## 22 November 2026: version 0.0.24: the menus after the reference program's (autonomous mode)

File, View, Item, Tools and Help as the reference program has them, with only what Chrono Vault does, and their
shortcuts shown at the right (D-039). New: item counts, Hide Sidebar, the left column's groups by choice, the copies of
the Item menu, Show Passwords, Clear Clipboard, Support website, and a copy button for the one-time code. Found along
the way: after an unlock the keyboard stayed in the hidden password field of the page left behind (typed letters went
there); it now goes to the item list. Fusion's menu builds entries of its own, not the style's; the five menus now name
the style's entry. `tests/integration/test_menus.cpp` drives it all.
Review round 1: the menus lost their shortcut text while a text field had the keyboard (the text now comes apart from
the working shortcut); Show Passwords stayed on through Hide Sensitive switched off and on (now it ends there). Both
with tests.
Review round 2: Launch URL took a host with a port ("nas.local:5000") for a scheme; now only "scheme://", mailto: and
tel: count as one, everything else opens as https. With a test.
Review round 3: hiding a group of the left column while one of its entries was chosen left the list narrowed with
nothing showing why (now it goes back to Home); an address with spaces around it did not launch (now trimmed). Both
with tests.
Review round 4: Copy OTP stayed clickable for a one-time code whose secret makes no code (a damaged import); now
greyed. With a test.

## 22 November 2026: version 0.0.25: Change the Master Password in Settings, and Autolock when (autonomous mode)

Settings > Security has a Change the Master Password row and the reference program's Autolock when: the main window
closed, the system sleeping, Chrono Vault idle, the system inactive (D-040). `src/ui/system_activity*` asks logind and
the desktop over D-Bus on Linux, the power messages and the last input on Windows. An old "0 minutes" becomes the idle
box unticked. `tests/integration/test_autolock.cpp` drives it through a stand-in system.
Review round 1: after a fingerprint unlock the system still counted the time before it as inactive (a finger is no
input the system sees), so "System inactive for" locked again at once; the inactive time now counts from the unlock
on. With a test.

## 22 November 2026: version 0.0.26: waits after wrong master passwords (autonomous mode)

The owner's rule (D-041): three tries at once, then 10 seconds, a minute and 10 minutes, and an hour once after every
25; counted per vault in the settings, so a restart does not end a wait; a correct unlock starts again. The unlock page
greys with "Try again in m:ss". `tests/integration/test_unlock_waits.cpp` drives 26 wrong tries with the tests' clock.

## 22 November 2026: version 0.0.27: the collapsible sidebar, and the fingerprint reader held only while asked

The sidebar collapses to its icons and its groups fold with their ▾; Archive and Trash have icons; the splitters are
easier to catch; Keep running in the tray is the last of the three in General (D-042). Found on the owner's laptop:
turning the fingerprint on asked for a finger without saying so and kept the reader; a second press was reported as a
failure; the unlock page never asked at a start. Now the reader is held only while a finger is asked and given back
the moment that is settled; the settings window says "Touch the fingerprint reader…" and the outcome, with a Cancel.
`tests/integration/test_reader.cpp` drives it with a stand-in fprintd.
Review round 1: on Windows a key not kept was blamed on a "desktop keyring" Windows does not have; each system now
says its own reason.

## 22 November 2026: version 0.0.28: the fingerprint one check box for all vaults

The owner: one line, a check box, for all vaults; the touch in a popup (D-043). Each vault's key is kept at its next
master-password unlock; unticking removes them all. Found: the Autolock switches were mirrored (on while saying Off);
the collapsed sidebar's triangles were not centered as in the owner's mockup.

## 22 November 2026: version 0.0.30: --debug, and the identifier chrono-vault for the settings

The fingerprint log is written only with `--debug` (the owner), to `~/.local/share/chrono-vault/fingerprint.log`. The
settings live under the identifier `chrono-vault`, no longer under the name "Chrono Vault"; those of before are
carried over once, the old place removed only after every value reads back the same (D-044).

## 22 November 2026: version 0.0.31: the triangles centered

The collapsed sidebar's group triangles stand centered: their button spans the column (0.0.28 tried a centering that a row of one item ignores, and it was not checked in a picture).

## 22 November 2026: version 0.0.32: no filler items in the top bar and the dialogs

The menu bar, and a dialog's left button area, take the free room themselves; the empty items that only took up room are gone (the editor's one, between two buttons that must not stretch, stays).

## 22 November 2026: version 0.0.33: room around the About box's text

The licence text with 8 pixels more room above it and 2 below (the owner).
