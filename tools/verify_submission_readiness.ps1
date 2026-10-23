[CmdletBinding()]
param(
    [switch]$RequireExternalGates
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$artifactRoot = Join-Path $repoRoot 'artifacts'
$evidenceRoot = Join-Path $artifactRoot 'release-evidence\4.0.0'
$packagePath = Join-Path $artifactRoot 'packages\LocalToolsSuite_4.0.0.0_x64.msix'
$unpackedRoot = Join-Path $artifactRoot 'msix-unpacked\LocalToolsSuite'
$screenshotRoot = Join-Path $artifactRoot 'store\screenshots'
$externalRoot = Join-Path $evidenceRoot 'external'
$reportPath = Join-Path $evidenceRoot 'submission-readiness.txt'

function Assert-Ready {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) { throw $Message }
}

Assert-Ready (Test-Path -LiteralPath $packagePath -PathType Leaf) 'Final MSIX candidate is missing.'
Assert-Ready (Test-Path -LiteralPath $unpackedRoot -PathType Container) 'Verified unpacked MSIX directory is missing.'
Assert-Ready (Test-Path -LiteralPath $evidenceRoot -PathType Container) 'Release evidence directory is missing.'

$package = Get-Item -LiteralPath $packagePath
$packageHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $packagePath).Hash.ToLowerInvariant()
$candidateRecordPath = Join-Path $evidenceRoot 'msix-candidate.txt'
$defenderRecordPath = Join-Path $evidenceRoot 'defender-scan.txt'
Assert-Ready (Test-Path -LiteralPath $candidateRecordPath -PathType Leaf) 'MSIX candidate record is missing.'
Assert-Ready (Test-Path -LiteralPath $defenderRecordPath -PathType Leaf) 'Defender scan record is missing.'
$candidateRecord = Get-Content -LiteralPath $candidateRecordPath -Raw
$defenderRecord = Get-Content -LiteralPath $defenderRecordPath -Raw
Assert-Ready ($candidateRecord -match [regex]::Escape($packageHash)) 'Candidate record does not match the current MSIX hash.'
Assert-Ready ($defenderRecord -match [regex]::Escape($packageHash)) 'Defender record does not match the current MSIX hash.'
Assert-Ready ($defenderRecord -match 'found no threats') 'Defender record does not contain a clean result.'

[xml]$manifest = Get-Content -LiteralPath (Join-Path $unpackedRoot 'AppxManifest.xml')
$identity = $manifest.Package.Identity
Assert-Ready ($identity.Name -eq 'FurkanErtrk.LocalToolsSuite') 'MSIX Identity Name mismatch.'
Assert-Ready ($identity.Publisher -eq 'CN=A6DAE150-129C-4193-B375-5B693386F8B6') 'MSIX Publisher mismatch.'
Assert-Ready ($identity.Version -eq '4.0.0.0') 'MSIX version mismatch.'
Assert-Ready ($identity.ProcessorArchitecture -eq 'x64') 'MSIX architecture mismatch.'

$translationPath = Join-Path $repoRoot 'resources\translations\tr_TR.ts'
Assert-Ready (@(Select-String -Path $translationPath -SimpleMatch 'type="unfinished"').Count -eq 0) 'Unfinished translations remain.'
Assert-Ready (@(Select-String -Path $translationPath -SimpleMatch 'type="obsolete"').Count -eq 0) 'Obsolete translations remain.'

Add-Type -AssemblyName System.Drawing
$expectedScreenshots = @(
    'en-US\01-home.png', 'en-US\02-pdf-organize.png', 'en-US\03-image-compress.png', 'en-US\04-metadata.png',
    'tr-TR\01-home.png', 'tr-TR\02-pdf-organize.png', 'tr-TR\03-image-compress.png', 'tr-TR\04-metadata.png'
)
foreach ($relative in $expectedScreenshots) {
    $path = Join-Path $screenshotRoot $relative
    Assert-Ready (Test-Path -LiteralPath $path -PathType Leaf) "Store screenshot is missing: $relative"
    $bitmap = [Drawing.Bitmap]::FromFile($path)
    try {
        Assert-Ready ($bitmap.Width -eq 1600 -and $bitmap.Height -eq 900) "Store screenshot dimensions are invalid: $relative"
    } finally {
        $bitmap.Dispose()
    }
}

