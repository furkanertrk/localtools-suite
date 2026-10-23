# Building LocalTools Suite

## Overview

LocalTools Suite is a desktop application (version 4.0.0) written in C++20, built with
Qt 6 (Qt 6.11.1 Community, MinGW 64-bit kit), CMake, and Ninja, targeting Windows x64.

The release audited for the Microsoft Store toolchain was:

- Qt 6.11.1 (Community edition, `mingw_64` kit)
- MinGW GCC 13.1.0 (Qt Tools `mingw1310_64`)
- CMake + Ninja

`CMakeLists.txt` accepts Qt >= 6.0, but the audited release was produced with Qt
6.11.1; other Qt 6 versions are not covered by that audit.

**Important:** a clean clone intentionally does **not** include the PDFium DLL and
import library, nor the FFmpeg executables. Both are third-party binaries that must be
obtained separately (see *Obtain dependencies*) before configuring the project.

**Important:** the documents under `legal/` are **build inputs, not optional
documentation**. `resources/resources.qrc` embeds five files under `legal/`
(`PRIVACY_POLICY_EN.md`, `PRIVACY_POLICY_TR.md`, `THIRD_PARTY_NOTICES.txt`,
`QT_LGPL_RELINKING.md`, `END_USER_LICENSE.md`) as Qt resources, and `CMakeLists.txt`
also installs the `legal/` directory. If any of them is missing, the build fails at
code generation with:

```text
ninja: error: '<repo>/legal/END_USER_LICENSE.md', needed by 'LocalTools_autogen/.../qrc_resources.cpp', missing and no known rule to make it
```

An incomplete copy of the repository that drops `legal/` cannot build; this exact
failure cost an entire earlier build attempt.

## Prerequisites

- Windows x64.
- Qt 6.11.1 Community with the MinGW 64-bit (`mingw_64`) kit installed. The following
  Qt runtime modules/components are required by the application:
  - Core, Gui, Widgets, Concurrent, Svg, Multimedia, MultimediaWidgets
  - LinguistTools (needed to build the translation files)
  - Note: the application does not link Qt Network directly, but Qt Network is
    transitively deployed by Qt Multimedia by the deployment tooling.
- MinGW GCC 13.1.0 (install the Qt Tools `mingw1310_64` package, or match its version).
- CMake and Ninja.
- PowerShell (used by the dependency fetch script).
- An extractor for `.tgz` archives: the built-in Windows `tar` command (Windows 10 and
  later) handles `.tgz` fine, or use 7-Zip.
- The complete `legal/` directory with its five documents. They are embedded as Qt
  resources and are required to build; see the note under *Overview*.

The commands below use path placeholders. Set them to your actual installation paths;
do not assume any specific user directory. Use forward slashes in these values: they
end up in CMake `-D` arguments, where backslash-separated paths make CMake abort with
`Invalid character escape '\X'`.

```powershell
$QtRoot='C:/Qt/6.11.1/mingw_64'
$QtTools='C:/Qt/Tools'
```

## Obtain dependencies

### PDFium (required for configure)

The project requires the prebuilt PDFium binaries:

- Version: **153.0.7999.0**
- Source: the `bblanchon/pdfium-binaries` GitHub repository, immutable release tag
  `chromium/7999`
- Asset: `pdfium-win-x64.tgz`
- Asset SHA256:
  `55329d5cb5de8a379a2fc563106492d7f385a1f795d18970922c71f708f9fbb4`

