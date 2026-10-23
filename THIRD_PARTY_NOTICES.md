# Third-Party Notices — LocalTools Suite 4.0.0

This file is the canonical third-party license record for the LocalTools Suite source
repository and for the relationship between the source tree and the published binaries.

| Field | Value |
|---|---|
| Product | LocalTools Suite 4.0.0 |
| Project source license | **Apache License, Version 2.0** — see [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE) in the repository root; applies to project-owned code only |
| Third-party components | Each remains under its own license, listed below |
| Related record | [`legal/THIRD_PARTY_NOTICES.txt`](legal/THIRD_PARTY_NOTICES.txt) — engineering release record for the Store binary. This root file is canonical for the source/binary relationship. |

## 1. License status of the LocalTools Suite application

- **The project-owned source code is licensed under the Apache License, Version 2.0.**
  The repository's `LICENSE` file carries the canonical upstream text, byte-identical
  to the upstream `LICENSE-2.0.txt` (SHA-256
  `CFC7749B96F63BD31C3C42B5C471BF756814053E847C10F3EB003417BC523D30`) except the
  appendix copyright line, which reads `Copyright 2026 Furkan Ertürk`. The repository's
  `NOTICE` file must be preserved in redistributed derivative works (Apache-2.0 §4(d)).
- The application license **does not remove or reduce any rights** that the third-party
  licenses below independently grant to users of the software. Nothing in this repository
  relicenses the third-party components; each keeps its own license and notices.
- **Distribution model.** Application source code: licensed under the Apache License,
  Version 2.0, as recorded in [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE). Microsoft
  Store binary: distributed under the Microsoft Store listing and its end-user license
  agreement, which governs the packaged product; the compatibility review of that
  agreement found no structural incompatibility; its scope wording now states that the
  agreement governs the Store-distributed binary while the source code is licensed
  under the Apache License, Version 2.0, and the source license governs the source in
  this repository.
  Third-party components: remain under their own licenses, unchanged by this repository.
  None of this is legal advice, and codec patent licensing for media formats is a
  separate matter that no copyright license in this repository grants. The end-user
  license agreement must not restrict reverse engineering that is required to debug the
  LGPL-licensed Qt components (see §3.1).
- Full license texts are present under the `legal/` and `third_party/` paths listed in
  §6 and are linked throughout this document.

## 2. Component overview

