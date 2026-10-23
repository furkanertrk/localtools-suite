# MSIX packaging

`AppxManifest.xml` is bound to the Partner Center identity reserved for
LocalTools Suite:

- Name: `FurkanErtrk.LocalToolsSuite`
- Publisher: `CN=A6DAE150-129C-4193-B375-5B693386F8B6`
- Publisher display name: `Furkan Ertürk`
- Version/architecture: `4.0.0.0`, x64
- Minimum OS: Windows 10 version 1809 (`10.0.17763.0`)

The application is a packaged Win32 desktop application. Its only declared
restricted capability is `runFullTrust`, which is required for this entry-point
type. It does not declare internet, broad file-system, location, camera,
microphone or other capabilities. Files are accessed through user-selected paths.

Build the unsigned Store candidate with:

```powershell
.\tools\build_msix.ps1
```

The script rebuilds and audits production staging, generates deterministic-size
PNG assets from the application icon, validates the manifest identity, runs
MakeAppx, unpacks the result, compares its payload to the package root, and
records hashes. The resulting unsigned package is for Partner Center ingestion;
local install/WACK requires an appropriately trusted test signature, while the
Store applies the production signature. Never commit a private signing key.