Download the asset and verify the checksum (e.g. `Get-FileHash -Algorithm SHA256`).
Then extract the archive into `third_party/pdfium` so that the following layout
exists (the archive's top-level entries go directly into `third_party/pdfium`):

```text
third_party/pdfium/
  include/
  bin/pdfium.dll
  lib/pdfium.dll.lib
  PDFiumConfig.cmake
  VERSION
```

The built-in Windows `tar` can extract the `.tgz` directly, for example:

```powershell
New-Item -ItemType Directory -Force third_party\pdfium | Out-Null
tar -xzf pdfium-win-x64.tgz -C third_party\pdfium
```

(7-Zip works equally well if you prefer it.)

The packaged `bin/pdfium.dll` is expected to have SHA256
`fb898a1f5ace57805834f390407500bdb6ef93eff326a252ad334a8aae809d8e`.

If `third_party/pdfium` is missing or incomplete, `find_package` fails and CMake
configure aborts before generating a build system.

### FFmpeg (required for `smoke_test`, media tests and Release configure)

FFmpeg is fetched with the bundled script:

```powershell
powershell -ExecutionPolicy Bypass -File tools\fetch_ffmpeg.ps1
```

The script downloads:

- URL:
  `https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-8.1.zip`
- Expected archive SHA256:
  `061f9868559f2a276dfa770046cf665440b61ef683ec9e98bd45783d72bc02a1`

**Measured on 2026-10-23: the unmodified script aborts.** The rolling `latest` asset
(Last-Modified 2026-10-22) hashes to
`d33ead5ed152ce6e2beb6d8b0b209984acfd9e9513e2a55b6855e6ab0956b5ef`, not the expected
value above, so the script's checksum gate currently blocks every fresh build. The
script is behaving correctly; the upstream artifact it pins has drifted.

The runtime version recorded during the audit is the snapshot
`n8.1.2-44-g7c533d0f86-20260815`. Its build configuration contains
`--enable-version3` but **not** `--enable-gpl`, `--enable-nonfree`,
`--enable-libx264`, `--enable-libx265`, or `--enable-libxvid` (it is an LGPL build).
That exact snapshot is **not retrievable** from the documented source: measured on
2026-10-23, upstream keeps only the rolling `latest` asset plus month-end autobuild
tags (July 31 = `n8.1.2-34`, August 31 = `n8.1.2-50`). The Store-shipped FFmpeg
runtime therefore cannot be re-downloaded — a reproducibility gap (see *Known
reproducibility gaps*).

**Measured working alternative (used in the 2026-10-23 clean-clone run):** the
immutable autobuild asset

- Release tag: `autobuild-2026-08-31-13-27`
- Asset URL:
  `https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-08-31-13-27/ffmpeg-n8.1.2-50-g1a748fe2cd-win64-lgpl-8.1.zip`
- Archive size: 146,078,616 bytes
- Archive SHA256:
  `f6274bbd9c247f9e90c1bbed066b03ed4a3907cece2fb91be6dd352393936365`
- Version: `n8.1.2-50-g1a748fe2cd-20260831`
- `bin/ffmpeg.exe` SHA256:
  `9c60da6c0b083110d59084ea39f60ae149aa3e031c3b4bb4f573fafa1c1e7cea`

Its `-buildconf` was audited: `--enable-version3` is present; `--enable-gpl`,
`--enable-nonfree`, `--enable-libx264`, `--enable-libx265` and `--enable-libxvid` are
absent. It is the same LGPL family as the release runtime, but **not byte-identical**
to it.

Manual provisioning steps for this asset:

1. Download the archive and verify its SHA256 against
   `f6274bbd9c247f9e90c1bbed066b03ed4a3907cece2fb91be6dd352393936365`
   (e.g. `Get-FileHash -Algorithm SHA256`).
2. Extract the archive and copy `bin/ffmpeg.exe` and `bin/ffprobe.exe` into
   `third_party/ffmpeg/bin/`.

After provisioning, `third_party/ffmpeg/bin/ffmpeg.exe` and
`third_party/ffmpeg/bin/ffprobe.exe` must exist. The **Release** configure step fails
if either executable is absent, and `smoke_test` asserts at runtime that an FFmpeg
runtime is available and fails without it (measured:
`[FAIL] hasFfmpeg at tests/smoke_test.cpp:592`).

Re-pinning `tools/fetch_ffmpeg.ps1` to an immutable asset (URL plus matching
checksum) is a maintainer task. Do not weaken or delete the script's checksum check.

How the tests find their inputs (already wired in `CMakeLists.txt`):

- `smoke_test` and `release_performance_test` receive the absolute path of the
  repository-root `test.pdf`, and `pdfium.dll` plus `test.pdf` are copied next to the
  `smoke_test` executable after the build.
- `ffmpeg_process_test` and `windows_media_backend_test` receive the absolute path
  `third_party/ffmpeg/bin/ffmpeg.exe`, so the FFmpeg runtime has to exist under
  `third_party/ffmpeg/bin/`.
- `smoke_test` additionally asserts that an FFmpeg runtime is available, so FFmpeg is
  required to run the suite, not only for a Release configure.
- Every test runs with the build directory as its working directory;
  `release_performance_test` writes `performance-results.json` there.
- A CMake post-build step copies the FFmpeg executables beside the **application**
  binary. Build the app target — or provision `third_party/ffmpeg/bin/` as described
  above — before running the suite; see *Run tests*.

## Build Debug

From the repository root:

```powershell
$QtRoot='C:/Qt/6.11.1/mingw_64'
$QtTools='C:/Qt/Tools'
$env:PATH="$QtRoot/bin;$QtTools/mingw1310_64/bin;$QtTools/Ninja;$env:PATH"

cmake -B build-debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH="$QtRoot" `
  -DCMAKE_C_COMPILER="$QtTools/mingw1310_64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="$QtTools/mingw1310_64/bin/g++.exe" `
  -DCMAKE_RC_COMPILER="$QtTools/mingw1310_64/bin/windres.exe" `
  -DCMAKE_MAKE_PROGRAM="$QtTools/Ninja/ninja.exe"

cmake --build build-debug
```

The forward slashes in the variable values are required, not cosmetic: defining
`$QtRoot`/`$QtTools` with backslash paths (e.g. `C:\Qt\6.11.1\mingw_64`) expands them
into the `-D` arguments and makes configure abort with `Invalid character escape '\Q'.`
(measured with this command's earlier backslash form). The forward-slash form above
configures cleanly — 34 tests registered in ~11 s (measured 2026-10-23, Windows 11 x64,
Qt 6.11.1 `mingw_64`, GCC 13.1.0, CMake, Ninja 1.12.1).

For a Release build, use the same commands with `-DCMAKE_BUILD_TYPE=Release` (and a
separate build directory such as `build-release`). Remember that Release configure
requires the FFmpeg executables to be present.

## Run tests

```powershell
ctest --test-dir build-debug --output-on-failure
```

There are **34 CTest cases** in total:

- 10 standalone tests, and
- a UI test matrix of 24 cases: 4 scale values x 3 themes x 2 languages.

Test requirements:

- All 34 cases run from the build directory and need the provisioned PDFium runtime.
- An FFmpeg runtime is required for `smoke_test`, `ffmpeg_process_test` and
  `windows_media_backend_test`, and for the Release configure step — not only for
  Release. Measured on 2026-10-23: without an FFmpeg runtime `smoke_test` fails with
  `[FAIL] hasFfmpeg at tests/smoke_test.cpp:592`.
- With a Debug build the CMake post-build step copies FFmpeg next to the application,
  so build the app target (or provision `third_party/ffmpeg/bin/` as described under
  *Obtain dependencies*) before running the suite.
- The UI matrix cases set `QT_SCALE_FACTOR` and the `LOCALTOOLS_TEST_THEME` /
  `LOCALTOOLS_TEST_LANGUAGE` environment variables. Headless execution has not been
  verified in this preparation.

## Release and Store packaging

A store-ready Release build is produced with the same CMake/Ninja flow using
`-DCMAKE_BUILD_TYPE=Release`, after which Qt deployment (windeployqt, using the Qt
modules listed under *Prerequisites*) places the required Qt runtime next to the
executable; Qt Network is included transitively because Qt Multimedia depends on it.

The repository also includes two packaging scripts:

- `tools/build_release.ps1` — assembles the packaged application (including PDFium and
  FFmpeg binaries).
- `tools/build_msix.ps1` — builds the MSIX package for Microsoft Store submission.

**These scripts are not reproducible on an arbitrary clean machine.** They assume:

- a specific `C:\Qt-OSS` Qt installation layout,
- a locally patched QtSvg artifact addressing CVE-2026-8168,
- additional legal files that are not part of this repository,
- specific SDK assumptions, and
- a fixed Store partner identity.

A Store build from clean source is therefore **not guaranteed**, and the packaging
flow described here has **not been verified in this document's audit**. Treat the
scripts as references for the intended packaging shape, not as turnkey steps.

## Troubleshooting

- **CMake configure aborts immediately with a `find_package` error for PDFium** —
  `third_party/pdfium` is missing or incomplete. Extract the archive so that
  `include/`, `bin/pdfium.dll`, `lib/pdfium.dll.lib`, `PDFiumConfig.cmake`, and
  `VERSION` exist under `third_party/pdfium`.
- **CMake configure aborts with `Invalid character escape '\X'`** — backslash-separated
  Windows paths reached the `-D` arguments (a value like `C:\Qt\...` expanded into
  them). Use forward slashes for the paths given to CMake, as shown under
  *Build Debug*.
- **Release configure fails mentioning ffmpeg.exe/ffprobe.exe** — place the FFmpeg
  binaries under `third_party/ffmpeg/bin/` before configuring the Release build.
  As measured on 2026-10-23, `tools/fetch_ffmpeg.ps1` aborts with a checksum mismatch
  against the rolling `latest` asset; use the manual provisioning steps under
  *Obtain dependencies* instead.
- **`ninja` or compiler not found during configure** — ensure
  `$QtTools\mingw1310_64\bin` and `$QtTools\Ninja` are on `PATH`, or pass
  `-DCMAKE_MAKE_PROGRAM` and the compiler paths explicitly as shown above.
- **Translation build fails** — the Qt LinguistTools component is not installed for
  your Qt kit; install it via the Qt Maintenance Tool.
- **Smoke test fails** — `test.pdf` is not present for the test, or no FFmpeg runtime
  is available: `smoke_test` itself asserts FFmpeg availability
  (`[FAIL] hasFfmpeg at tests/smoke_test.cpp:592`), as do the media tests; see
  *Run tests* for how FFmpeg reaches the suite.
- **`tar` cannot extract the PDFium archive** — use 7-Zip instead; the expected
  extracted layout is listed under *Obtain dependencies*.
- **Deployed app reports a missing Qt Network DLL** — this is expected behavior of
  Qt Multimedia deployment; ensure the deployment step ran against a complete
  `mingw_64` Qt installation.

## Known reproducibility gaps

This document describes how the audited release was built; the following items are
known to limit reproduction from a clean clone:

1. Third-party binaries (PDFium DLL/import library, FFmpeg executables) are
   intentionally not committed and must be fetched manually or via
   `tools/fetch_ffmpeg.ps1`.
2. `tools/build_release.ps1` and `tools/build_msix.ps1` are not clean-machine
   reproducible (hardcoded `C:\Qt-OSS` layout, patched QtSvg CVE-2026-8168 artifact,
   external legal files, SDK assumptions, fixed partner identity). The Store package
   is not guaranteed to be rebuildable from this source tree alone.
3. The FFmpeg pin has drifted and the pinned snapshot is unrecoverable upstream.
   Measured on 2026-10-23, the rolling `latest` asset no longer matches the checksum
   pinned in `tools/fetch_ffmpeg.ps1`, so the script's checksum gate blocks every
   fresh fetch, and the audited snapshot `n8.1.2-44-g7c533d0f86-20260815` cannot be
   re-downloaded because upstream keeps only `latest` plus month-end autobuild tags
   (July 31 = `n8.1.2-34`, August 31 = `n8.1.2-50`). The Store-shipped FFmpeg runtime
   therefore cannot be re-obtained from the documented source.
   `tools/fetch_ffmpeg.ps1` needs a maintainer re-pin to an immutable asset; a
   measured working alternative is documented under *Obtain dependencies*.
4. Only the audited toolchain listed in *Overview* (Qt 6.11.1, GCC 13.1.0) has been
   exercised for the release, even though CMake accepts any Qt >= 6.0.
5. `src/core/backends/opencv_backend.cpp/.h` is present in the tree and `#include`s
   OpenCV headers, but it is **not** listed in any `add_executable`/`add_library`
   source list and OpenCV is not a configured dependency, so the file is never
   compiled and OpenCV does not need to be installed. Its behaviour is provided by
   `native_vision_backend.cpp`. The file is published unchanged; removing or wiring it
   up is a separate code decision.
6. The five `legal/` documents are build inputs, not optional documentation: they are
   embedded as Qt resources via `resources/resources.qrc` and installed by
   `CMakeLists.txt`. An incomplete copy of the repository that drops `legal/` cannot
   build.

## Verified clean-clone result

On 2026-10-23 a clean copy of this tree was built end to end: after provisioning
PDFium (the `55329d5c…` archive and the `fb898a1f…` `pdfium.dll` — both checksums
matched) and the alternative FFmpeg asset documented under *Obtain dependencies*, the
project configured in ~11 s, built 248/248 steps in ~160 s, and passed 34/34 CTest
cases, including the 24-case UI matrix (Windows 11 x64, Qt 6.11.1 `mingw_64`,
GCC 13.1.0, CMake, Ninja 1.12.1). This is one observed run on one machine, not a CI
guarantee.
