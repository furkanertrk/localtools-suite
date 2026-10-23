[CmdletBinding()]
param(
    [string]$QtRoot = 'C:\Qt-OSS',
    [string]$QtVersion = '6.11.1',
    [string]$OutputDirectory = 'artifacts\dependency-build\qtsvg-6.11.1-cve-2026-8168',
    [switch]$ForceRebuild
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

function Assert-Sha256 {
    param([string]$Path, [string]$Expected)
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Expected.ToLowerInvariant()) {
        throw "SHA-256 mismatch for $Path. Expected $Expected, found $actual."
    }
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts\dependency-build'))
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$allowedPrefix = $allowedRoot.TrimEnd('\') + '\'
if (-not $outputRoot.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase) -or
    $outputRoot -eq $allowedRoot) {
    throw "OutputDirectory must resolve to a child of $allowedRoot"
}

$qtPrefix = Join-Path $QtRoot "$QtVersion\mingw_64"
$toolRoot = Join-Path $QtRoot 'Tools'
$cmake = Join-Path $toolRoot 'CMake_64\bin\cmake.exe'
$ninja = Join-Path $toolRoot 'Ninja\ninja.exe'
$mingw = Join-Path $toolRoot 'mingw1310_64\bin'
$qmake = Join-Path $qtPrefix 'bin\qmake.exe'
$git = (Get-Command git.exe -ErrorAction Stop).Source

$requiredTools = @(
    $cmake, $ninja, $qmake,
    (Join-Path $mingw 'gcc.exe'),
    (Join-Path $mingw 'g++.exe'),
    (Join-Path $mingw 'windres.exe')
)
$missingTools = @($requiredTools | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missingTools.Count -gt 0) {
    throw "QtSvg build prerequisites are missing:`n$($missingTools -join "`n")"
}

$actualQtVersion = (& $qmake -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $actualQtVersion -ne $QtVersion) {
    throw "Qt SDK version mismatch. Expected $QtVersion, found $actualQtVersion."
}

$sourceName = "qtsvg-everywhere-src-$QtVersion"
$sourceArchiveName = "$sourceName.zip"
$sourceUrl = "https://download.qt.io/official_releases/qt/6.11/$QtVersion/submodules/$sourceArchiveName"
$sourceSha256 = '767730188d4610a89bf8da502f87acf1c8881a3ac54f1e0eb167ab1e08b03a75'
$patches = @(
    @{ Name = 'CVE-2026-8168-qtsvg-6.11-0000.diff'; Sha256 = '81b533bbbe44eb964a47502d988bc578cbaf0986374b5b63e8b86eadf097ff89' },
    @{ Name = 'CVE-2026-8168-qtsvg-6.11-0001.diff'; Sha256 = '8c8ba1613567ec09f1486fc130a3b9cf8ffdd99f0386b30ce40ce38e403e4942' },
    @{ Name = 'CVE-2026-8168-qtsvg-6.11-0002.diff'; Sha256 = 'a281a92f8dd2c4c72e75fd4251a07649b9078be75aa5ffd114611faf6ea4e6aa' },
    @{ Name = 'CVE-2026-8168-qtsvg-6.11-0003.diff'; Sha256 = 'c584ec7b8d9677a8b91cd2d2abe6de345de09707864fe2d84a0f0ec469763b47' },
    @{ Name = 'CVE-2026-8168-qtsvg-6.11-0004.diff'; Sha256 = 'c81cf94ff178eabc1dee92d14ef703f85e12f8e4709d3b95c8a090bf0a6a3ed1' }
)

$installRoot = Join-Path $outputRoot 'install'
$requiredOutputs = @(
    (Join-Path $installRoot 'bin\Qt6Svg.dll'),
    (Join-Path $installRoot 'plugins\iconengines\qsvgicon.dll'),
    (Join-Path $installRoot 'plugins\imageformats\qsvg.dll'),
    (Join-Path $outputRoot 'provenance.txt')
)
if (-not $ForceRebuild -and ($requiredOutputs | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) }).Count -eq 0) {
    Write-Host "Patched QtSvg is already available at $installRoot"
    return
}

if (Test-Path -LiteralPath $outputRoot) {
    Remove-Item -LiteralPath $outputRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$downloadRoot = Join-Path $outputRoot 'downloads'
$sourceRoot = Join-Path $outputRoot 'source'
$buildRoot = Join-Path $outputRoot 'build'
$patchRoot = Join-Path $outputRoot 'patches'
New-Item -ItemType Directory -Path $downloadRoot, $sourceRoot, $patchRoot -Force | Out-Null

$sourceArchive = Join-Path $downloadRoot $sourceArchiveName
Write-Host "Downloading official QtSvg $QtVersion source..."
Invoke-WebRequest -UseBasicParsing -Uri $sourceUrl -OutFile $sourceArchive
Assert-Sha256 $sourceArchive $sourceSha256

foreach ($patch in $patches) {
    $patchPath = Join-Path $patchRoot $patch.Name
    $patchUrl = "https://download.qt.io/official_releases/qt/6.11/$($patch.Name)"
    Write-Host "Downloading $($patch.Name)..."
    Invoke-WebRequest -UseBasicParsing -Uri $patchUrl -OutFile $patchPath
    Assert-Sha256 $patchPath $patch.Sha256
}

Expand-Archive -LiteralPath $sourceArchive -DestinationPath $sourceRoot
$expandedSource = Join-Path $sourceRoot $sourceName
if (-not (Test-Path -LiteralPath (Join-Path $expandedSource 'CMakeLists.txt') -PathType Leaf)) {
    throw 'The verified QtSvg source archive did not contain the expected source tree.'
}

$relativeExpandedSource = [IO.Path]::GetRelativePath($repoRoot, $expandedSource).Replace('\', '/')
Push-Location $repoRoot
try {
    foreach ($patch in $patches) {
        $patchPath = Join-Path $patchRoot $patch.Name
        Invoke-Checked $git @('apply', '--check', "--directory=$relativeExpandedSource", $patchPath)
        Invoke-Checked $git @('apply', "--directory=$relativeExpandedSource", $patchPath)
    }
} finally {
    Pop-Location
}

$configureArguments = @(
    '-S', $expandedSource,
    '-B', $buildRoot,
    '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_PREFIX_PATH=$($qtPrefix.Replace('\', '/'))",
    "-DCMAKE_INSTALL_PREFIX=$($installRoot.Replace('\', '/'))",
    "-DCMAKE_C_COMPILER=$((Join-Path $mingw 'gcc.exe').Replace('\', '/'))",
    "-DCMAKE_CXX_COMPILER=$((Join-Path $mingw 'g++.exe').Replace('\', '/'))",
    "-DCMAKE_RC_COMPILER=$((Join-Path $mingw 'windres.exe').Replace('\', '/'))",
    "-DCMAKE_MAKE_PROGRAM=$($ninja.Replace('\', '/'))",
    '-DQT_BUILD_TESTS=OFF',
    '-DQT_BUILD_EXAMPLES=OFF'
)

Write-Host 'Configuring and building the security-patched QtSvg module...'
Invoke-Checked $cmake $configureArguments
Invoke-Checked $cmake @('--build', $buildRoot, '--parallel', '4')
Invoke-Checked $cmake @('--install', $buildRoot, '--config', 'Release', '--strip')

$missingOutputs = @($requiredOutputs[0..2] | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missingOutputs.Count -gt 0) {
    throw "The patched QtSvg build is incomplete:`n$($missingOutputs -join "`n")"
}

$provenance = @(
    "QtSvg $QtVersion security-patched build for LocalTools Suite",
    "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "Source: $sourceUrl",
    "Source SHA-256: $sourceSha256",
    'Applied official Qt patches:'
)
foreach ($patch in $patches) {
    $provenance += "  $($patch.Name)  SHA-256 $($patch.Sha256)"
}
$provenance += 'Release binaries:'
foreach ($path in $requiredOutputs[0..2]) {
    $relative = [IO.Path]::GetRelativePath($installRoot, $path).Replace('\', '/')
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    $provenance += "  $relative  SHA-256 $hash"
}
$provenance | Set-Content -LiteralPath (Join-Path $outputRoot 'provenance.txt') -Encoding utf8

Write-Host "Patched QtSvg build completed: $installRoot"
