[CmdletBinding()]
param(
    [string]$QtRoot = 'C:\Qt-OSS',
    [string]$QtVersion = '6.11.1',
    [string]$BuildDirectory = 'build-release-store',
    [string]$StageDirectory = 'artifacts\staging\LocalToolsSuite',
    [string]$EvidenceDirectory = 'artifacts\release-evidence\4.0.0',
    [switch]$SkipTests
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Invoke-Checked {
    param([string]$FilePath, [string[]]$Arguments)
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code $LASTEXITCODE`: $FilePath $($Arguments -join ' ')"
    }
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
$allowedStageRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts\staging'))
$stagePath = [IO.Path]::GetFullPath((Join-Path $repoRoot $StageDirectory))
$allowedEvidenceRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts\release-evidence'))
$evidencePath = [IO.Path]::GetFullPath((Join-Path $repoRoot $EvidenceDirectory))
$allowedPrefix = $allowedStageRoot.TrimEnd('\') + '\'

if (-not $stagePath.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "StageDirectory must resolve below $allowedStageRoot"
}
if ($stagePath -eq $allowedStageRoot) {
    throw 'Refusing to use the staging root itself as the package directory.'
}
$allowedEvidencePrefix = $allowedEvidenceRoot.TrimEnd('\') + '\'
if (-not $evidencePath.StartsWith($allowedEvidencePrefix, [StringComparison]::OrdinalIgnoreCase) -or
    $evidencePath -eq $allowedEvidenceRoot) {
    throw "EvidenceDirectory must resolve to a version directory below $allowedEvidenceRoot"
}

$qtPrefix = Join-Path $QtRoot "$QtVersion\mingw_64"
$toolRoot = Join-Path $QtRoot 'Tools'
$cmake = Join-Path $toolRoot 'CMake_64\bin\cmake.exe'
$ninja = Join-Path $toolRoot 'Ninja\ninja.exe'
$mingw = Join-Path $toolRoot 'mingw1310_64\bin'
$objdump = Join-Path $mingw 'objdump.exe'
$windeployqt = Join-Path $qtPrefix 'bin\windeployqt.exe'
$qtLicense = Join-Path $QtRoot 'Licenses\LICENSE'
$mingwLicenses = Join-Path $toolRoot 'mingw1310_64\licenses'
$qtDocs = Join-Path $QtRoot "Docs\Qt-$QtVersion"
$patchedQtSvgRoot = Join-Path $repoRoot 'artifacts\dependency-build\qtsvg-6.11.1-cve-2026-8168'
$patchedQtSvgInstall = Join-Path $patchedQtSvgRoot 'install'
$pdfiumRoot = Join-Path $repoRoot 'third_party\pdfium'
$pdfiumDll = Join-Path $pdfiumRoot 'bin\pdfium.dll'
$pdfiumVersionFile = Join-Path $pdfiumRoot 'VERSION'
$expectedPdfiumDllSha256 = 'fb898a1f5ace57805834f390407500bdb6ef93eff326a252ad334a8aae809d8e'

$required = @($cmake, $ninja, $windeployqt, $qtLicense,
    (Join-Path $mingw 'gcc.exe'), (Join-Path $mingw 'g++.exe'),
    (Join-Path $mingw 'windres.exe'), $objdump,
    $pdfiumDll, $pdfiumVersionFile,
    (Join-Path $repoRoot 'third_party\ffmpeg\bin\ffmpeg.exe'),
    (Join-Path $repoRoot 'third_party\ffmpeg\bin\ffprobe.exe'),
    (Join-Path $patchedQtSvgInstall 'bin\Qt6Svg.dll'),
    (Join-Path $patchedQtSvgInstall 'plugins\iconengines\qsvgicon.dll'),
    (Join-Path $patchedQtSvgInstall 'plugins\imageformats\qsvg.dll'),
    (Join-Path $patchedQtSvgRoot 'provenance.txt'))
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missing.Count -gt 0) {
    throw "Release prerequisites are missing:`n$($missing -join "`n")"
}

$pdfiumVersion = (Get-Content -LiteralPath $pdfiumVersionFile -Raw)
if ($pdfiumVersion -notmatch '(?m)^MAJOR=153\s*$' -or
    $pdfiumVersion -notmatch '(?m)^BUILD=7999\s*$') {
    throw 'PDFium version mismatch. Store release requires the reviewed 153.0.7999.0 artifact.'
}
$actualPdfiumDllSha256 = (Get-FileHash -LiteralPath $pdfiumDll -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualPdfiumDllSha256 -ne $expectedPdfiumDllSha256) {
    throw "PDFium DLL SHA-256 mismatch. Expected $expectedPdfiumDllSha256, found $actualPdfiumDllSha256."
}

# The Store/privacy claim is offline processing. Fail closed if production
# source starts using a network client or embeds a developer-machine path.
$productionSourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src') -Recurse -File |
    Where-Object { $_.Extension -in @('.cpp', '.h') })
