# Architecture

How Chrono Vault is put together inside. **Only what the owner decided is written here as fact**; what is not yet
decided is marked as a proposal.

## 1. Decided

1. C++20, Qt 6, CMake (D-002).
2. The windows in Qt Quick (QML), one design for all five platforms (D-003).
3. The kinds of storage, built in this order: a local vault file, WebDAV, Google Drive, pCloud (the owner moved Google
   Drive before pCloud, D-080); the technique for each is decided when development reaches it (D-005).
4. Changes from two devices are merged; what cannot be merged goes to the person (D-005).
5. Nothing that reads a disk, decrypts or derives a key runs on the thread that draws the window (OR-5).

## 2. The source layout (D-012)

| Folder | What belongs in it |
|---|---|
| `src/core/` | The vault core: plain C++ with Qt Core only, no window code, tested without a screen |
| `src/ui/vault_controller_ask.cpp` | The standard dialog's question: proves a master password, PIN or finger for any vault without touching the one shown (D-073) |
| `style/` | Chrono Vault's Qt Quick Controls style: Fusion with every size times the UI scaling and every text enlarged by exactly it (D-023, D-069); rows and columns are `ExactRow` and `ExactColumn` |
| `qml/` | The windows, one QML module (URI `ChronoVault`) in Qt Quick; they only call the core |
| `src/app/` | `main.cpp`, which starts the windows |
| `tests/unit/` | Tests of one class each, Qt Test under CTest |
| `tests/integration/` | Tests of pieces together, run without a screen |
| `cmake/` | Build settings shared by our own targets: warnings as errors, the sanitizer build |

The version lives only in the `project(...)` line of `CMakeLists.txt` (CPP-2); the core reports it.

## 3. Not yet decided

1. The encryption method and the key derivation (`REQUIREMENTS.md` 9.1).
2. The format of the vault file (9.2).

## 4. What a vault holds (D-015, D-027 to D-029)

1. `src/core/vault_contents.*`: items, the person's own categories and templates, the hidden categories; read and
   written as JSON inside the sealed vault (contents format 3; formats 1 and 2 still read).
2. `src/core/templates.*`: the eight built-in categories and thirteen built-in templates, and the names a person sees.
3. `src/ui/vault_controller.*`: what the windows call; it reads files for attachments, writes them where the person
   chooses, and puts the contents into JSON for a save, all off the window's thread (OR-5).
4. `src/ui/item_list_model.*`: the item list, narrowed to Home, Favorites, a category or a tag.
5. `qml/CustomizePage.qml` and `qml/TemplateEditor.qml`: Settings > Customize.

## 5. Merging (D-054)

`src/core/vault_merge.*` merges two `VaultContents` against a base, without disk or keys. The controller
(`src/ui/vault_controller.cpp`) keeps the base (the version last read or written), reads the other version on the
save thread when the vault file's stamp changed, merges on the window's thread, and saves the result with the usual
atomic, verified save.

**Open, Closed, Locked (D-075).** `VaultController::lock` locks the open vault and the key store (the primary's key and the
kept passwords, in memory); `closeOpenVault` and `lockVault` (`src/ui/vault_controller_vaults.cpp`) work on one vault.
`returnTo` is the vault that opens by itself once the primary is unlocked after a lock (`goBackThroughPrimary`).
**The screen lock (D-076)**, `src/ui/vault_controller_pin.cpp`: `sealSession` puts what is open away under the vault logged in
with, sealed with its key and its PIN's; `takeBackSession` and `openTakenBack` give it back. A PIN per vault in `pins/`.
**Open a vault (D-077)**: `qml/StorageWindow.qml` in its "open" mode lists the known vaults; a chosen one goes to
`openKnownAsk` in `qml/Main.qml`, the standard dialog as Settings > Vaults uses it.
**Switching without a page between (D-078)**: `VaultController::startSwitch` makes the key and reads the other vault off the
window's thread while the open one stays (`switching`, the shields in `qml/Main.qml` and `qml/SettingsWindow.qml`); the swap is
one step on the window's thread; a failure leaves the open vault and says `switchFailed`.

## 6. WebDAV storage (D-057)

`src/ui/webdav_client.*` makes the WebDAV requests with Qt Network; `src/ui/webdav_storage.*` is WebDAV as a cloud
storage (section 7), and `src/ui/webdav_sync.*` keeps where a vault is on its server. `src/ui/cloud_sync.*` syncs one vault's working
copy (an ordinary vault file on this device) with its server: download when the tag changed, merge with
`src/core/vault_merge.*` against the base file, upload under a temporary name, read it back, move it into place under
a lock. Where each vault is kept is in `chrono-vault.conf` (`vaults/webdav`); the password is in the system's store
(`src/ui/secret_store*`). The controller's part is `src/ui/vault_controller_webdav.cpp`; the windows are
`qml/WebDavWindow.qml`, the choice in `qml/WelcomePage.qml` and Settings > Vaults in `qml/SettingsWindow.qml`.

## 7. Cloud storage (D-080)

`src/ui/cloud_storage.*` is the base of every storage (`CloudStorage`): sign in, list a folder, download, upload,
delete, make a folder, a file's version tag, put an upload in place, and a lock where the storage has locks; with
`CloudPlace`, where one vault is kept (`vaults/<kind>` in `chrono-vault.conf`, its secret in the system's store as
`<kind>-<id>`), and `cloudKinds()`, the one list of storages this build has, which every window that names a storage
reads. `CloudSync` (`src/ui/cloud_sync.*`) is the one sync for all of them. Each storage is a class of its own:
`WebDavStorage` (`src/ui/webdav_storage.*`) and `GoogleDriveStorage` (`src/ui/google_drive_storage.*`, with
`GoogleSignIn`, the browser sign-in with Qt Network Authorization). Google Drive is in a build only when the build was
given the program's Google client (`CHRONO_VAULT_GOOGLE_CLIENT_ID` and `_SECRET`, written into the build folder's
`google_drive_client.h`, never into git). The controller's Google Drive part is `src/ui/vault_controller_googledrive.cpp`;
its window `qml/GoogleDriveWindow.qml`, with the same file dialog as WebDAV (`qml/WebDavFileDialog.qml`).
