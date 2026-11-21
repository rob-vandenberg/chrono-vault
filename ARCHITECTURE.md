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
