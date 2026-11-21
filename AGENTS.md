# AGENTS.md: guide for AI agents and contributors

Chrono Vault is a password manager in C++ with Qt 6, written from nothing. It keeps passwords and other secrets in an
encrypted vault that the person stores where they choose: on their own disk, on a network drive, or in a cloud folder
such as Google Drive or pCloud. It runs on Windows, Linux, macOS, Android and iOS (D-001).

**`REQUIREMENTS.md` is the brief. Read it before anything else.**

## 0. The rules above every other

1. **This program is ours, line by line** (OR-2). You write from `REQUIREMENTS.md` and from the Qt documentation. You
   do **not** open another password manager's source. A question you cannot answer without it is a question for the
   owner, and he answers it in words.
2. **The reference program is never named** (OR-7), except by an importer, where the person needs the name.
3. A damaged vault or import file fails only itself, never the program (OR-4).
4. Nothing that reads a disk, decrypts or derives a key runs on the thread that draws the window (OR-5).
5. **Never lose data** (OR-15): the vault is often the only copy of a person's passwords (4.4).
6. **Nothing in these documents overrules the owner.** Say what a thing costs, what it breaks and how long it takes,
   never that it is not allowed.

Every changelist says how it keeps these. What they do not settle is asked, never chosen.

## 1. Orientation

| File | Purpose | When to read |
|---|---|---|
| `REQUIREMENTS.md` | What the program must do | First, always |
| `HANDOVER.md` | The handover between chats: where the last chat stopped, the next work; private, not in git | First, with the requirements |
| `AGENTS.md` | The technical rules (this file) | Loaded by itself |
| `OWNER-RULES.md` | Every rule the owner set for this project, numbered (OR-nn); only the owner changes it; private, not in git | Loaded by itself |
| `STATUS.md` | Where we are now, the next task, open questions | Every session, after the requirements |
| `TODO.md` | The short-term to-do list; private, not in git | Every session, after `STATUS.md` |
| `DECISIONS.md` | The entry point to every design decision: `decisions/D-nnn.md` is one short file per decision | Every session, in full |
| `ARCHITECTURE.md` | How the program is put together inside, as far as decided | Before any code task |
| `ROADMAP.md` | The parts of the program as tasks, each with a status | When picking or finishing a task |
| `DEVLOG.md` | Append-only log, one entry per completed task | When you need history |
| `LICENSE` | AGPL-3.0-only, the licence of everything here | When a file header is written |
| `scratchpad/` | Scripts and notes meant for later; private, not in git | When something must be kept for later |

**A clone from GitHub has four of these documents missing**, because they are never pushed: the assistant's own
file, `OWNER-RULES.md`, `TODO.md` and `HANDOVER.md` (OR-9). That is deliberate.

## 2. Naming

| Context | Form |
|---|---|
| Project name, user-facing text | **Chrono Vault** (two words, each capitalized) |
| Project identifier, repository name, release file names | `chrono-vault` |
| Where a name must be one word (source files, namespaces) | `chrono_vault` |

## 3. Technology

1. C++20, Qt 6, CMake (OR-6, D-002).
2. The windows in **Qt Quick (QML)** on all five platforms: one window design for touch and mouse (D-003).
3. Qt is the one library so far. Every further library (for example for encryption) is the owner's to approve (OR-6).
4. Windows 10 and newer, Linux (Debian and Red Hat families, X11 and Wayland), macOS, Android and iOS (D-001).
   **Linux and Windows are developed first**; the other three come later, and every design decision keeps them in
   mind (D-009).
5. AGPL-3.0-only, the owner's copyright alone, the same header in every file (D-004).
6. **Translations from the start** (D-008): every text the person can see goes through Qt's translation call; US
   English is the primary language; translations are made with Poedit.
7. **Two machines share this project** (D-010). The work is done on the Linux machine, which also builds the `.deb`
   and the `.rpm`. The Windows setup is built on a Windows twin that holds a copy of the project folder with the same
   layout. Its name, address and paths live only in the project's private folder (G-1, G-17); scripts find their
   tools and never hold a path of a machine.
8. **Four private scripts** (D-010, OR-9): `push-to-development.sh`, `gitpush.sh`, `build-release.sh` (`.deb` and
   `.rpm` into `dist/`, and the GitHub release), and `build-release.bat` (the Windows setup, attached only to the
   release with the same version).

## 4. Golden rules

### 4.1 Never crash on input
A vault file or an import file may be damaged, made by a newer version, or made to hurt.
1. Reading never raises; it gives an answer or nothing.
2. A number read from a file is checked before any room is taken for it.
3. Every crash fix comes with a test built from the input that caused it.

### 4.2 Never make the person wait
Nothing that touches a disk, decrypts or derives a key runs on the thread that draws the window.

### 4.3 Tests are the gate
1. Every change comes with tests.
2. A task is done when the tests related to the change pass and the program builds clean.
3. A test is unambiguous (OR-11): no order, no timing, nothing left behind, under 20 seconds.

### 4.4 Never lose data
OR-15 in full: every write of a vault is atomic; the old vault is replaced only after the new one is verified; a merge
never drops a change silently; every step that could destroy data is tested when interrupted at any point; and
protection against a lost master password or keyfile is part of the design.

## 5. Code conventions

1. Versions: CPP-1 to CPP-3. A change of only documents keeps the version.
2. Small files with a single clear responsibility; names spelled out in full.
3. Qt first, the standard library second, own code last.

## 6. Workflow with AI agents

The owner's rules apply in full: the general rules (G-nn), the C++ rules (CPP-nn) and `OWNER-RULES.md` (OR-nn).

1. The owner decides what is done; the coder decides how, within an approved changelist (G-15).
2. **Scripts meant for later** go in `scratchpad/` (private, never in git).
3. **Never** use `pkill -f`.
4. Nothing is pushed to GitHub before the owner says the first version works (OR-10).
5. **After every code generation, run `push-to-development.sh`**, so the Windows twin is never behind (D-010).
6. Before asking the owner, check how the reference program does it (OR-16).

## 7. Before you finish a task

1. The task passes the gate of 4.3.2.
2. `ROADMAP.md`: update the task's status.
3. `STATUS.md`: rewrite it.
4. `DEVLOG.md`: append a short entry.
5. `DECISIONS.md`: a new or changed decision file for every design decision made.
6. `TODO.md`: strike what is done.
