# Status

## Now

**Version 0.1.42**: Esc as Chrono Viewer has it, so it closes the About box also with "Use the Esc key to exit the
program" ticked (D-052). Before, 0.1.41: arrow keys show the item; the cards on the theme's Base colour; the one Notes box per item; the
reference program's twenty field types; our own switch (D-051); `iconPath` for Microsoft's compiler. Before, 0.0.40: the icon picker: the vault's own icons kept in the vault, the shipped icons from `icons/` as
`icons.md` lists them, copied into the person's own folder; the editor's Add, Save and Cancel; the reminder below
Change the Master Password (D-050). Before, 0.0.39: Settings tidied: Autolock's four tick boxes in a frame, the master password reminder every Day,
Month, Quarter (default), Year or Never, "Clear the clipboard after [30] seconds"; normal scroll bars with arrows in
the settings window and the About box; collapsed sidebar entries as tall as expanded; Esc closes the About box; the
Windows setup builds again (D-049). Before, 0.0.38: two files, the vaults in `chrono-vault.conf` and everything else in `chrono-vault.ini`, both in
the program's folder (Windows too, no registry); `--reset` deletes the `.ini` (D-026); no carry-over from earlier
versions. "Use the Esc key to exit the program" in Settings > General (D-048).

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
