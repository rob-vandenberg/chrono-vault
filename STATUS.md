# Status

## Now

**Version 0.0.5** (D-021): the update. "Check for updates" in Settings > General (Daily, Weekly, Monthly, Never),
checked by itself as set; the About box on F1 with "Check for update". A newer release that carries this system's own
installer (`.deb`, `.rpm` or the Windows setup) is offered with Download or Skip this version; it is downloaded to
Downloads, checked against GitHub's SHA-256 fingerprint, installed through PackageKit on Linux or the setup run silently
on Windows, and the new version starts once this one has ended. The Linux packages need `ca-certificates` and carry
Qt's TLS plugin and OpenSSL's `libssl`; the bare-system checks make one real request to GitHub. Fifteen tests pass on
Linux, under the sanitizers and on Windows; six review rounds, the last found nothing real.

## Next task

The owner tries the update: an installed 0.0.5 finds and installs the next version (0.0.4 has no updater). Then the rest of `ROADMAP.md` steps 3 and 6 to 9, through a changelist and the
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
