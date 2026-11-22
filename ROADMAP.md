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
| 6 | Vault contents | Items, templates, custom fields, categories, nested tags, attachments, several vaults, keyfile | 1 month | in part: items, custom fields (0.0.2); categories, templates, nested tags, attachments (0.0.14); not yet several vaults, keyfile |
| 7 | The windows | Unlock, item list, item editor, template editor, the settings window (D-011), auto-lock; nothing slow on the window's thread (OR-5) | 1.5 to 2 months | done in 0.0.14 (template editor in Settings > Customize) |
| 8 | Tools | Password generator, one-time codes (TOTP), copy with the clipboard cleared after a time (D-007) | 2 to 3 weeks | done in 0.0.3 |
| 9 | Storage: local vault file | Open and save on a local disk or network share; detect a change made elsewhere (D-005) | 2 weeks | in part, 0.0.3: open and save; not yet noticing a change made elsewhere |
| 10 | Merging | Merge changes from two devices; what cannot be merged is shown to the person to settle (D-005, OR-15) | 1 to 1.5 months | not started |
| 11 | Storage: WebDAV | Technique decided by the owner when this step is reached (D-005) | 2 to 3 weeks | not started |
| 12 | Audit | Weak, reused and leaked passwords, with the Have I Been Pwned check | 2 weeks | not started |
| 13 | Importers | CSV first, then the formats the owner chooses, each tested with random damaged input (OR-3, OR-4) | about 1 week per format | not started |
| 14 | Storage: pCloud | Technique decided by the owner when this step is reached (D-005) | 2 to 3 weeks | not started |
| 15 | Storage: Google Drive | Technique decided by the owner when this step is reached (D-005) | 2 to 3 weeks | not started |
| 16 | First release, Linux and Windows | The owner tests it; push guard and personal-data scan; first push only after the owner says it works (OR-10) | 2 weeks | not started |
| 17 | Android | Build, phone layout, the storages on the phone (D-009) | 2 to 3 months, shared with 18 and 19 | not started |
| 18 | macOS | Needs a Mac with Xcode, and Apple signing (D-009) | | not started |
| 19 | iOS | Needs a Mac with Xcode, Apple signing and the App Store (D-009) | | not started |

**In all: about 15 to 20 person-months without AI, or about 1.5 to 4 months with AI.** The owner's own testing on five
platforms and the store reviews push the real time toward the upper end.

The order is the owner's where his requirements set it (storage: local file, WebDAV, pCloud, Google Drive; Linux and
Windows first). Elsewhere it is the coder's proposal, approved with the changelist: the scripts and the Windows build
come early so every later step is built on both systems; merging comes right after the local file, before WebDAV;
the audit and the importers sit between WebDAV and pCloud.
