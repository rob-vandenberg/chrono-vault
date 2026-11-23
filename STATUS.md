# Status

## Now

**Version 0.3.62**: Select Storage with a tile per storage; the WebDAV server chosen from a list, the folder in a browser with New folder; a new WebDAV vault kept only after its first upload; leftover uploads removed; pCloud's new tags no longer seen as changes; the server's address on the unlock page; Save closes the editor (D-057 points 20-26, D-034). Before, 0.3.61: the first upload to pCloud no longer ends as "Busy": a lock's empty placeholder is no version (D-057 point 6); every WebDAV request and sync step in the `--debug` log. Before, 0.3.60: WebDAV storage, `ROADMAP.md` step 11 (D-057): a vault kept on a WebDAV server, with a working copy on this device, synced at every unlock, every 10 minutes and within 5 seconds of a save, merged as D-054, uploaded only under a lock and never over another device's version; New vault… and Open vault… ask where; Settings > Vaults with Move to WebDAV… and Disconnect…; Tools > Sync Now (Ctrl+J); the password in the system's store; an untrusted certificate trusted by its fingerprint; plain http with a warning. Before, 0.2.59: the item editor's labels and fields 50 pixels to the left, as the view (the owner). Before, 0.2.58: the Edit category dialog's tick box reads "Show in sidebar", under "Name:" (the owner, D-056). Before, 0.2.57: a Search field in Choose Icon, by name, any case, anywhere in the name (D-050); the Edit category dialog with the icon at the left and a pen badge (D-056). Before, 0.2.56: every category can be edited (name, icon, shown), Reset for a built-in one (D-056); the owner's icons ship, in folders inside folders, named by their file names (D-050). Before, 0.2.55: the font is 10 points on every system, between Windows' 9 and the owner's Linux 11 (the owner, D-017). Before, 0.2.54: the font is 11 points on every system, no longer the system's size, so Chrono Vault looks the same on Linux and Windows under the same scaling (the owner, D-017). Before, 0.2.53: on Windows, turning on Windows Hello opens no box of our own; a red "Waiting for Windows Hello fingerprint..." at the bottom left of the settings window, and Hello opens over Chrono Vault where Windows can (D-038 point 10). Before, 0.2.52: on Windows every file dialog is Explorer's own (D-055); Settings > Security's reminder reads "Ask for the master password again, once every", Change stands right after its text, and the clipboard field is 40 pixels narrower (the owner). Before, 0.2.51: every file dialog as Chrono Viewer's (D-055). Before, 0.2.50: keys a text field never uses are commands while typing (the owner). Before, 0.2.49: narrower Security fields; the sidebar snap at 80 (the owner). Before, 0.2.48: the sidebar snaps after 100 pixels of travel each way (the owner). Before, 0.2.47: item counts above 0 only, long names cut off, the sidebar snap (the owner). Before, 0.2.46: the main window's shortcuts only while it has the keyboard (P33); the owner's layout changes. Before, 0.2.45: merging, `ROADMAP.md` step 10 (D-054). Before, 0.1.44: Settings > Keyboard shortcuts as Chrono Viewer's: every key pressed in the key box is the new key, and a key in use is asked about (D-053). Before, 0.1.43: every dialog also takes Esc in its own keys (D-052). Before, 0.1.42: Esc as Chrono Viewer has it, so it closes the About box also with "Use the Esc key to exit the
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

As the owner chooses: `ROADMAP.md` step 12 (Audit), or the backlog (PIN unlock first).

## Open questions

1. Which other password managers' exports are imported (9.3).
2. Whether a lost master password can be recovered in any way (9.4, OR-15).
3. Unlocking with a fingerprint or face (9.5).
4. Exporting a vault (9.6).
5. How a release is built for Android, macOS and iOS (9.7); macOS and iOS need a Mac with Xcode (D-009).
6. The technique for pCloud and Google Drive, decided when each step is reached (9.8, D-005); WebDAV's is D-057.
7. A push guard against naming the reference program, and the personal-data scan of `gitpush.sh`, before any first
    push (OR-7, OR-10).
8. How the settings window looks on a phone screen, decided at `ROADMAP.md` step 17 (D-011).

Before asking the owner any of these, the coder checks how the reference program does it (OR-16).
