# Architecture

How Chrono Vault is put together inside. **Only what the owner decided is written here as fact**; what is not yet
decided is marked as a proposal.

## 1. Decided

1. C++20, Qt 6, CMake (D-002).
2. The windows in Qt Quick (QML), one design for all five platforms (D-003).
3. Four kinds of storage, built in this order: a local vault file, WebDAV, pCloud, Google Drive; the technique for
   each is decided when development reaches it (D-005).
4. Changes from two devices are merged; what cannot be merged goes to the person (D-005).
5. Nothing that reads a disk, decrypts or derives a key runs on the thread that draws the window (OR-5).

## 2. The source layout (D-012)

| Folder | What belongs in it |
|---|---|
| `src/core/` | The vault core: plain C++ with Qt Core only, no window code, tested without a screen |
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

## 6. WebDAV storage (D-057)

`src/ui/webdav_client.*` makes the WebDAV requests with Qt Network. `src/ui/webdav_sync.*` syncs one vault's working
copy (an ordinary vault file on this device) with its server: download when the tag changed, merge with
`src/core/vault_merge.*` against the base file, upload under a temporary name, read it back, move it into place under
a lock. Where each vault is kept is in `chrono-vault.conf` (`vaults/webdav`); the password is in the system's store
(`src/ui/secret_store*`). The controller's part is `src/ui/vault_controller_webdav.cpp`; the windows are
`qml/WebDavWindow.qml`, the choice in `qml/WelcomePage.qml` and Settings > Vaults in `qml/SettingsWindow.qml`.
