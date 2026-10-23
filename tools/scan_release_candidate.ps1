[CmdletBinding()]
param(
    [string]$PackagePath = 'artifacts\packages\LocalToolsSuite_4.0.0.0_x64.msix',
    [string]$EvidencePath = 'artifacts\release-evidence\4.0.0\defender-scan.txt'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$package = [IO.Path]::GetFullPath((Join-Path $repoRoot $PackagePath))
$allowedPackageRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts\packages')).TrimEnd('\') + '\'
if (-not $package.StartsWith($allowedPackageRoot, [StringComparison]::OrdinalIgnoreCase) -or
    -not (Test-Path -LiteralPath $package -PathType Leaf)) {
    throw 'PackagePath must identify an existing package below artifacts\packages.'
}

$evidence = [IO.Path]::GetFullPath((Join-Path $repoRoot $EvidencePath))
$allowedEvidenceRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts\release-evidence')).TrimEnd('\') + '\'
if (-not $evidence.StartsWith($allowedEvidenceRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'EvidencePath must resolve below artifacts\release-evidence.'
}

$platformRoot = Join-Path $env:ProgramData 'Microsoft\Windows Defender\Platform'
$scanner = Get-ChildItem -LiteralPath $platformRoot -Directory -ErrorAction Stop |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'MpCmdRun.exe' } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1
if (-not $scanner) {
    throw 'Microsoft Defender command-line scanner was not found.'
}

$scanOutput = (& $scanner -Scan -ScanType 3 -File $package -DisableRemediation 2>&1) -join "`n"
$exitCode = $LASTEXITCODE
$packageHash = (Get-FileHash -LiteralPath $package -Algorithm SHA256).Hash.ToLowerInvariant()
$record = @(
    'LocalTools Suite Microsoft Defender candidate scan',
    "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
    "Package: $([IO.Path]::GetFileName($package))",
    "Package SHA-256: $packageHash",
    "Scanner: $scanner",
    "Exit code: $exitCode",
    '',
    $scanOutput
)
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($evidence)) -Force | Out-Null
$record | Set-Content -LiteralPath $evidence -Encoding utf8

if ($exitCode -ne 0) {
    throw "Microsoft Defender scan failed or detected a threat (exit $exitCode). See $evidence"
}
Write-Host "Microsoft Defender found no threats. Evidence: $evidence"
