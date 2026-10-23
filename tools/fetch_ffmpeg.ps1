param(
    [string]$DestinationRoot = (Join-Path $PSScriptRoot "..\third_party\ffmpeg")
)

$ErrorActionPreference = "Stop"

# Standalone LGPL build: the application launches ffmpeg.exe/ffprobe.exe as
# child processes and does not link against FFmpeg libraries.
$assetUrl = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-8.1.zip"
$expectedSha256 = "061f9868559f2a276dfa770046cf665440b61ef683ec9e98bd45783d72bc02a1"
$assetName = "ffmpeg-n8.1-latest-win64-lgpl-8.1.zip"

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$resolvedDestination = [System.IO.Path]::GetFullPath($DestinationRoot)
if (-not $resolvedDestination.StartsWith($repositoryRoot + [System.IO.Path]::DirectorySeparatorChar,
                                         [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "FFmpeg destination must remain inside the repository: $resolvedDestination"
}

$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("LocalTools-ffmpeg-" + [guid]::NewGuid().ToString("N"))
$archivePath = Join-Path $temporaryRoot $assetName
$extractPath = Join-Path $temporaryRoot "extract"

try {
    New-Item -ItemType Directory -Path $temporaryRoot, $extractPath -Force | Out-Null
    Write-Host "Downloading pinned LGPL FFmpeg asset..."
    Invoke-WebRequest -Uri $assetUrl -OutFile $archivePath

    $actualSha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha256 -ne $expectedSha256) {
        throw "FFmpeg SHA-256 mismatch. Expected $expectedSha256, got $actualSha256. The rolling upstream asset may have changed; review and repin it before release."
    }

    Expand-Archive -LiteralPath $archivePath -DestinationPath $extractPath -Force
    $ffmpegFile = Get-ChildItem -LiteralPath $extractPath -Recurse -Filter "ffmpeg.exe" -File | Select-Object -First 1
    $ffprobeFile = Get-ChildItem -LiteralPath $extractPath -Recurse -Filter "ffprobe.exe" -File | Select-Object -First 1
    if (-not $ffmpegFile -or -not $ffprobeFile) {
        throw "The verified archive does not contain ffmpeg.exe and ffprobe.exe."
    }

    $binDestination = Join-Path $resolvedDestination "bin"
    New-Item -ItemType Directory -Path $binDestination -Force | Out-Null
    Copy-Item -LiteralPath $ffmpegFile.FullName -Destination (Join-Path $binDestination "ffmpeg.exe") -Force
    Copy-Item -LiteralPath $ffprobeFile.FullName -Destination (Join-Path $binDestination "ffprobe.exe") -Force

    $licenseFile = Get-ChildItem -LiteralPath $extractPath -Recurse -File |
        Where-Object { $_.Name -in @("LICENSE.txt", "COPYING.LGPLv2.1", "COPYING.LGPLv3") } |
        Select-Object -First 1
    if ($licenseFile) {
        Copy-Item -LiteralPath $licenseFile.FullName -Destination (Join-Path $resolvedDestination $licenseFile.Name) -Force
    }

    & (Join-Path $binDestination "ffmpeg.exe") -hide_banner -version
    if ($LASTEXITCODE -ne 0) {
        throw "Verified FFmpeg executable did not start successfully."
    }
    Write-Host "FFmpeg installed at $binDestination"
} finally {
    $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
    $systemTemporaryRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
    if ($resolvedTemporaryRoot.StartsWith($systemTemporaryRoot, [System.StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedTemporaryRoot)) {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
