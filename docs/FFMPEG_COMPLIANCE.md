# FFmpeg 8.1 Release Compliance Record

## Approved artifact

- Build project: `BtbN/FFmpeg-Builds`
- Artifact: `ffmpeg-n8.1-latest-win64-lgpl-8.1.zip`
- Architecture: Windows x64
- Expected archive SHA-256: `061f9868559f2a276dfa770046cf665440b61ef683ec9e98bd45783d72bc02a1`
- Local integration: standalone `ffmpeg.exe` and `ffprobe.exe` child processes;
  LocalTools Suite does not link FFmpeg libraries.

`tools/fetch_ffmpeg.ps1` pins and verifies the archive. A changed upstream hash
must fail closed and trigger a fresh review; the expected hash must not be
updated without inspecting the new build.

Qt 6.11.1 also offers an FFmpeg 7.1.3 media-player plugin, but the Store staging
explicitly excludes that plugin and its shared libraries. In-app playback uses
the Windows Media Foundation plugin, verified by `windows_media_backend_test`.
This removes the older decoder set from the package without changing the
standalone command-line runtime used for conversion.

## Allowed release policy

- Only the BtbN `win64-lgpl` variant may enter staging.
- GPL and `nonfree` builds are prohibited.
- GPL-only encoders, including `libx264` and `libx265`, must not be advertised or
  selected by LocalTools Suite.
- The release audit records `ffmpeg -version`, build configuration, codec list
  and hashes from the exact staged executables.
- The package contains the LGPL text, source/build URL and third-party notice.
- Qt's `ffmpegmediaplugin.dll` and `avcodec-61` family must be absent from Store staging.

## Security review note — 2026-08-16

NVD CVE-2026-64831 identifies a Vulkan HEVC decoder stack overflow in upstream
FFmpeg 8.0 through the 8.1.2 release. The pinned executable reports snapshot
`n8.1.2-44-g7c533d0f86-20260815`. Official FFmpeg source at that revision contains
the bounds check introduced by fix commit
`92737390dc133daadce47dd7d2ec8ef3d9ebcbed`, so the version-only affected range
does not describe this post-release snapshot accurately. The build revision and
archive hash remain pinned; replacing either requires a fresh source comparison.

## Patent and codec note

An open-source copyright license does not grant every possible patent right.
The publisher must review the markets and codecs offered in the Store listing.
This engineering record cannot substitute for jurisdiction-specific legal advice.

## Required release evidence

The following files are generated beside the staging manifest:

- `ffmpeg-version.txt`
- `ffmpeg-codecs.txt`
- `dependency-inventory.txt`
- `staging-manifest-sha256.txt`

The R5 gate remains open if the staged binary differs from the approved artifact
or reports a GPL/nonfree configuration.
