# Status

## Now

**Version 0.0.10** (D-019, D-023): three fixes after the owner's test of 0.0.9. The tray and start settings are no
longer stored in a group named "general" (Qt reserves it; the choice was lost at every start). Chrono Vault's own
colours are drawn on every desktop from the start (a Qt Quick window took the desktop theme's colours, Breeze's on
KDE); "New Item" is a flat fill in the program's blue. The UI scaling grows everything together, as Chrono Mail does,
in steps of 5%: Chrono Vault's own Qt Quick style (`style/`) over Fusion, and every size in the windows times the
factor. Seventeen tests pass on Linux; the review round found nothing real.

## Next task

The owner tries the update from 0.0.5 or 0.0.6 to 0.0.7 on Linux and Windows, and the theme and UI scaling. Then the
rest of `ROADMAP.md` steps 6 to 9, through a changelist and the owner's Proceed.

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