$networkApiPattern = '\b(QNetworkAccessManager|QNetworkRequest|QTcpSocket|QUdpSocket|QWebSocket|WinHttpOpen|InternetOpen[AW]?)\b'
$networkApiMatches = @($productionSourceFiles | Select-String -Pattern $networkApiPattern)
if ($networkApiMatches.Count -gt 0) {
    throw "Offline release policy violation: network API found in production source:`n$($networkApiMatches -join "`n")"
}
$developerPathMatches = @($productionSourceFiles |
    Select-String -Pattern '(?i)(C:[\\/]Users[\\/]|D:[\\/]DATA[\\/])')
if ($developerPathMatches.Count -gt 0) {
    throw "Developer-machine absolute path found in production source:`n$($developerPathMatches -join "`n")"
}

$qmake = Join-Path $qtPrefix 'bin\qmake.exe'
$actualQtVersion = (& $qmake -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $actualQtVersion -ne $QtVersion) {
    throw "Qt SDK version mismatch. Expected $QtVersion, found $actualQtVersion."
}

Write-Host "Configuring LocalTools Suite 4.0.0 Release with Qt $QtVersion OSS..."
Invoke-Checked $cmake @(
    '-S', $repoRoot,
    '-B', $buildPath,
    '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_PREFIX_PATH=$($qtPrefix.Replace('\', '/'))",
    "-DCMAKE_C_COMPILER=$((Join-Path $mingw 'gcc.exe').Replace('\', '/'))",
    "-DCMAKE_CXX_COMPILER=$((Join-Path $mingw 'g++.exe').Replace('\', '/'))",
    "-DCMAKE_RC_COMPILER=$((Join-Path $mingw 'windres.exe').Replace('\', '/'))",
    "-DCMAKE_MAKE_PROGRAM=$($ninja.Replace('\', '/'))"
)
Invoke-Checked $cmake @('--build', $buildPath, '--parallel', '4')

if (-not $SkipTests) {
    $ctest = Join-Path $toolRoot 'CMake_64\bin\ctest.exe'
    Invoke-Checked $ctest @('--test-dir', $buildPath, '--output-on-failure', '-C', 'Release')
}

if (Test-Path -LiteralPath $stagePath) {
    # The path was canonicalized and constrained above before recursive deletion.
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}
New-Item -ItemType Directory -Path $stagePath -Force | Out-Null
if (Test-Path -LiteralPath $evidencePath) {
    Remove-Item -LiteralPath $evidencePath -Recurse -Force
}
New-Item -ItemType Directory -Path $evidencePath -Force | Out-Null
@(
    "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    'Result: PASS',
    'Production source contains no prohibited network-client API.',
    'Production source contains no C:/Users or D:/DATA developer-machine path.',
    'Explicit user-invoked QDesktopServices URL opening is not an in-process network client.'
) | Set-Content -LiteralPath (Join-Path $evidencePath 'offline-source-audit.txt') -Encoding utf8
if (-not $SkipTests) {
    $performanceResult = Join-Path $buildPath 'performance-results.json'
    if (-not (Test-Path -LiteralPath $performanceResult -PathType Leaf)) {
        throw 'Release performance evidence was not generated by CTest.'
    }
    Copy-Item -LiteralPath $performanceResult -Destination (Join-Path $evidencePath 'performance-results.json') -Force
}

