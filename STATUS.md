# Status

## Now

**Version 0.0.4** (D-017 to D-019): Fusion and the Noto fonts carried inside, the eye in every password field, the
version in every title, item icons, the program's icon everywhere, the system tray with the General settings (keep
running in the tray, start at startup, start minimized), one Chrono Vault per person, every window and dialog remembering its size and
place (D-020), and the four scripts with the Linux packages (tried in Ubuntu 22.04, Debian 12, Fedora 43 and openSUSE
Leap 16) and the Windows setup. Thirteen tests pass on Linux, under the sanitizers and on Windows; the last review
rounds found nothing real. The first version pushed to GitHub and released.

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
