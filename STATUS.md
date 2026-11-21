# Status

## Now

**Version 0.0.1** (`ROADMAP.md` step 1, D-012): the skeleton on Linux. An empty Qt Quick window titled "Chrono
Vault"; the vault core as a library of its own; a unit test of the version and an integration test that the main
window loads without QML errors; a normal build in `build/` and a sanitizer build in `build-asan/`, both passing.
Started once on the owner's X11 desktop. Committed locally; nothing pushed (OR-10).

## Next task

`ROADMAP.md` step 2, translations from the start, through a changelist and the owner's Proceed. Steps 2 to 4 need none
of the open questions below; step 5, the vault core, needs questions 1 and 2 settled first.

## Open questions

1. The encryption method, the key derivation and the encryption library (`REQUIREMENTS.md` 9.1, OR-6).
2. The format of the vault file (9.2).
3. Which other password managers' exports are imported (9.3).
4. Whether a lost master password can be recovered in any way (9.4, OR-15).
5. Unlocking with a fingerprint or face (9.5).
6. Exporting a vault (9.6).
7. How a release is built for Android, macOS and iOS (9.7); macOS and iOS need a Mac with Xcode (D-009).
8. The technique for WebDAV, pCloud and Google Drive, decided when each step is reached (9.8, D-005).
9. A push guard against naming the reference program, and the personal-data scan of `gitpush.sh`, before any first
    push (OR-7, OR-10).
10. How the settings window looks on a phone screen, decided at `ROADMAP.md` step 17 (D-011).

Before asking the owner any of these, the coder checks how the reference program does it (OR-16).
