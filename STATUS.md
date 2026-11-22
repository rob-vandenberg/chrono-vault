# Status

## Now

**Version 0.0.23**: unlocking with a fingerprint (Windows Hello; fprintd and the keyring on Linux) and the yearly
master password reminder (D-038). Before: export (0.0.22), website icons (0.0.20), the importer (0.0.19).

## Next task

The owner tests the fingerprint on his two laptops and the import and export. Then, by his choice: the reference
program's settings Chrono Vault lacks, or `ROADMAP.md` step 10, merging.

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
