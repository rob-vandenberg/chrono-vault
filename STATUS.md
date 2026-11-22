# Status

## Now

**Version 0.0.26**: waits after wrong master passwords, kept through a restart (D-041). Before: Settings > Security
with Autolock when (0.0.25), the menus (0.0.24), unlocking with a fingerprint (0.0.23).

## Next task

The backlog in `ROADMAP.md` (PIN unlock first). The owner tests the fingerprint.

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
