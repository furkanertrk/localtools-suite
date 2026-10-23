# LocalTools Suite — Architecture

## Architectural goals
1. Keep UI independent from the PDF engine.
2. Make PDF operations independently testable.
3. Keep the application responsive during heavy PDF work.
4. Make it possible to replace the PDF backend later.
5. Keep the architecture simple enough for a solo developer.

## Dependency direction

```text
Qt UI
  |
  v
Application / Services
  |
  v
PDF Operations
  |
  v
PDF Core Abstractions
  |
  v
PDF Backend Adapter
  |
  v
PDFium
```

The dependency direction should not be reversed.

## Suggested source tree

```text
LocalToolsSuite/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── application.h
│   │   └── application.cpp
│   ├── core/
│   │   ├── pdf/
│   │   │   ├── pdf_document.h/.cpp
│   │   │   ├── pdf_page.h/.cpp
│   │   │   ├── pdf_metadata.h/.cpp
│   │   │   └── pdf_renderer.h/.cpp
│   │   ├── backend/
│   │   │   ├── pdf_backend.h/.cpp
│   │   │   └── pdfium_backend.h/.cpp
│   │   └── operations/
│   │       ├── merge_pdf.h/.cpp
│   │       ├── split_pdf.h/.cpp
│   │       ├── extract_pages.h/.cpp
│   │       ├── delete_pages.h/.cpp
│   │       ├── reorder_pages.h/.cpp
│   │       ├── rotate_pages.h/.cpp
│   │       ├── images_to_pdf.h/.cpp
│   │       ├── pdf_to_images.h/.cpp
│   │       ├── pdf_to_text.h/.cpp
│   │       ├── compress_pdf.h/.cpp
│   │       └── edit_metadata.h/.cpp
│   ├── services/
│   │   ├── file_service.h/.cpp
│   │   ├── settings_service.h/.cpp
│   │   ├── temp_file_service.h/.cpp
│   │   ├── recent_files_service.h/.cpp
│   │   └── logging_service.h/.cpp
│   ├── workers/
│   │   └── pdf_worker.h/.cpp
│   └── ui/
│       ├── main_window.h/.cpp
│       ├── common/
│       ├── pages/
│       └── dialogs/
├── tests/
│   ├── unit/
│   └── integration/
├── resources/
│   ├── icons/
│   ├── images/
│   └── themes/
├── packaging/
│   └── msix/
└── docs/
```

## Core abstractions

### PdfBackend
Abstracts the concrete PDF library.

Responsibilities:
- Open a PDF
- Save a PDF
- Create/write documents as required
- Expose functionality required by higher layers without leaking PDFium types

### PdfDocument
Represents an opened PDF document.

Responsibilities:
- Page count
- Access pages
- Metadata access
- Document-level operations required by the application

### PdfPage
Represents a PDF page.

Responsibilities may include:
- Page index
- Dimensions
- Rotation
- Rendering access

### PdfRenderer
Converts PDF pages into Qt images suitable for thumbnails and previews.

The UI should receive `QImage`/similar presentation data rather than PDFium-specific objects.

## UI Information Architecture & Tool Workflows

1. **Home = Tool Launcher**:
   - Home is a tool launcher and dashboard (not a single document workspace).
   - Dropping a PDF on Home triggers an inline Quick Action Card ("Bu PDF ile ne yapmak istiyorsunuz?") allowing 1-click entry into a specific tool workflow with that PDF pre-loaded.
   - Independent tools manage their own file input states.

2. **Organize Suite & Shared `PageWorkspace`**:
   - `Split`, `Extract`, `Delete`, `Reorder`, and `Rotate` share a unified `PageWorkspace` UI component.
   - `PageWorkspace` manages page-level operations, thumbnail grids, page selection models, zoom controls, and rotation previews.
   - `Merge PDF` uses a multi-file list workspace (`MergeWorkspace`) since it requires multiple PDF inputs.

3. **Convert Segmented Workspace**:
   - `Images -> PDF`, `PDF -> Images`, and `PDF -> Text` share a segmented switcher in `ConvertPage`.
   - Each conversion mode manages its specific input validation, preset options, and result formats.

4. **Standalone Workflows**:
   - `Compress PDF` (`OptimizePage`) operates as a single-document preset-based workflow.
   - `Metadata` (`MetadataPage`) operates as a single-document key-value editing workspace.

## Operation model
Each user-facing PDF capability is a focused operation/use case.

Examples:
- MergePdf
- SplitPdf
- ExtractPages
- DeletePages
- ReorderPages
- RotatePages
- ImagesToPdf
- PdfToImages
- PdfToText
- CompressPdf
- EditMetadata

Each operation should accept a request structure and return a clear result/error model.

## Error model
Use explicit error categories such as:
- FileNotFound
- InvalidPdf
- PermissionDenied
- InvalidPageRange
- OutputExists
- InsufficientDiskSpace
- BackendError
- UnsupportedFeature

Do not expose raw PDFium error details directly to users.

## Threading model
All expensive PDF work must run outside the UI thread.

Preferred flow:

```text
UI action
  -> request
  -> worker
  -> operation
  -> backend
  -> progress/result signal
  -> UI update
```

## File handling
Use `std::filesystem::path` for core/application file paths where practical.
Use Qt types at the UI boundary.

Temporary files should be isolated under the application's local temporary directory and cleaned up safely.

## Settings
Use `QSettings` or a thin `SettingsService` abstraction for:
- Theme
- Language
- Default output directory
- Recent files
- User preferences

## Logging
Logs may include:
- Operation type
- Start/end time
- Duration
- Success/failure
- Non-sensitive error codes

Never log:
- PDF contents
- Extracted document text
- Passwords
- Full sensitive metadata unless explicitly needed

## Backend replacement rule
No file outside the backend adapter layer should depend on PDFium-specific headers/types unless there is a strong, documented reason.

This keeps the application portable across PDF engines.

## Release and deployment boundary

Release packaging is a separate, reproducible boundary; a development build directory
is never a distributable product by itself.

Planned dependency direction:

```text
Pinned source revision
  -> CMake x64 Release build
  -> allowlisted install/staging tree
  -> windeployqt + approved third-party runtimes/licenses
  -> AppxManifest + Store assets
  -> MSIX
  -> Partner Center certification + clean-machine test + package hash
  -> Partner Center flight
  -> public Store release
```

Rules:

1. Tests, fixtures, build-system files, logs, local settings, and debug artifacts must
   not enter the production staging tree.
2. The manifest identity must come from the reserved Partner Center product; secrets
   and certificate material must not be committed.
3. Runtime dependencies must be explicit, versioned, licensed, and tested without the
   developer SDK or PATH.
4. The exact package hash that passes Partner Center certification and release validation is submitted.
5. Package capabilities follow least privilege; local file access must use the normal
   user-selected file/folder model wherever possible.
6. The authoritative gates are documented in
   [`RELEASE_HARDENING_PLAN.md`](RELEASE_HARDENING_PLAN.md).
7. Release binaries are produced by a separate Qt Community Edition/LGPL toolchain;
   the Educational/Enterprise installation is not part of release provenance.
8. FFmpeg is a bundled child-process dependency with pinned LGPL-only build provenance;
   GPL/nonfree codec builds are not accepted into the Store staging tree.
9. The manifest minimum is Windows 10 version 1809 (build 17763), x64. No Windows 7
   compatibility layer or alternate legacy build is part of this release architecture.
