# Status

## Now

**Version 0.0.22**: File > Export… in the reference program's format, after the master password, readable by the
person alone (D-037). Before: website icons (0.0.20), the importer (0.0.19), the editor (0.0.18), the item (0.0.17).

## Next task

In autonomous mode: 0.0.23, unlocking with a fingerprint (Windows Hello on Windows; fprintd and the desktop keyring
on Linux), and the yearly master password reminder.

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