Invoke-Checked $cmake @('--install', $buildPath, '--prefix', $stagePath, '--config', 'Release')
Invoke-Checked $windeployqt @(
    '--release',
    '--compiler-runtime',
    '--no-translations',
    '--no-system-d3d-compiler',
    '--no-opengl-sw',
    '--exclude-plugins', 'ffmpegmediaplugin',
    '--dir', $stagePath,
    (Join-Path $stagePath 'LocalTools.exe')
)

# Qt 6.11.1 predates the official CVE-2026-8168 QtSvg patches. Replace every
# deployed QtSvg runtime entry point with the verified, locally rebuilt module.
Copy-Item -LiteralPath (Join-Path $patchedQtSvgInstall 'bin\Qt6Svg.dll') -Destination (Join-Path $stagePath 'Qt6Svg.dll') -Force
Copy-Item -LiteralPath (Join-Path $patchedQtSvgInstall 'plugins\iconengines\qsvgicon.dll') -Destination (Join-Path $stagePath 'iconengines\qsvgicon.dll') -Force
Copy-Item -LiteralPath (Join-Path $patchedQtSvgInstall 'plugins\imageformats\qsvg.dll') -Destination (Join-Path $stagePath 'imageformats\qsvg.dll') -Force
$qtSvgRuntimePairs = @(
    @((Join-Path $patchedQtSvgInstall 'bin\Qt6Svg.dll'), (Join-Path $stagePath 'Qt6Svg.dll')),
    @((Join-Path $patchedQtSvgInstall 'plugins\iconengines\qsvgicon.dll'), (Join-Path $stagePath 'iconengines\qsvgicon.dll')),
    @((Join-Path $patchedQtSvgInstall 'plugins\imageformats\qsvg.dll'), (Join-Path $stagePath 'imageformats\qsvg.dll'))
)
foreach ($pair in $qtSvgRuntimePairs) {
    $sourceHash = (Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash
    $stagedHash = (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash
    if ($sourceHash -ne $stagedHash) {
        throw "The staged QtSvg runtime does not match the verified patched build: $($pair[1])"
    }
}
Copy-Item -LiteralPath (Join-Path $patchedQtSvgRoot 'provenance.txt') -Destination (Join-Path $evidencePath 'qtsvg-security-provenance.txt') -Force

$licenseDir = Join-Path $stagePath 'licenses'
New-Item -ItemType Directory -Path $licenseDir -Force | Out-Null
Copy-Item -LiteralPath $qtLicense -Destination (Join-Path $licenseDir 'Qt-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $patchedQtSvgRoot 'provenance.txt') -Destination (Join-Path $licenseDir 'QtSvg-security-provenance.txt') -Force
$runtimeLicenseFiles = @{
    (Join-Path $mingwLicenses 'gcc\COPYING3') = 'GCC-GPL-3.0.txt'
    (Join-Path $mingwLicenses 'gcc\COPYING.RUNTIME') = 'GCC-RUNTIME-LIBRARY-EXCEPTION.txt'
    (Join-Path $mingwLicenses 'mingw-w64\COPYING') = 'MinGW-w64-COPYING.txt'
}
foreach ($entry in $runtimeLicenseFiles.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Key -PathType Leaf)) {
        throw "Required compiler runtime notice is missing: $($entry.Key)"
    }
    Copy-Item -LiteralPath $entry.Key -Destination (Join-Path $licenseDir $entry.Value) -Force
}

$qtAttributionDir = Join-Path $licenseDir 'Qt-third-party-attributions'
New-Item -ItemType Directory -Path $qtAttributionDir -Force | Out-Null
$qtAttributions = @(
    'qtdoc\licenses-used-in-qt.html'
)
foreach ($relativeAttribution in $qtAttributions) {
    $sourceAttribution = Join-Path $qtDocs $relativeAttribution
    if (-not (Test-Path -LiteralPath $sourceAttribution -PathType Leaf)) {
        throw "Required Qt attribution is missing: $sourceAttribution"
    }
    Copy-Item -LiteralPath $sourceAttribution -Destination $qtAttributionDir -Force
}

$requiredStageFiles = @(
    'LocalTools.exe', 'pdfium.dll', 'ffmpeg.exe', 'ffprobe.exe',
    'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Svg.dll', 'platforms\qwindows.dll',
    'iconengines\qsvgicon.dll', 'imageformats\qsvg.dll',
    'multimedia\windowsmediaplugin.dll',
    'licenses\THIRD_PARTY_NOTICES.txt', 'licenses\Qt-LICENSE.txt',
    'licenses\QtSvg-security-provenance.txt',
    'licenses\FFmpeg-LGPL-3.0.txt', 'licenses\PDFium-LICENSE.txt',
    'licenses\PRIVACY_POLICY_TR.md', 'licenses\PRIVACY_POLICY_EN.md'
)
$missingStage = @($requiredStageFiles | Where-Object {
    -not (Test-Path -LiteralPath (Join-Path $stagePath $_) -PathType Leaf)
})
if ($missingStage.Count -gt 0) {
    throw "Staging is missing required production files:`n$($missingStage -join "`n")"
}

$forbidden = @(Get-ChildItem -LiteralPath $stagePath -Recurse -File | Where-Object {
    $relative = $_.FullName.Substring($stagePath.Length).TrimStart('\')
    -not $relative.StartsWith('licenses\', [StringComparison]::OrdinalIgnoreCase) -and (
        $_.Extension -in @('.pdb', '.ilk', '.exp', '.lib', '.a', '.obj', '.o', '.log', '.ini') -or
        $_.Name -eq 'test.pdf' -or $_.Name -match '(^|_)(smoke|fixture|test)(_|\.|$)'
    )
})
if ($forbidden.Count -gt 0) {
    throw "Forbidden development artifacts entered staging:`n$($forbidden.FullName -join "`n")"
}

$forbiddenMediaRuntime = @(
    'multimedia\ffmpegmediaplugin.dll',
    'avcodec-61.dll', 'avformat-61.dll', 'avutil-59.dll',
    'swresample-5.dll', 'swscale-8.dll'
)
$presentForbiddenMedia = @($forbiddenMediaRuntime | Where-Object {
    Test-Path -LiteralPath (Join-Path $stagePath $_) -PathType Leaf
})
if ($presentForbiddenMedia.Count -gt 0) {
    throw "Qt's older FFmpeg playback backend entered Store staging:`n$($presentForbiddenMedia -join "`n")"
}

$ffmpeg = Join-Path $stagePath 'ffmpeg.exe'
$ffmpegVersionFile = Join-Path $evidencePath 'ffmpeg-version.txt'
$ffmpegCodecsFile = Join-Path $evidencePath 'ffmpeg-codecs.txt'
& $ffmpeg -hide_banner -version 2>&1 | Set-Content -LiteralPath $ffmpegVersionFile -Encoding utf8
if ($LASTEXITCODE -ne 0) { throw 'Staged FFmpeg version audit failed.' }
& $ffmpeg -hide_banner -codecs 2>&1 | Set-Content -LiteralPath $ffmpegCodecsFile -Encoding utf8
if ($LASTEXITCODE -ne 0) { throw 'Staged FFmpeg codec audit failed.' }
$ffmpegConfig = Get-Content -LiteralPath $ffmpegVersionFile -Raw
if ($ffmpegConfig -match '(?<!disable-)--enable-(gpl|nonfree)' -or
    $ffmpegConfig -match '--enable-libx26[45]') {
    throw 'The staged FFmpeg configuration violates the LGPL-only release policy.'
}

$inventoryPath = Join-Path $evidencePath 'dependency-inventory.txt'
$inventoryLines = @(
    'LocalTools Suite 4.0.0 production dependency inventory',
    "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    'Toolchain: Qt 6.11.1 Community / MinGW 13.1.0 / CMake / Ninja',
    'QtSvg: 6.11.1 rebuilt with the five official CVE-2026-8168 Qt 6.11 patches',
    'PDFium: 153.0.7999.0',
    'FFmpeg: approved BtbN 8.1 LGPL family',
    '',
    'Executable and DLL files:'
)
$binaryFiles = Get-ChildItem -LiteralPath $stagePath -Recurse -File |
    Where-Object { $_.Extension -in @('.exe', '.dll') } |
    Sort-Object { $_.FullName.Substring($stagePath.Length) }
foreach ($file in $binaryFiles) {
    $relative = $file.FullName.Substring($stagePath.Length).TrimStart('\').Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $inventoryLines += "$hash  $relative  ($($file.Length) bytes)"
}
$inventoryLines | Set-Content -LiteralPath $inventoryPath -Encoding utf8

$manifestPath = Join-Path $evidencePath 'staging-manifest-sha256.txt'
$manifestLines = @()
$manifestFiles = Get-ChildItem -LiteralPath $stagePath -Recurse -File |
    Sort-Object { $_.FullName.Substring($stagePath.Length) }
foreach ($file in $manifestFiles) {
    $relative = $file.FullName.Substring($stagePath.Length).TrimStart('\').Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $manifestLines += "$hash  $relative"
}
$manifestLines | Set-Content -LiteralPath $manifestPath -Encoding ascii

$peAuditPath = Join-Path $evidencePath 'pe-security-flags.txt'
$peAuditLines = @(
    'LocalTools Suite 4.0.0 PE security flag audit',
    "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    'Required for every staged executable and DLL: DYNAMIC_BASE, NX_COMPAT',
    ''
)
foreach ($file in $binaryFiles) {
    $relative = $file.FullName.Substring($stagePath.Length).TrimStart('\').Replace('\', '/')
    $headers = (& $objdump -p $file.FullName 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "PE header inspection failed for $relative"
    }
    $hasDynamicBase = $headers -match '(?m)^\s*DYNAMIC_BASE\s*$'
    $hasNxCompat = $headers -match '(?m)^\s*NX_COMPAT\s*$'
    $hasHighEntropy = $headers -match '(?m)^\s*HIGH_ENTROPY_VA\s*$'
    $peAuditLines += "$relative | DYNAMIC_BASE=$hasDynamicBase | NX_COMPAT=$hasNxCompat | HIGH_ENTROPY_VA=$hasHighEntropy"
    if (-not $hasDynamicBase -or -not $hasNxCompat) {
        throw "Required PE security flags are missing from $relative"
    }
}
$peAuditLines | Set-Content -LiteralPath $peAuditPath -Encoding utf8

# Verify that the production staging is self-contained. Limit PATH during the
# probe so an installed Qt SDK cannot accidentally satisfy a missing runtime.
$runtimeProbePath = Join-Path $evidencePath 'staging-runtime-probe.txt'
$startInfo = [Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = Join-Path $stagePath 'LocalTools.exe'
$startInfo.WorkingDirectory = $stagePath
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.Environment['PATH'] = "$stagePath;$env:SystemRoot\System32;$env:SystemRoot"
$runtimeProcess = [Diagnostics.Process]::Start($startInfo)
if ($null -eq $runtimeProcess) { throw 'The staged application process could not be created.' }
$survivedProbe = -not $runtimeProcess.WaitForExit(3000)
if ($survivedProbe) {
    $runtimeProcess.Kill($true)
    $runtimeProcess.WaitForExit(5000) | Out-Null
}
if (-not $survivedProbe) {
    throw "The staged application exited during the runtime probe (exit code $($runtimeProcess.ExitCode))."
}
@(
    "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    'Result: PASS',
    'Method: LocalTools.exe remained running for 3 seconds with PATH restricted to staging and Windows system directories.',
    'The process was then terminated by the release audit.'
) | Set-Content -LiteralPath $runtimeProbePath -Encoding utf8

Write-Host "Release staging complete: $stagePath"
Write-Host "Files: $((Get-ChildItem -LiteralPath $stagePath -Recurse -File).Count)"
Write-Host "Release evidence: $evidencePath"
