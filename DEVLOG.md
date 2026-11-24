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

## 22 November 2026: version 0.0.34: the splitters catch the mouse over 8 pixels

The owner measured the resize cursor over only 2 of the 4 pixels; now 8 (the line shown stays 2).

## 22 November 2026: version 0.0.35: keyboard shortcuts set by the person

Settings > Keyboard shortcuts after Chrono Viewer's page (D-045); Ctrl+B collapses or expands the sidebar, Clear Clipboard moves to Shift+Ctrl+B. One review round, nothing real.

## 22 November 2026: version 0.0.36: no bounce, and the About box's text read by keys

Every scrolling page and list stops at its ends; the About box's licence text has a clear scroll bar and scrolls with the arrow, page and Home/End keys (D-046).

## 22 November 2026: version 0.0.37: the shortcuts page as Chrono Viewer's

Square × and + buttons in columns, "no key" in the keys' column; a settings topic as tall as itself (D-047).

## 22 November 2026: version 0.0.38: Esc exits the program, settings in their own file

The vaults' configuration and login tries in `chrono-vault.conf`, every other setting and stored value in
`chrono-vault.ini`, side by side in the program's folder on Linux and Windows; `--reset` deletes the `.ini` (D-026).
The carry-over from earlier versions and the pre-0.0.28 fingerprint fallback removed (no backwards compatibility).
"Use the Esc key to exit the program", off by default: Esc in the main window, not in edit mode, closes it (D-048).

## 22 November 2026: version 0.0.39: settings tidied, and the Windows setup again

Autolock's switches are tick boxes, the four in a frame; the reminder a choice of Day, Month, Quarter, Year, Never;
the clipboard setting on one line; a normal scroll bar of the style's own (ScrollBar.qml) in the settings window and
the About box, beside the page; collapsed sidebar entries as tall as expanded with 24-pixel icons; Esc closes the
About box (the main window's Esc from 0.0.38 had taken it); no local `key` hiding the member, so Microsoft's compiler
builds the Windows setup again (D-049).

## 22 November 2026: version 0.0.40: the icon picker

"Choose Icon" after the reference program's picker (IconPicker.qml); the vault's own icons in the contents (format
5); the shipped icons in `icons/` by `icons.md`, installed beside the program and copied into
`~/.local/share/chrono-vault/icons` or `%LOCALAPPDATA%\chrono-vault\icons` at each start, never written over
(icon_library); their headings through the translation; the editor's Add at the left, Save and Cancel at the right,
the icon clickable; the reminder below Change the Master Password; keyName's parameter renamed for Microsoft's
compiler (D-050).

## 23 November 2026: version 0.1.41: a clearer name in the icon code

The loop variable in `VaultController::iconSections` is `iconPath`; it no longer hides the member `path`, which
Microsoft's compiler refused. The 0.0.40 setup was built with this change before it was in git.
Also in 0.1.41 (D-051): Up, Down, Page Up, Page Down, Home and End in the item list show the item; the cards on the
theme's Base colour; the item's own Notes box in the editor, the built-in templates without a "Notes" field; the
reference program's twenty field types (six new, contents format 6); `style/Switch.qml`, the owner's switch.

## 23 November 2026: version 0.1.42: Esc as Chrono Viewer has it

The main window's Esc is no longer a shortcut but its own key handling, and the keyboard goes back to the content after
a menu, Chrono Viewer's two fixes (`786104c`, `ae07b6c`), so Esc closes the About box on one of the owner's laptops (D-052).

## 23 November 2026: version 0.1.43: Esc in every dialog's own keys

Every dialog also takes Esc in its own key handling, as Chrono Viewer's QDialog does, next to its Cancel shortcut (D-052).

## 23 November 2026: version 0.1.44: the key box as Chrono Viewer's

Settings > Keyboard shortcuts: the key box and the question about a key in use are windows of their own, as Chrono
Viewer's; while they are open no shortcut answers, so every key is the new key (D-053, P29).

## 23 November 2026: version 0.2.45: merging

`ROADMAP.md` step 10. A change here and one elsewhere are merged against the base, no longer kept as a conflict copy;
the newest change stands, the other is kept in the field's history and in a note on the item; items deleted for good
are remembered, contents format 7 (D-054). Tests: `tests/unit/test_vault_merge.cpp`, `test_elsewhere`, the item page.

## 23 November 2026: version 0.2.46: shortcuts only in the main window, and the layout

The main window's shortcuts answer only while it has the keyboard (P33, D-053). The owner's layout changes: the
sidebar on the cards' colour (Base), the top bar 5 pixels more above and below, the item list 5 pixels in at each
side, the labels of an item ending 50 pixels further left.

## 23 November 2026: version 0.2.47: item counts, long names and the sidebar snap

Counts above 0 only; long sidebar names end in "…" before their count; the sidebar at least 180 wide, snapping shut or
open when its splitter is dragged past 120 (the owner; D-039, D-042).

## 23 November 2026: version 0.2.48: a larger dead zone for the sidebar snap

The sidebar snaps after 100 pixels of travel each way: shut past 80, open past 164 (the owner, D-042).

## 23 November 2026: version 0.2.49: narrower Security fields, the sidebar snap at 80

Settings > Security: the reminder box as wide as its longest word (open: the word with 5 pixels either side), the two
minutes fields 40 pixels narrower; the sidebar snaps after 80 pixels of travel each way (the owner, D-042).

## 23 November 2026: version 0.2.50: keys that cannot be typed are commands while typing

Only keys a text field uses stand aside while one has the keyboard; F1 opens About on the unlock page (the owner, D-039).

## 23 November 2026: version 0.2.51: file dialogs as Chrono Viewer's

Every file dialog is Qt's widget dialog with the desktop's own places, a path to type, its size and folder kept, and
closed by a lock (D-055). Two review rounds; round 1 found the folder not kept and a dialog outliving a lock, both
fixed with tests; round 2 found nothing real.

## 23 November 2026: version 0.2.52: Explorer's file dialog on Windows

On Windows every file dialog is Explorer's own, with its own places, look and size; Linux unchanged (the owner,
D-055). Qt's dialog there was unreadable in dark: `QWidget::setStyle` on the dialog reaches none of its child widgets.
Settings > Security: "Ask for the master password again, once every"; Change right after its text. Settings >
Clipboard: the seconds field 40 pixels narrower. No reviewers (the owner struck them); the Windows dialog is the
owner's to test.

