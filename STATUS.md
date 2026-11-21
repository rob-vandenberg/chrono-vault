# Status

## Now

**Version 0.0.3, the first working version** (autonomous mode, G-26), on Linux and Windows: create a vault, unlock,
add, change and delete logins and secure notes with custom fields, a password generator, one-time codes, copy with the
clipboard cleared and marked secret, lock and auto-lock, and a settings window (D-011). The vault is one encrypted file
(D-013, D-014), saved safely after every change (OR-15). Ten tests pass on Linux (also under the sanitizers) and on
Windows. Committed locally; nothing pushed (OR-10).

## Next task

The owner tries the first version. Then the rest of `ROADMAP.md` steps 3 and 6 to 9, through a changelist and the
owner's Proceed.

## Open questions

1. Which other password managers' exports are imported (9.3).
2. Whether a lost master password can be recovered in any way (9.4, OR-15).
3. Unlocking with a fingerprint or face (9.5).
4. Exporting a vault (9.6).
5. How a release is built for Android, macOS and iOS (9.7); macOS and iOS need a Mac with Xcode (D-009).
6. The technique for WebDAV, pCloud and Google Drive, decided when each step is reached (9.8, D-005).
7. A push guard against naming the reference program, and the personal-data scan of `gitpush.sh`, before any first
    push (OR-7, OR-10).
8. How the settings window looks on a phone screen, decided at `ROADMAP.md` step 17 (D-011).

Before asking the owner any of these, the coder checks how the reference program does it (OR-16).
