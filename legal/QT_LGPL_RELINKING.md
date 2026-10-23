# Qt LGPL replacement and debugging instructions

LocalTools Suite 4.0.0 uses Qt 6.11.1 Community Edition under the GNU Lesser
General Public License version 3. Qt libraries are dynamically linked. QtSvg
6.11.1 is rebuilt from the official source with Qt's five official
CVE-2026-8168 patches; application-authored changes are not added to Qt.

This document explains an engineering path for exercising LGPL rights. It does
not limit any rights granted by the applicable open-source licenses.

## Obtain the corresponding Qt source

The official Qt 6.11.1 module archives are available at:

https://download.qt.io/official_releases/qt/6.11/6.11.1/submodules/

The exact QtSvg source URL, source SHA-256, official patch URLs/hashes and
resulting binary hashes are recorded in
`licenses/QtSvg-security-provenance.txt`. The reproducible preparation command
is `tools/build_patched_qtsvg.ps1` in the corresponding source release.

The package's `licenses/Qt-LICENSE.txt` contains the applicable license text.
The Store package uses Qt Multimedia's Windows Media Foundation plugin. Qt's
optional FFmpeg media plugin and FFmpeg shared libraries are not packaged.

## Build a compatible modified Qt

1. Apply the five recorded official CVE-2026-8168 patches when reproducing the
   packaged QtSvg module, then build the Qt 6.11.1 modules used by LocalTools
   Suite for Windows x64 with a binary-compatible MinGW toolchain and the same
   release configuration.
2. The relevant application-facing libraries are `Qt6Core.dll`, `Qt6Gui.dll`,
   `Qt6Widgets.dll`, `Qt6Network.dll`, `Qt6Svg.dll`, `Qt6Multimedia.dll` and
   `Qt6MultimediaWidgets.dll`. Corresponding Qt plugins are stored in the named
   subdirectories beside the executable.
3. Keep the original package as a backup. Unpack the MSIX with Microsoft's
   `MakeAppx.exe unpack /p <package.msix> /d <folder>` or another format-aware
   archive tool into a user-writable folder.
4. Replace the LGPL-covered Qt DLLs/plugins in that unpacked folder with the
   compatible modified build. Do not modify the signed installed package in
   place; its integrity protection will reject changed bytes.
5. Run `LocalTools.exe` from the unpacked folder. Windows may show security
   warnings for locally modified or unsigned binaries. Those warnings are a
   platform trust response, not an application-imposed restriction.

The publisher does not prohibit reverse engineering needed to debug changes to
LGPL-covered libraries and does not apply application-level DRM to prevent this
workflow. Modified libraries are unsupported and remain governed by their own
licenses; users are responsible for the security of their builds.

For an accepted release, the exact package hash and dependency inventory are
recorded separately so users can identify the corresponding version.