## 23 November 2026: version 0.2.53: Windows Hello over Chrono Vault

On Windows, ticking "Use Windows Hello to unlock vault" opens no box of our own (its Cancel could not stop Hello);
the settings window says "Waiting for Windows Hello fingerprint..." in red at its bottom left, the tick box follows
Hello's answer. Hello gets the window with the keyboard as owner (`RequestCreateForWindowAsync`,
`RequestSignForWindowAsync`, `GetWindowIdFromWindow` loaded at run time), so it opens over Chrono Vault where Windows
has those calls (D-038 point 10). Linux unchanged. No reviewers (the owner tests it). Compiled on the twin; the twin's
full build stops in `tests/unit/test_vault_merge.cpp` line 72 (C4715, from 0.2.45), not touched.

## 23 November 2026: version 0.2.54: one font size of our own

The font was Noto Sans at the system's size: 11 points on the owner's Linux, 9 on Windows, so at Windows' 125% the
text stayed Linux's size while everything else was 125%. Now 11 points on every system (`programFontPoints`); the
scalings enlarge it with the rest (the owner, D-017, with his rule: one design, identical on Linux and Windows). Test:
`test_appearance` checks 11 points from a 9-point system; its double-scale check now compares the size set, not the
drawn size (rounded to whole pixels). Pictures on Linux match the owner's. No reviewers (the owner tests it).

## 23 November 2026: version 0.2.55: 10 points

11 points made Chrono Vault about 20% larger than the other programs on Windows (normal there: 9 points). Now 10
points on every system, the middle road (the owner, D-017). No reviewers (the owner tests it).

## 23 November 2026: version 0.2.56: editing categories, and the owner's icons

The owner's icons ship in `icons/categories` and `icons/symbols/uppercase`, `lowercase`, `digits`; `icons.md` may
name a folder inside a folder; a line starting with "-" is a folder taken out; a shipped folder the person's
`icons.md` does not name is added at its end at start; an icon's name is its file name, translated (D-050). Every
category is edited in one dialog (name, icon, shown), from Customize and a right-click in the left column; Reset for
a built-in one; the changes to built-in categories kept in the vault, contents format 8, merged (D-056). The Edit
dialog is made when first opened: kept hidden inside the settings window, its icon picker's OK button was found by
`test_menus` in place of the settings' own. `fieldOf` in `test_vault_merge.cpp` returns on every path (P42). One
review round (2 reviewers), nothing real.

## 23 November 2026: version 0.2.57: searching the icons

Choose Icon has a Search field: by the shown name, any case, anywhere in the name; groups without a match and the
vault's own icons (no names) are left out; empty at each opening (the owner, D-050 point 9). The Edit category
dialog: the icon at the left with a pen badge, the whole icon a button, "Name:" beside it (D-056 point 8). One
review round (2 reviewers), nothing real; remarked, not faults: no "no results" line, the search field not focused
at opening.

