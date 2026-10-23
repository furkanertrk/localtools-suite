[CmdletBinding()]
param(
    [string]$QtRoot = 'C:\Qt-OSS',
    [string]$QtVersion = '6.11.1',
    [string]$BuildDirectory = 'build-release-store',
    [string]$OutputDirectory = 'artifacts\store\screenshots'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
$allowedOutputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts\store'))
$outputPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
if (-not $outputPath.StartsWith($allowedOutputRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw "OutputDirectory must resolve below $allowedOutputRoot"
}

$cmake = Join-Path $QtRoot 'Tools\CMake_64\bin\cmake.exe'
$qtBin = Join-Path $QtRoot "$QtVersion\mingw_64\bin"
$mingwBin = Join-Path $QtRoot 'Tools\mingw1310_64\bin'
$ninjaBin = Join-Path $QtRoot 'Tools\Ninja'
$captureExe = Join-Path $buildPath 'store_screenshot_tool.exe'
foreach ($required in @($cmake)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required screenshot prerequisite is missing: $required"
    }
}

$env:PATH = "$qtBin;$mingwBin;$ninjaBin;$env:PATH"
& $cmake --build $buildPath --target store_screenshot_tool --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Store screenshot tool build failed.' }

if (Test-Path -LiteralPath $outputPath) {
    Remove-Item -LiteralPath $outputPath -Recurse -Force
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

$previousScale = $env:QT_SCALE_FACTOR
try {
    $env:QT_SCALE_FACTOR = '1'
    foreach ($language in @('en', 'tr')) {
        $locale = if ($language -eq 'tr') { 'tr-TR' } else { 'en-US' }
        $localePath = Join-Path $outputPath $locale
        New-Item -ItemType Directory -Path $localePath -Force | Out-Null
        & $captureExe $localePath $language
        if ($LASTEXITCODE -ne 0) {
            throw "Store screenshot capture failed for $locale with exit code $LASTEXITCODE."
        }
    }
} finally {
    $env:QT_SCALE_FACTOR = $previousScale
}

Add-Type -AssemblyName System.Drawing
$captures = @(Get-ChildItem -LiteralPath $outputPath -Recurse -Filter '*.png' -File)
if ($captures.Count -ne 8) { throw "Expected 8 screenshots, found $($captures.Count)." }
foreach ($capture in $captures) {
    $bitmap = [Drawing.Bitmap]::FromFile($capture.FullName)
    try {
        if ($bitmap.Width -lt 1500 -or $bitmap.Height -lt 850) {
            throw "Screenshot is too small: $($capture.FullName) ($($bitmap.Width)x$($bitmap.Height))"
        }
    } finally {
        $bitmap.Dispose()
    }
}

Write-Host "Store screenshots generated: $outputPath"
Write-Host "Files: $($captures.Count)"
