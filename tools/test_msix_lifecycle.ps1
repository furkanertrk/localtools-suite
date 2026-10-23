[CmdletBinding()]
param(
    [string]$WindowsSdkVersion = '10.0.19041.0'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$artifactRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts'))
$sourcePackage = Join-Path $artifactRoot 'packages\LocalToolsSuite_4.0.0.0_x64.msix'
$testRoot = [IO.Path]::GetFullPath((Join-Path $artifactRoot 'lifecycle-test'))
$allowedPrefix = $artifactRoot.TrimEnd('\') + '\'
if (-not $testRoot.StartsWith($allowedPrefix, [StringComparison]::OrdinalIgnoreCase) -or
    $testRoot -eq $artifactRoot) {
    throw 'Lifecycle test directory escaped the repository artifact root.'
}
if (-not (Test-Path -LiteralPath $sourcePackage -PathType Leaf)) {
    throw 'MSIX candidate is missing. Run tools/build_msix.ps1 first.'
}

$packageName = 'FurkanErtrk.LocalToolsSuite'
$publisher = 'CN=A6DAE150-129C-4193-B375-5B693386F8B6'
$packageFamilyName = 'FurkanErtrk.LocalToolsSuite_nf279kfe3pf3g'
$existing = Get-AppxPackage -Name $packageName -ErrorAction SilentlyContinue
if ($existing) {
    throw "Refusing to replace an existing installed package: $($existing.PackageFullName)"
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this lifecycle test from an elevated (Run as administrator) PowerShell window.'
}

$signTool = "C:\Program Files (x86)\Windows Kits\10\bin\$WindowsSdkVersion\x64\signtool.exe"
$makeAppx = "C:\Program Files (x86)\Windows Kits\10\bin\$WindowsSdkVersion\x64\makeappx.exe"
foreach ($sdkTool in @($signTool, $makeAppx)) {
    if (-not (Test-Path -LiteralPath $sdkTool -PathType Leaf)) {
        throw "Required Windows SDK tool was not found: $sdkTool"
    }
}

if (Test-Path -LiteralPath $testRoot) {
    Remove-Item -LiteralPath $testRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$signedPackage = Join-Path $testRoot 'LocalToolsSuite_4.0.0.0_x64_test-signed.msix'
$previousPackage = Join-Path $testRoot 'LocalToolsSuite_3.9.0.0_x64_test-signed.msix'
$previousRoot = Join-Path $testRoot 'previous-package-root'
$certificatePath = Join-Path $testRoot 'LocalToolsSuite-test.cer'
$pfxPath = Join-Path $testRoot 'LocalToolsSuite-test.pfx'
Copy-Item -LiteralPath $sourcePackage -Destination $signedPackage -Force

# Build a lower-version package from the exact candidate payload. It exists
# only inside the disposable lifecycle-test directory and makes the update
# path testable without retaining or trusting an unrelated historic binary.
& $makeAppx unpack /p $sourcePackage /d $previousRoot /o | Out-Null
if ($LASTEXITCODE -ne 0) { throw "MakeAppx unpack failed with exit code $LASTEXITCODE." }
[xml]$previousManifest = Get-Content -LiteralPath (Join-Path $previousRoot 'AppxManifest.xml')
$previousManifest.Package.Identity.Version = '3.9.0.0'
$previousManifest.Save((Join-Path $previousRoot 'AppxManifest.xml'))
Remove-Item -LiteralPath (Join-Path $previousRoot 'AppxBlockMap.xml') -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $previousRoot 'AppxSignature.p7x') -Force -ErrorAction SilentlyContinue
& $makeAppx pack /d $previousRoot /p $previousPackage /o | Out-Null
if ($LASTEXITCODE -ne 0) { throw "MakeAppx pack failed with exit code $LASTEXITCODE." }

$certificate = $null
$trustedCertificate = $null
$installedPackage = $null
$appProcess = $null
$passwordText = [Convert]::ToBase64String([Security.Cryptography.RandomNumberGenerator]::GetBytes(24))
$password = ConvertTo-SecureString -String $passwordText -AsPlainText -Force
$result = 'FAIL'
$details = @()

function Start-AndVerifyPackagedApp {
    param([string]$ExpectedStage)

    Start-Process explorer.exe -ArgumentList "shell:AppsFolder\$packageFamilyName!App" | Out-Null
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    $process = $null
    do {
        Start-Sleep -Milliseconds 250
        $process = Get-Process -Name LocalTools -ErrorAction SilentlyContinue |
            Select-Object -First 1
    } while (-not $process -and [DateTime]::UtcNow -lt $deadline)
    if (-not $process) { throw "$ExpectedStage application launch timed out after 15 seconds." }

    Start-Sleep -Seconds 3
    if ($process.HasExited) {
        throw "$ExpectedStage application exited unexpectedly with code $($process.ExitCode)."
    }
    Stop-Process -Id $process.Id -Force -ErrorAction Stop
    return "$ExpectedStage launch: PASS"
}

try {
    $certificate = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject $publisher `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -KeyAlgorithm RSA `
        -KeyLength 3072 `
        -HashAlgorithm SHA256 `
        -NotAfter (Get-Date).AddDays(7)

    Export-PfxCertificate -Cert $certificate -FilePath $pfxPath -Password $password | Out-Null
    Export-Certificate -Cert $certificate -FilePath $certificatePath -Type CERT | Out-Null
    $trustedCertificate = Import-Certificate `
        -FilePath $certificatePath `
        -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople'

    foreach ($packageToSign in @($previousPackage, $signedPackage)) {
        & $signTool sign /fd SHA256 /f $pfxPath /p $passwordText $packageToSign
        if ($LASTEXITCODE -ne 0) { throw "SignTool failed with exit code $LASTEXITCODE." }
        $signature = Get-AuthenticodeSignature -FilePath $packageToSign
        if (-not $signature.SignerCertificate -or
            $signature.SignerCertificate.Thumbprint -ne $certificate.Thumbprint) {
            throw "The test package is not signed by the generated certificate: $packageToSign"
        }
    }

    # Install and launch the disposable previous version.
    Add-AppxPackage -Path $previousPackage -ForceApplicationShutdown
    $installedPackage = Get-AppxPackage -Name $packageName -ErrorAction Stop
    if ($installedPackage.PackageFamilyName -ne $packageFamilyName) {
        throw "Installed package family mismatch: $($installedPackage.PackageFamilyName)"
    }
    if ($installedPackage.Version.ToString() -ne '3.9.0.0') {
        throw "Previous-version install mismatch: $($installedPackage.Version)"
    }
    $details += "Previous install: $($installedPackage.PackageFullName)"
    $details += Start-AndVerifyPackagedApp 'Previous-version install'

    # Upgrade in place to the exact 4.0.0.0 candidate and launch it.
    Add-AppxPackage -Path $signedPackage -ForceUpdateFromAnyVersion -ForceApplicationShutdown
    $installedPackage = Get-AppxPackage -Name $packageName -ErrorAction Stop
    if ($installedPackage.Version.ToString() -ne '4.0.0.0') {
        throw "Upgrade version mismatch: $($installedPackage.Version)"
    }
    $details += "Upgraded package: $($installedPackage.PackageFullName)"
    $details += Start-AndVerifyPackagedApp 'Upgrade'

    # Uninstall, verify removal, then reinstall and relaunch the same candidate.
    Remove-AppxPackage -Package $installedPackage.PackageFullName -ErrorAction Stop
    if (Get-AppxPackage -Name $packageName -ErrorAction SilentlyContinue) {
        throw 'Package remained installed after the uninstall step.'
    }
    $details += 'Uninstall: PASS'

    Add-AppxPackage -Path $signedPackage -ForceApplicationShutdown
    $installedPackage = Get-AppxPackage -Name $packageName -ErrorAction Stop
    if ($installedPackage.Version.ToString() -ne '4.0.0.0') {
        throw "Reinstall version mismatch: $($installedPackage.Version)"
    }
    $details += "Reinstalled package: $($installedPackage.PackageFullName)"
    $details += Start-AndVerifyPackagedApp 'Reinstall'
    $result = 'PASS'
} finally {
    if ($appProcess -and -not $appProcess.HasExited) {
        Stop-Process -Id $appProcess.Id -Force -ErrorAction SilentlyContinue
    }
    $installedNow = Get-AppxPackage -Name $packageName -ErrorAction SilentlyContinue
    if ($installedNow) {
        Remove-AppxPackage -Package $installedNow.PackageFullName -ErrorAction Continue
    }
    if ($trustedCertificate) {
        Remove-Item -LiteralPath "Cert:\LocalMachine\TrustedPeople\$($trustedCertificate.Thumbprint)" `
            -Force -ErrorAction SilentlyContinue
    }
    if ($certificate) {
        Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($certificate.Thumbprint)" `
            -Force -ErrorAction SilentlyContinue
    }

    $evidenceDirectory = Join-Path $artifactRoot 'release-evidence\4.0.0'
    New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
    @(
        "Generated (UTC): $([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))",
        "Result: $result",
        "OS: $([Environment]::OSVersion.VersionString)",
        'Method: temporary code-signing certificate; 3.9.0.0 install/launch; in-place 4.0.0.0 upgrade/launch; uninstall; 4.0.0.0 reinstall/launch; final uninstall and certificate cleanup.',
        $details
    ) | Set-Content -LiteralPath (Join-Path $evidenceDirectory 'local-msix-lifecycle.txt') -Encoding utf8

    if (Test-Path -LiteralPath $testRoot) {
        Remove-Item -LiteralPath $testRoot -Recurse -Force
    }
}

if ($result -ne 'PASS') { throw 'Local MSIX lifecycle test failed.' }
Write-Host 'Local MSIX install, upgrade, launch, uninstall, reinstall and certificate cleanup passed.'
