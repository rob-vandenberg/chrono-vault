# Status

## Now

**Version 0.0.9** (D-024, D-025): an update is offered in a popup as in Chrono Mail (Download, Skip this update,
Cancel; then Update downloaded with Install; on Linux Restart or Close), no longer as a line at the bottom. The main
window follows the reference program's layout: one bar with the menus File, View, Item, Tools and Help and the
buttons New Item, Generate Password, lock and preferences; three columns with the vault, the items under letter
headings and the chosen item read-only, the editor behind the pencil. The About box's name, copyright and GitHub link
line up with the icon. Seventeen tests pass on Linux; two review rounds, the last found nothing real.

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
