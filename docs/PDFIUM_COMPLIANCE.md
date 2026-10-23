# PDFium 153.0.7999.0 Release Record

Review date: 2026-08-16

## Approved artifact

- Binary distributor: `bblanchon/pdfium-binaries`
- Immutable release: `chromium/7999` / PDFium 153.0.7999.0
- Release date: 2026-08-11
- Asset: `pdfium-win-x64.tgz`
- Asset SHA-256:
  `55329d5cb5de8a379a2fc563106492d7f385a1f795d18970922c71f708f9fbb4`
- Packaged `pdfium.dll` SHA-256:
  `fb898a1f5ace57805834f390407500bdb6ef93eff326a252ad334a8aae809d8e`
- Upstream branch: `chromium/7999`
- Configuration: x64, release, standalone, V8 disabled, XFA disabled

Release page:

https://github.com/bblanchon/pdfium-binaries/releases/tag/chromium%2F7999

`tools/build_release.ps1` verifies the version file and packaged DLL hash before
compilation. A changed PDFium binary must fail closed and receive a fresh
license/security review.

## Security review

The reviewed build was the distributor's latest immutable release on
2026-08-16. Its branch/version is newer than the published PDFium fixes cited in
Chrome 147–150 security releases, including the April–June 2026 PDFium memory
safety disclosures. Search of the vendor release notes found no later PDFium
advisory preceding the review date.

This comparison is time-bound: PDF parsing remains a high-risk native attack
surface. Before freezing a future Store candidate, check for a newer immutable
PDFium release and newly published PDFium/Chrome advisories. Do not infer that
the absence of a known advisory proves the absence of defects.

## License and provenance

PDFium and its bundled third-party components retain their own license terms.
The package includes the distributor-provided license directory plus the root
PDFium license; `legal/THIRD_PARTY_NOTICES.txt` records source URLs and version.
