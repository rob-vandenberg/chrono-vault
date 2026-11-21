# Decisions

Every design decision of Chrono Vault, one short file each in `decisions/`. A decision is written down only after
the owner approved it with a Proceed (OR-13).

Twelve decisions have been taken, all on 20 and 21 November 2026: the platforms, the
language, the window toolkit, the licence, where the vault is stored, what the first version leaves out, how
a password is used, translations, the order of the platforms, how releases are built, the settings window, and the source layout.

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