## 23 November 2026: version 0.2.58: the category dialog's tick box

"Show in sidebar", its square under "Name:" (its own padding taken off), 4 pixels higher (the owner, D-056 point 8).
No reviewers (the owner).

## 23 November 2026: version 0.2.59: the editor's labels 50 to the left

`qml/ItemEditor.qml` `labelWidth` 220 → 170, as the view in 0.2.46 (the owner). No reviewers (one value).

## 23 November 2026: version 0.3.60: WebDAV storage

`ROADMAP.md` step 11 (D-057), the owner's changelist items 1-27; version 0.3.60 at the owner's word. Built:
`src/ui/webdav_client.*` (Qt Network), `src/ui/webdav_sync.*` (download, merge, upload, read back, lock, move),
`src/ui/secret_store*` (keyring, Credential Manager), `src/ui/vault_controller_webdav.cpp`, `qml/WebDavWindow.qml`,
the choice in `qml/WelcomePage.qml`, Settings > Vaults, Tools > Sync Now, the line in the left column;
`openVaultDataWithKey` and `saveVaultData` in `src/core/vault_file.*`. Tests: `test_webdav_client`,
`test_webdav_sync`, `test_webdav_interruptions` (a sync stopped at each of its seven steps loses nothing),
`test_webdav_windows`, with a WebDAV server of our own (`tests/webdav_test_server.*`), and `test_menus`. Four review
rounds: round 1 an open edit kept when another device changed the master password, 403 no longer a wrong password,
redirects, the keyring's question waited for; round 2 names Windows cannot keep, servers without tags; round 3 a
master password changed here sent up, not taken for another device's; round 4 clean. Not testable here: a real
server, the desktop keyring's question, Windows Credential Manager (the owner tests).

## 23 November 2026: version 0.3.61: the first upload to pCloud

