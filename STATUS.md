# Status

## Now

**Version 0.0.6** (D-022): the settings window as in Chrono Viewer: a framed sidebar with the chosen topic marked
across its width, and Restore defaults, OK and Cancel at the bottom right. OK (and Enter) keeps every topic's changes,
Cancel (Esc, closing) throws them away, Restore defaults puts back the topic shown. A failed "Start at startup" keeps
the window open and says so. Also the release to try the update from 0.0.5 (D-021). Fifteen tests pass on Linux, under
the sanitizers and on Windows.

## Next task

The owner tries the update from 0.0.5 to 0.0.6 on Linux and Windows. Then the owner's two new requirements, as in
Chrono Viewer: a theme (Light, Dark, Follow the system) and UI scaling, through a changelist and the owner's Proceed.

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
