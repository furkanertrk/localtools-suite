[CmdletBinding()]
param(
    [string]$WindowsSdkVersion = '10.0.19041.0',
    [switch]$SkipBuild
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

function Assert-ChildPath {
    param([string]$Path, [string]$Parent, [string]$Label)
    $fullPath = [IO.Path]::GetFullPath($Path)
    $fullParent = [IO.Path]::GetFullPath($Parent)
    if (-not $fullPath.StartsWith($fullParent.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $fullPath -eq $fullParent) {
        throw "$Label must resolve below $fullParent"
    }
    return $fullPath
}

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$artifactRoot = Join-Path $repoRoot 'artifacts'
$stagePath = Join-Path $artifactRoot 'staging\LocalToolsSuite'
$packageRoot = Assert-ChildPath (Join-Path $artifactRoot 'msix-root\LocalToolsSuite') (Join-Path $artifactRoot 'msix-root') 'Package root'
$unpackPath = Assert-ChildPath (Join-Path $artifactRoot 'msix-unpacked\LocalToolsSuite') (Join-Path $artifactRoot 'msix-unpacked') 'Unpack path'
$outputDir = Join-Path $artifactRoot 'packages'
$evidencePath = Join-Path $artifactRoot 'release-evidence\4.0.0'
$packagePath = Join-Path $outputDir 'LocalToolsSuite_4.0.0.0_x64.msix'
$makeAppx = "C:\Program Files (x86)\Windows Kits\10\bin\$WindowsSdkVersion\x64\makeappx.exe"

if (-not (Test-Path -LiteralPath $makeAppx -PathType Leaf)) {
    throw "MakeAppx was not found: $makeAppx. Install the Windows 10/11 SDK."
}
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build_release.ps1')
    if ($LASTEXITCODE -ne 0) { throw 'Release staging failed.' }
}
if (-not (Test-Path -LiteralPath (Join-Path $stagePath 'LocalTools.exe') -PathType Leaf)) {
    throw 'Production staging is missing. Run tools/build_release.ps1 first.'
}

foreach ($cleanPath in @($packageRoot, $unpackPath)) {
    if (Test-Path -LiteralPath $cleanPath) {
        Remove-Item -LiteralPath $cleanPath -Recurse -Force
    }
    New-Item -ItemType Directory -Path $cleanPath -Force | Out-Null
}
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
if (Test-Path -LiteralPath $packagePath) {
    Remove-Item -LiteralPath $packagePath -Force
}

Copy-Item -Path (Join-Path $stagePath '*') -Destination $packageRoot -Recurse -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'packaging\msix\AppxManifest.xml') -Destination $packageRoot -Force

[xml]$manifest = Get-Content -LiteralPath (Join-Path $packageRoot 'AppxManifest.xml')
$identity = $manifest.Package.Identity
if ($identity.Name -ne 'FurkanErtrk.LocalToolsSuite' -or
    $identity.Publisher -ne 'CN=A6DAE150-129C-4193-B375-5B693386F8B6' -or
    $identity.Version -ne '4.0.0.0' -or
    $identity.ProcessorArchitecture -ne 'x64') {
    throw 'AppxManifest identity does not match the frozen Partner Center identity.'
}

# Generate exact-size package artwork from the largest frame in the canonical
# ICO. ExtractAssociatedIcon() commonly returns the 32 px shell frame and made
# larger Store assets visibly blurry even though the ICO contains 128 px art.
Add-Type -AssemblyName System.Drawing
$assetsPath = Join-Path $packageRoot 'Assets'
New-Item -ItemType Directory -Path $assetsPath -Force | Out-Null
$iconPath = Join-Path $repoRoot 'resources\icons\LocalTools.ico'
$sourceIcon = [Drawing.Icon]::new($iconPath, 128, 128)
if ($null -eq $sourceIcon -or $sourceIcon.Width -lt 128 -or $sourceIcon.Height -lt 128) {
    throw 'The canonical application icon does not contain the required 128 px source frame.'
}
$sourceBitmap = $sourceIcon.ToBitmap()

function Save-Asset {
    param([string]$Name, [int]$Width, [int]$Height, [switch]$Wide)
    $bitmap = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([Drawing.Color]::Transparent)
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $requestedSide = if ($Wide) { [Math]::Min($Height * 0.72, $Width * 0.40) } else { [Math]::Min($Width, $Height) * 0.78 }
        $side = [Math]::Min($requestedSide, 128)
        $left = ($Width - $side) / 2
        $top = ($Height - $side) / 2
        $graphics.DrawImage($sourceBitmap, [Drawing.RectangleF]::new($left, $top, $side, $side))
        $bitmap.Save((Join-Path $assetsPath $Name), [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

try {
    Save-Asset 'StoreLogo.png' 50 50
    Save-Asset 'Square44x44Logo.png' 44 44
    Save-Asset 'Square150x150Logo.png' 150 150
    Save-Asset 'Wide310x150Logo.png' 310 150 -Wide
    Save-Asset 'SplashScreen.png' 620 300 -Wide
} finally {
    $sourceBitmap.Dispose()
    $sourceIcon.Dispose()
}

Invoke-Checked $makeAppx @('pack', '/d', $packageRoot, '/p', $packagePath, '/o')
Invoke-Checked $makeAppx @('unpack', '/p', $packagePath, '/d', $unpackPath, '/o')

$forbiddenPackage = @(Get-ChildItem -LiteralPath $unpackPath -Recurse -File | Where-Object {
    $relative = $_.FullName.Substring($unpackPath.Length).TrimStart('\')
    -not $relative.StartsWith('licenses\', [StringComparison]::OrdinalIgnoreCase) -and (
        $_.Extension -in @('.pdb', '.ilk', '.exp', '.lib', '.a', '.obj', '.o', '.log', '.ini') -or
        $_.Name -eq 'test.pdf' -or $_.Name -match '(^|_)(smoke|fixture|test)(_|\.|$)'
    )
})
if ($forbiddenPackage.Count -gt 0) {
    throw "Forbidden development artifacts entered MSIX:`n$($forbiddenPackage.FullName -join "`n")"
}

New-Item -ItemType Directory -Path $evidencePath -Force | Out-Null
$packageHash = (Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash.ToLowerInvariant()
@(
    "Package: $([IO.Path]::GetFileName($packagePath))",
    'Identity: FurkanErtrk.LocalToolsSuite',
    'Publisher: CN=A6DAE150-129C-4193-B375-5B693386F8B6',
    'Version: 4.0.0.0',
    'Architecture: x64',
    "Size: $((Get-Item -LiteralPath $packagePath).Length) bytes",
    "SHA-256: $packageHash",
    'Signature: unsigned Store-ingestion candidate; not installable until signed'
) | Set-Content -LiteralPath (Join-Path $evidencePath 'msix-candidate.txt') -Encoding utf8

Write-Host "MSIX candidate complete: $packagePath"
Write-Host "SHA-256: $packageHash"
