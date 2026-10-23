# FFmpeg runtime provenance

LocalTools invokes FFmpeg as a standalone child process. It does not link the application binary to FFmpeg libraries.

- Upstream build project: https://github.com/BtbN/FFmpeg-Builds
- FFmpeg release family: 8.1
- Asset: `ffmpeg-n8.1-latest-win64-lgpl-8.1.zip`
- Variant: Windows x64, LGPL, static standalone executables
- Asset publication observed: 2026-08-15
- Imported on: 2026-08-16
- Expected SHA-256: `061f9868559f2a276dfa770046cf665440b61ef683ec9e98bd45783d72bc02a1`
- Fetch script: `tools/fetch_ffmpeg.ps1`

The fetch script fails closed when the rolling upstream asset changes. A maintainer must then review the new build configuration, update the checksum and rerun the codec/license audit; the hash must never be updated blindly.

Release policy:

- GPL and nonfree FFmpeg builds are prohibited.
- GPL-only encoders such as `libx264` and `libx265` are prohibited.
- Store staging must use only the verified `third_party/ffmpeg/bin/ffmpeg.exe` and `ffprobe.exe` files.
- The final package must include FFmpeg license notices and corresponding-source instructions.
