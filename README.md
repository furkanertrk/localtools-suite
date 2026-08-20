<div align="center">

# LocalTools Suite

### Powerful tools. Private by design.

A privacy-focused Windows desktop utility suite for working with PDFs, images, documents, audio, video, QR codes, file hashes, and metadata — with core file processing performed locally on your device.

[![Microsoft Store](https://img.shields.io/badge/Microsoft%20Store-Get%20LocalTools%20Suite-0078D4?logo=microsoft&logoColor=white)](https://apps.microsoft.com/detail/9P1RLFPFKN6N?hl=tr-tr&gl=TR&ocid=pdpshare)
![Version](https://img.shields.io/badge/version-4.0.0-2F81F7)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-0078D4?logo=windows11&logoColor=white)
![Architecture](https://img.shields.io/badge/architecture-x64-555555)
![Languages](https://img.shields.io/badge/UI-English%20%7C%20Türkçe-555555)

[Microsoft Store](https://apps.microsoft.com/detail/9P1RLFPFKN6N?hl=tr-tr&gl=TR&ocid=pdpshare) · [Privacy Policy](https://furkanertrk.github.io/localtools-suite/) · [Support](mailto:furkanerturk.dev@gmail.com)

</div>

---

## About

**LocalTools Suite** is a native Windows desktop application that brings common file utilities together in one focused interface.

Instead of opening a different website for every PDF, image, audio, video, QR, hash, or metadata task, LocalTools Suite keeps these workflows available directly on your PC. The product is designed around a simple principle: **supported file operations should not require uploading your documents or media to an online processing service.**

The application is available publicly on the **Microsoft Store**.

## Highlights

- **Local-first processing** — supported file workflows run on the user's device; no document upload is required for core tools.
- **One desktop suite** — PDF, image, document, media, QR, hash, and metadata utilities in a single application.
- **No account required** — core tools can be used without creating an account or signing in.
- **Free first release** — no subscription, paid add-on, or in-app purchase in version 4.0.0.
- **English & Turkish UI** — the interface can switch languages without restarting the application.
- **Windows-native distribution** — packaged as x64 MSIX and distributed through the Microsoft Store.

## Features

| Area | Included tools |
| --- | --- |
| **PDF Organize** | Merge, split, extract pages, delete pages, reorder pages, rotate pages |
| **PDF Convert** | Images → PDF, PDF → Images, PDF → Text |
| **PDF Optimize & Metadata** | Compress PDF, view/edit PDF metadata |
| **Images** | Convert image formats, compress images |
| **Documents & ID** | A4 ID-card copy sheets, passport/biometric photo sheets |
| **File Utilities** | Calculate and verify MD5, SHA-1, SHA-256, and SHA-512 hashes |
| **QR & Metadata** | Generate QR codes, read QR codes from images, inspect and clean supported file metadata |
| **Video** | Compress video, video → GIF, extract audio, trim video |
| **Audio** | Convert audio, compress audio, trim audio |

## Privacy by design

LocalTools Suite is built for workflows where files may contain private or sensitive information.

- Selected documents and media are processed locally for supported operations.
- The application does not require a publisher-controlled cloud processing backend for core tools.
- No telemetry is designed to collect document contents.
- Logs are designed to avoid document contents, extracted text, passwords, and unnecessary sensitive metadata.
- Users control which files are opened and where outputs are written.

For details about local preferences, recent-file history, temporary files, logs, retention, and user controls, see the full **[Privacy Policy](https://furkanertrk.github.io/localtools-suite/)**.

## Engineering

LocalTools Suite was developed as a production-oriented Windows desktop application rather than a demo project.

### Technology

- **C++ / Qt 6** for the native desktop application and UI
- **PDFium** for PDF rendering and document operations
- **FFmpeg** for local audio/video processing
- **qrcodegen / quirc** for QR generation and decoding
- **MSIX** for Windows packaging and Microsoft Store distribution
- Asynchronous worker architecture for long-running file operations

### Release engineering

The Microsoft Store release candidate went through a dedicated hardening and evidence process, including:

- **34 / 34 automated Release tests passed**
- **714 / 714 Turkish translation entries completed**
- Automated UI regression coverage across language, theme, and DPI combinations
- Release staging audits to prevent debug/test artifacts from entering the package
- Dependency provenance and third-party license records for Qt, PDFium, FFmpeg, and QR components
- Microsoft Defender scan of the release candidate with no threats found
- SHA-256 evidence tying the tested candidate to the submitted MSIX
- Microsoft Store package validation and certification

## Requirements

- **Windows 10 version 1809 (build 17763) or later**
- **Windows 11** supported
- **x64** architecture

Windows 7, Windows 8, and Windows 8.1 are not supported by the Store release.

## Install

The recommended installation method is the Microsoft Store:

### [Download LocalTools Suite from Microsoft Store](https://apps.microsoft.com/detail/9P1RLFPFKN6N?hl=tr-tr&gl=TR&ocid=pdpshare)

Store ID: `9P1RLFPFKN6N`

## Current release

**LocalTools Suite 4.0.0** is the first public Microsoft Store release.

The initial release focuses on reliable local file tooling, privacy, localization, packaging quality, and a polished Windows desktop experience rather than cloud accounts or online processing.

## Repository scope

This public repository currently hosts **product information and the public privacy policy** for LocalTools Suite. The application source code is not included in this repository.

## Support

For product support, bug reports, or privacy-related questions:

**furkanerturk.dev@gmail.com**

## Author

Developed and published by **Furkan Ertürk**.

---

<div align="center">

**LocalTools Suite — useful file tools, without turning your files into uploads.**

</div>