| # | Component | Version / pin | License route used | Form of distribution |
|---|---|---|---|---|
| 1 | [Qt 6.11.1 Community Edition](#31-qt-6111-community-edition) | 6.11.1; QtSvg rebuilt with official CVE-2026-8168 patches | LGPL-3.0 (dynamic-linking route) | Dynamically linked DLLs and plugins deployed with the app |
| 2 | [FFmpeg](#32-ffmpeg-81) | 8.1, exact snapshot `n8.1.2-44-g7c533d0f86-20260815` (BtbN `win64-lgpl` build) | LGPL-3.0+ (built with `--enable-version3`) | Standalone child-process executable; **not linked** into the app |
| 3 | [PDFium](#33-pdfium-153079990) | 153.0.7999.0, from `bblanchon/pdfium-binaries`, channel `chromium/7999` | MIT-style package license (Benoit Blanchon) over upstream BSD-style PDFium and its bundled components | Prebuilt `pdfium.dll` shipped with the app, plus all component license files |
| 4 | [qrcodegen](#34-qrcodegen-mit) | Project Nayuki, vendored directly | MIT | Vendored source files in-tree; notice carried in the header |
| 5 | [quirc](#35-quirc-isc) | Pinned upstream commit `927d680904dc95fdff4cd9d022eb374b438ff8f2` | ISC | Vendored files from upstream `lib/`; notices retained |
| 6 | [MinGW-w64 / GCC runtime](#36-mingw-w64--gcc-runtime) | From the build toolchain, deployed by `windeployqt` | GPL-3.0 with GCC Runtime Library Exception; MinGW-w64 notices from the toolchain | Runtime DLLs dynamically linked, deployed next to the app |
| 7 | [Windows Media Foundation](#37-windows-media-foundation-os-component) | OS component | Microsoft OS license | **Not redistributed**; used in place via Qt's playback plugin |

## 3. Component details

### 3.1 Qt 6.11.1 Community Edition

- **License route:** LGPL-3.0, satisfied via the **dynamic-linking** route.
- **Directly linked modules:** `Core`, `Gui`, `Widgets`, `Concurrent`, `Svg`,
  `Multimedia`, `MultimediaWidgets`. `LinguistTools` is build-only and is not shipped.
- **Transitive runtime components:** the packaged runtime also contains
  `Qt6Network.dll`, the TLS plugins, and the NetworkInformation plugin, pulled in as
  transitive Qt Multimedia components.
- **QtSvg modification and corresponding source:** Qt is dynamically linked and its
  DLLs are replaceable by the user; no end-user license term restricts reverse
  engineering needed to debug modifications to LGPL-licensed Qt. QtSvg 6.11.1 is
  rebuilt with the five official CVE-2026-8168 patches. The provenance record shipped
  with the binary (`QtSvg-security-provenance.txt`) lists the patch names and SHA-256
  hashes, while the patch URLs, the source-archive URL and hash, the verification steps
  and the complete build recipe live in this repository:
  [`tools/build_patched_qtsvg.ps1`](tools/build_patched_qtsvg.ps1) pins the official
  QtSvg 6.11.1 source archive and the five official patch files by URL and SHA-256,
  verifies each with `git apply --check` before applying, and builds with CMake and
  Ninja, so the exact patched QtSvg can be rebuilt from this repository. See also
  [`legal/QT_LGPL_RELINKING.md`](legal/QT_LGPL_RELINKING.md).
- **Corresponding Qt source (offer):** the official upstream source is
  <https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/>.
- **Media backend:** Qt uses its Windows Media Foundation playback plugin. Qt's
  optional FFmpeg media plugin is **excluded** from the package.
- **LGPL duties carried by this project:**
  1. dynamic linking of the Qt libraries (in place),
  2. retention of Qt license texts and copyright notices,
  3. provision of, or a standing offer for, the corresponding Qt source,
  4. allowing users to relink or replace the Qt libraries,
  5. the publisher **does not restrict reverse engineering that is needed to debug
     LGPL-licensed Qt**; any EULA must preserve this.

### 3.2 FFmpeg 8.1

- **Exact build:** snapshot `n8.1.2-44-g7c533d0f86-20260815`, the BtbN
  `win64-lgpl` build.
- **Integration:** shipped as a **standalone child process**; the application does not
  link FFmpeg.
- **License:** LGPL-3.0 or later, per the build's `--enable-version3` configuration.
- **License-safety checks (verified on the bundled build):** absence of
  `--enable-gpl`, `--enable-nonfree`, `--enable-libx264`, `--enable-libx265`, and
  `--enable-libxvid`.
- **Measured from the shipped binaries:** the configuration was measured directly from
  the shipped `ffmpeg.exe`/`ffprobe.exe` executables (`-buildconf` output):
  `--enable-version3` is present and `--enable-gpl`, `--enable-nonfree`,
  `--enable-libx264`, `--enable-libx265` and `--enable-libxvid` are absent. The
  upstream-pinned archive asset has since drifted, so the recorded hash in §7
  identifies the exact artifact that was shipped, not the file currently served at
  the upstream download URL.
- **Source / build project:** <https://github.com/BtbN/FFmpeg-Builds> (provides the
  corresponding source and build instructions for this build).
- **Archive checksum:** the download archive is verified against expected
  SHA-256 `061f9868559f2a276dfa770046cf665440b61ef683ec9e98bd45783d72bc02a1`.
- **Obligations:** the FFmpeg LGPL obligations attach to the bundled FFmpeg executable
  itself: it must ship with its license text and be accompanied by (or offered with)
  its corresponding source / build instructions.
- **Important — patents:** the FFmpeg copyright license does **not** resolve codec
  **patent** licensing. A separate market/codec patent review is required and is out
  of scope for this document.

### 3.3 PDFium 153.0.7999.0

- **Package:** `bblanchon/pdfium-binaries`, channel `chromium/7999`. The package's own
  LICENSE is an MIT-style license, copyright 2014–2025 Benoit Blanchon.
- **Upstream source:** <https://pdfium.googlesource.com/pdfium/> (BSD-style license).
- **Checksums (as recorded):**
  - Release asset SHA-256: `55329d5cb5de8a379a2fc563106492d7f385a1f795d18970922c71f708f9fbb4`
  - Bundled `pdfium.dll` SHA-256: `fb898a1f5ace57805834f390407500bdb6ef93eff326a252ad334a8aae809d8e`
- **Bundled component licenses:** kept in
  [`third_party/pdfium/licenses/`](third_party/pdfium/licenses/). For **any binary
  release, every file in that directory must ship**; the existing Store staging maps
  them to `licenses/PDFium-components`.

| License file (in `third_party/pdfium/licenses/`) | Covered component | License |
|---|---|---|
| `abseil.txt` | Abseil C++ | Apache-2.0 |
| `agg23.txt` | agg23 | see file |
| `fast_float.txt` | fast_float | see file |
| `freetype.txt` | FreeType | FTL / GPLv2 (dual) |
| `icu.txt` | ICU | Unicode License |
| `lcms.txt` | Little CMS | see file |
| `libjpeg_turbo.ijg` | libjpeg-turbo (IJG license text) | IJG license |
| `libjpeg_turbo.md` | libjpeg-turbo (BSD-3-Clause parts / build notes) | BSD-3-Clause |
| `libopenjpeg.txt` | OpenJPEG | see file |
| `libpng.txt` | libpng | see file |
| `libtiff.txt` | libtiff | see file |
| `llvm-libc.txt` | LLVM libc++ | Apache-2.0 |
| `pdfium.txt` | PDFium (upstream) | BSD-style |
| `simdutf.txt` | simdutf | see file |
| `zlib.txt` | zlib | see file |

"see file" means the terms are recorded verbatim in that file and are retained
unaltered; they must ship with it in every binary distribution.

### 3.4 qrcodegen (MIT)

- Project Nayuki's qrcodegen, **MIT license**, vendored directly into the tree.
- The MIT copyright notice is carried in the vendored header file and must not be
  removed or altered.

### 3.5 quirc (ISC)

- Daniel Beer's quirc, **ISC license**, pinned to upstream commit
  `927d680904dc95fdff4cd9d022eb374b438ff8f2`.
- Vendored files come from upstream `lib/`; the original notices are retained.
- Comparison against upstream shows **only line-ending and trailing-newline
  formatting differences — no functional modifications.**

### 3.6 MinGW-w64 / GCC runtime

- The compiler runtime libraries are **dynamically linked** and deployed next to the
  application by `windeployqt`.
- The bundled GCC runtime is under **GPL-3.0 with the GCC Runtime Library Exception**;
  the exception text permits distribution of application code built against it.
- The GCC GPL-3.0 license text, the GCC Runtime Library Exception and the MinGW-w64
  notices come from the build toolchain. They are **not vendored in this repository**;
  the release staging step copies them into the package as
  `licenses/GCC-GPL-3.0.txt`, `licenses/GCC-RUNTIME-LIBRARY-EXCEPTION.txt` and
  `licenses/MinGW-w64-COPYING.txt`. Any binary release must ship those three files
  next to the deployed runtime DLLs.

### 3.7 Windows Media Foundation (OS component)

- Qt's multimedia playback uses Windows Media Foundation. WMF is part of Windows,
  **not redistributed** by this project, and carries no bundling obligation here.
  It is subject to the Microsoft OS license on the user's machine.

## 4. Obligations — source release vs. binary Store release

| Obligation | Source release (publishing this repository) | Binary Store release (packaged app) |
|---|---|---|
| Project license — Apache-2.0 `LICENSE` retention | `LICENSE` ships in the repository root; redistributed copies and derivative works of the project-owned source must retain it | Store binary distribution remains under the Store terms; the source license governs this repository (EULA scope correction applied 2026-10-23, see §1 and §8) |
| Project license — `NOTICE` retention (Apache-2.0 §4(d)) | `NOTICE` ships in the repository root and must be preserved by redistributed derivative works alongside `LICENSE` | Store binary distribution remains under the Store terms; the EULA compatibility review covers this (§1) |
| Qt — retain license texts and notices | Notices retained in this file and [`legal/QT_LGPL_RELINKING.md`](legal/QT_LGPL_RELINKING.md) | Ship Qt license texts and notices with the package |
| Qt — corresponding source | Official Qt 6.11.1 submodules URL documented (§3.1) serves as the source offer | The same offer must remain available to users; QtSvg rebuild provenance ships as `QtSvg-security-provenance.txt` |
| Qt — relinking / replacement | N/A (no binaries) | Keep dynamic linking; do not impede relinking or library replacement; instructions in `legal/QT_LGPL_RELINKING.md` |
| Qt — no RE restriction for LGPL debugging | N/A | EULA must permit reverse engineering needed to debug LGPL Qt |
| FFmpeg — license text + corresponding source/build instructions | Repo pins the exact snapshot, checksum, and BtbN build project; obligations attach to the distributed binary, not to the repo itself | Ship the FFmpeg license text with the bundled executable and provide the BtbN corresponding source / build instructions |
| FFmpeg — GPL/nonfree guard | N/A (not vendored) | On any version bump, re-verify absence of `--enable-gpl`, `--enable-nonfree`, `--enable-libx264`, `--enable-libx265`, `--enable-libxvid` |
| PDFium — copyright + license texts | Package LICENSE and all files under [`third_party/pdfium/licenses/`](third_party/pdfium/licenses/) remain in the tree | **Every file** in that directory must ship; existing staging maps them to `licenses/PDFium-components` |
| qrcodegen / quirc — notice retention | Keep the MIT header notice and the ISC notices intact on the vendored files | Notices must accompany the distributed binary per the [`legal/THIRD_PARTY_NOTICES.txt`](legal/THIRD_PARTY_NOTICES.txt) staging record |
| GCC / MinGW-w64 runtime | Not vendored; `windeployqt` deploys it at build time | Bundle the GPL-3.0 text, the GCC Runtime Library Exception, and the MinGW-w64 notices with the deployed runtime DLLs |
| Windows Media Foundation | None | None (not redistributed) |

## 5. Not covered: codec patents

Copyright licenses (Qt, FFmpeg, PDFium and its components) do **not** grant any patent
rights for media codecs. Encoding/decoding formats reachable through FFmpeg and Qt
Multimedia may require separate patent licenses depending on distribution channel and
region. A dedicated market/codec patent review is required; nothing here substitutes
for it.

## 6. Where the full license texts live

| Path | Contents |
|---|---|
| [`LICENSE`](LICENSE) (repository root) | Apache-2.0 license text for the project-owned code (canonical upstream text; appendix copyright line reads `Copyright 2026 Furkan Ertürk`) |
| [`NOTICE`](NOTICE) (repository root) | Project attributions under Apache-2.0 §4(d); must be preserved in redistributed derivative works |
| [`legal/THIRD_PARTY_NOTICES.txt`](legal/THIRD_PARTY_NOTICES.txt) | Engineering release record for the Store binary (staging map and shipped files) |
| [`legal/QT_LGPL_RELINKING.md`](legal/QT_LGPL_RELINKING.md) | Qt LGPL relinking instructions and exact QtSvg 6.11.1 / CVE-2026-8168 provenance |
| [`third_party/pdfium/licenses/`](third_party/pdfium/licenses/) | All 15 bundled PDFium component license files (see §3.3) — ship all of them in binary releases |
| `QtSvg-security-provenance.txt` (packaged runtime directory) | Provenance record for the rebuilt QtSvg shipped to users |
| Packaged `licenses/` directory (not in this repository) | `Qt-LICENSE.txt`, `Qt-third-party-attributions/`, `PDFium-LICENSE.txt`, `FFmpeg-LGPL-3.0.txt`, `GCC-GPL-3.0.txt`, `GCC-RUNTIME-LIBRARY-EXCEPTION.txt`, `MinGW-w64-COPYING.txt`; the exact shipped set is recorded in [`legal/THIRD_PARTY_NOTICES.txt`](legal/THIRD_PARTY_NOTICES.txt) |

## 7. Recorded checksums

| Artifact | SHA-256 |
|---|---|
| FFmpeg build archive (`n8.1.2-44-g7c533d0f86-20260815`) | `061f9868559f2a276dfa770046cf665440b61ef683ec9e98bd45783d72bc02a1` |
| PDFium release asset (153.0.7999.0) | `55329d5cb5de8a379a2fc563106492d7f385a1f795d18970922c71f708f9fbb4` |
| Bundled `pdfium.dll` | `fb898a1f5ace57805834f390407500bdb6ef93eff326a252ad334a8aae809d8e` |

## 8. Corrections applied 2026-10-23

- Line 4 (old): "This product is distributed as a proprietary application that uses and
  redistributes the open-source components listed below."
- Line 20 (old): "No Qt source code was modified for LocalTools Suite."

Both sentences above were replaced on 2026-10-23 in
[`legal/THIRD_PARTY_NOTICES.txt`](legal/THIRD_PARTY_NOTICES.txt): the notice now
describes the application as open-source (source under Apache-2.0) and the shipped
QtSvg module as a security rebuild of the upstream Qt 6.11.1 source plus the five
official CVE-2026-8168 patches and nothing else. That file is embedded into the
shipped binary as a Qt resource, so the currently-published Store package (4.0.0)
still embeds the previous text; the corrected text ships with the next package build.
Until that build ships, this file (`THIRD_PARTY_NOTICES.md`) is the authoritative
record for the repository.

---

*This document is an engineering and release record of license facts as verified at
publication time. It is not legal advice.*