$expectedArtwork = @{
    'StoreLogo.png' = @(50, 50)
    'Square44x44Logo.png' = @(44, 44)
    'Square150x150Logo.png' = @(150, 150)
    'Wide310x150Logo.png' = @(310, 150)
    'SplashScreen.png' = @(620, 300)
}
foreach ($entry in $expectedArtwork.GetEnumerator()) {
    $path = Join-Path $unpackedRoot (Join-Path 'Assets' $entry.Key)
    Assert-Ready (Test-Path -LiteralPath $path -PathType Leaf) "Package artwork is missing: $($entry.Key)"
    $bitmap = [Drawing.Bitmap]::FromFile($path)
    try {
        Assert-Ready ($bitmap.Width -eq $entry.Value[0] -and $bitmap.Height -eq $entry.Value[1]) "Package artwork dimensions are invalid: $($entry.Key)"
    } finally {
        $bitmap.Dispose()
    }
}

$requiredEngineeringEvidence = @(
    'dependency-inventory.txt', 'ffmpeg-version.txt', 'ffmpeg-codecs.txt',
    'offline-source-audit.txt', 'pe-security-flags.txt', 'performance-results.json',
    'qtsvg-security-provenance.txt', 'staging-manifest-sha256.txt',
    'staging-runtime-probe.txt', 'store-screenshot-manifest-sha256.txt',
    'package-artwork-manifest-sha256.txt'
)
foreach ($file in $requiredEngineeringEvidence) {
    Assert-Ready (Test-Path -LiteralPath (Join-Path $evidenceRoot $file) -PathType Leaf) "Engineering evidence is missing: $file"
}

$externalGates = [ordered]@{
    'local-msix-lifecycle.txt' = 'Elevated local install/upgrade/uninstall/reinstall test'
    'external\windows10-lifecycle.txt' = 'Clean Windows 10 1809 lifecycle test'
    'external\windows11-lifecycle.txt' = 'Clean current Windows 11 lifecycle test'
    'external\accessibility-manual.txt' = 'Manual keyboard and screen-reader test'
    'external\privacy-url.txt' = 'Published stable HTTPS privacy-policy URL'
    'external\support-contact.txt' = 'Public monitored support contact'
    'external\legal-review.txt' = 'Publisher/legal LGPL and codec-market review'
    'external\partner-center-certification.txt' = 'Partner Center certification result for this exact hash'
    'external\flight-results.txt' = 'Package flight on at least two clean environments'
    'external\store-submission.txt' = 'IARC, listing fields, screenshots and controlled release settings'
}
$missingExternal = [Collections.Generic.List[string]]::new()
foreach ($entry in $externalGates.GetEnumerator()) {
    $path = Join-Path $evidenceRoot $entry.Key
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $missingExternal.Add($entry.Value)
    }
}

$status = if ($missingExternal.Count -eq 0) { 'READY_FOR_EXPLICIT_GO' } else { 'ENGINEERING_PASS_EXTERNAL_GATES_OPEN' }
$report = @(
    'LocalTools Suite 4.0.0 submission readiness',
    "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "Status: $status",
    "Package: $($package.Name)",
    "Size: $($package.Length) bytes",
    "SHA-256: $packageHash",
    'Engineering verification: PASS',
    "Open external gates: $($missingExternal.Count)"
)
foreach ($gate in $missingExternal) { $report += "- $gate" }
New-Item -ItemType Directory -Path $evidenceRoot -Force | Out-Null
$report | Set-Content -LiteralPath $reportPath -Encoding utf8
$report | Write-Host

if ($RequireExternalGates -and $missingExternal.Count -gt 0) {
    throw "Public submission is blocked by $($missingExternal.Count) external gate(s)."
}
