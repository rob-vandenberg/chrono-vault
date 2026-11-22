# Status

## Now

**Version 0.0.8** (D-021): the About box as in the owner's mockup: the icon with the name, version and copyright,
below them a scrollable text area with every licence Chrono Vault ships (our AGPL-3.0 first, then Qt's LGPL-3.0,
GPL-3.0 and GPL-2.0, OpenSSL's Apache 2.0 and the Noto fonts' OFL 1.1), Check for update at the bottom left and
Close at the bottom right. Every dialog has the narrow edge of the settings window. The tray setting reads "Keep
Chrono Vault running in the system tray". Sixteen tests pass on Linux; the review round found nothing real.

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
