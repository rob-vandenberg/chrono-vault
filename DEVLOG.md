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