The owner's test on pCloud ended as "Not synchronized: another device is saving it". Debug version 0.3.60.1 wrote
every WebDAV request to the `--debug` log: after LOCK on the missing vault file pCloud lists an empty placeholder
with tag, date and size "404 Not Found", which the version check took for another device's vault, three rounds.
Fixed in `src/ui/webdav_sync.cpp` (checkTag reads the file: only content is a version); the test server lists
placeholders the same way; the `--debug` lines kept (the owner's changelist item 3).

## 23 November 2026: version 0.3.62: the WebDAV flow after the owner's pCloud test

The owner's changelist items 1-18: `qml/StorageWindow.qml` (tiles, our own two icons), `qml/WebDavWindow.qml`
rebuilt (server list, folder browser, New folder), the first upload before a new vault is kept
(`VaultController::finishCreating` split from `createVault`), own leftover uploads removed, the same bytes no change,
the unlock page's address, Save closes the editor. Tests: `test_webdav_create` (new), `test_webdav_client`,
`test_webdav_windows`, `test_pages`; the test server learnt subfolders and MKCOL. Three review rounds: round 1 a folder that could not be
opened left Chrono Vault pointing at it (now the folder shown changes only once listed); round 2 a second Connect
kept the first server as the browser's top; round 3 clean.

## 23 November 2026: version 0.3.63: a real file dialog for WebDAV

The owner's changelist items 1-11 and 13 (item 12, the related tests and reviewers, struck: the owner tests it):
`qml/WebDavFileDialog.qml` (new, with `art/folder.svg`), `qml/WebDavWindow.qml` (one button, Browse…, its height),
`qml/StorageWindow.qml` (Local first), the folder's entries with size and time of change. `test_webdav_windows`
updated and run; pictures checked.

## 23 November 2026: version 0.3.64: the unlock page with a vault selector

The owner's changelist items 1-13 (reviewers struck): `qml/UnlockPage.qml` as his mockup with the vault dropdown,
`VaultController::lock` keeps the vault used last (D-030 point 5 replaced), `FilePickers::vaultsFolder` and the
start in Documents/chrono-vault, the tick box explaining the primary vault (D-058). Tests: `test_vaults` and
`test_webdav_windows` updated, `test_file_pickers` (the default folder); related tests pass; picture checked in light
(the test's switch to dark does not take).

## 23 November 2026: version 0.3.65: vaults that are not found

The owner's changelist items 1-9, no reviewers (his limits; review rounds later): `VaultController::checkVaults`
(background, re-checks), `qml/UnlockPage.qml` (his design, the found vaults), `qml/WelcomePage.qml` (not found, ↻).
Tests: `test_vaults` (the check), `test_main_window`, `test_unlock_waits`, `test_webdav_windows` updated.

## 23 November 2026: version 0.3.66: one start page, two forms

The owner's changelist items 1-8, no reviewers: `qml/BigButton.qml`, `qml/WelcomePage.qml` (only open and create),
`qml/UnlockPage.qml` (Open another vault… in the list, Create a new vault… below, the place line), `qml/Main.qml`
(the unlock page's two requests). Related tests pass (D-058 points 11-13).


## 24 November 2026: version 0.3.67: the review rounds skipped in 0.3.63 to 0.3.66

The owner's Proceed, autonomous mode. The reviewers saw the range 0.3.62..HEAD by a `git reset --mixed 0.3.62` (files
untouched, commits put back afterwards). Five rounds; real faults fixed, each with a test: a change kept through a
lock was put at risk without a word when another vault was picked or opened from the unlock page (OR-15; asked again,
and given up only once another vault is really opened, `giveUpKeptChangesOnLeaving`); a password typed for one vault
stayed in the field when the vault shown changed; a cancelled Browse… left Open, Next… or Move greyed out; a refused
New folder left a bogus Back step; a look at which vaults exist, still running, dropped a vault made meanwhile (no
test: it depends on timing, OR-11). Not real: no Unlock button (the owner's design), flicker of the not-found line
(no signal while looking), `#` and `?` in folder names (checked: the address comes back the same). Round 5 found
nothing real. The 12 related tests pass.

## 24 November 2026: version 0.4.70: PIN unlock

The owner's changelist items 1-19 (Proceed; x.y = 0.4 at the owner's word): Settings > Security in the owner's order
with the PIN line, the unlock page's PIN field with "Use master password instead", the PIN only while Chrono Vault
runs, wrong PINs in the master password's waits (D-059). `src/ui/vault_controller_pin.cpp` (new), the controller,
`qml/SettingsWindow.qml`, `qml/UnlockPage.qml`, `qml/PasswordField.qml` (the eye only where allowed);
`tests/integration/test_pin_unlock.cpp` (new). Pictures of both pages checked. One review round, nothing real; two of
its remarks taken (a PIN entry that no longer fits also leaves memory; an empty PIN is no attempt). Related tests pass.

## 24 November 2026: version 0.4.71: the PIN page

The owner's changelist items 1-10: with the PIN the unlock page shows only the logo, "Enter the PIN to unlock", the field
and "Use master password instead" (`qml/UnlockPage.qml`, D-059 point 6); `test_pin_unlock` checks both forms. Pictures
of both checked.

## 24 November 2026: version 0.4.72: Windows Hello on top

The owner's changelist items 1-14. The cause, from Microsoft's documentation: the calls that give a key credential's
window an owner exist only from Windows 11 build 26100; the owner's Windows 10 (build 19045) fell back to calls without
one. Now `IUserConsentVerifierInterop::RequestVerificationForWindowAsync` with Chrono Vault's window, the vault's key under
DPAPI (`src/ui/quick_unlock_windows.cpp`, D-038 point 11); compiled on the twin, the owner tests it. "Unlock with
Windows Hello" as wide as its text (`qml/UnlockPage.qml`, checked in `test_reader`). The translation check of the owner
found every text translatable; `test_every_string_translatable` now also checks choices, continuation lines and lists.

## 24 November 2026: debug version 0.4.72.1: where Windows Hello opens

The owner: in 0.4.72 Windows Hello still opens under Chrono Vault on his Windows 10. The debug version (no tests, no
reviewers, the owner's rule) writes to the `--debug` log at every Windows Hello request the window it is given, Qt's
focus window, the window in front and whose it is, the call used, and the answer (`src/ui/quick_unlock_windows.cpp`).

## 24 November 2026: version 0.4.73: Windows Hello asked by the active window

The owner's changelist items 1-9, from 0.4.72.1's log ("owner 0x0, Qt focus window: none, in front: another program"):
Windows Hello always gets the window that asks (`src/ui/quick_unlock_windows.cpp`), and the unlock page asks only once its
window is active (`qml/UnlockPage.qml`); `test_reader` checks both the active rule and, within a pixel, the button width. Two review rounds: round 1 found that a cancelled Hello
would be asked again at once (the window active again); now asked once per showing of the page; round 2 clean.

## 24 November 2026: version 0.4.74: Windows Hello brought to the front

The owner's changelist items 1-10. 0.4.73's log: owner given, our window in front, the call with an owner, Hello still
behind. Now, as Chromium, Bitwarden and KeePassXC do but without a timer, Windows' Hello window is brought to the front
when Windows reports it shown (`src/ui/quick_unlock_windows.cpp`, the owner's exception to G-3, D-038 point 12). A lost
key turns the setting off (Q15 A; `test_quick_unlock`). Two review rounds: round 1 found that a cancelled
turn-on's late answer could stop the next request's listening (now only the request still asking stops it); round 2
clean.
