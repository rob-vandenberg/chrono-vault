# Status

## Now

**Version 0.0.7** (D-023): the theme (Light, Dark, Follow the system) and the UI scaling (50% to 400%) at the top of
Settings > General, as in Chrono Viewer: shown at once, kept with OK, put back with Cancel, applied at every start.
Every window takes the program's font, so the scaling reaches every control; the settings pages scroll. Sixteen tests
pass on Linux, under the sanitizers and on Windows; the review round found nothing real.

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
