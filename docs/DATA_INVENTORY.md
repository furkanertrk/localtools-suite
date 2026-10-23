# LocalTools Suite 4.0.0 — Local Data Inventory

LocalTools Suite has no publisher-operated backend and performs its advertised
file operations locally. This inventory is the source of truth for the Store
privacy declaration and the in-product privacy copy.

| Data | Purpose | Location | Retention | User control |
|---|---|---|---|---|
| Selected input files | User-requested processing and preview | Original user-selected path; read locally | Not copied permanently by the app | User chooses files; remove/clear controls stop use |
| Output files | Result of requested operation | User-selected path or the application's local LocalTools output folder, depending on the tool | Until user deletes them | User manages the resulting files; source inputs are not silently overwritten |
| UI theme and language | Persist preferences | Per-user Qt INI settings | Until reset/uninstall/manual deletion | Settings page |
| Output preferences | Confirm overwrite, auto-open, default folder | Per-user Qt INI settings | Until changed | Settings page |
| Favorite tool IDs | Home-page personalization | Per-user Qt INI settings | Until changed | Favorite buttons |
| Recent file paths | Optional recent list, maximum 10 paths | Per-user Qt INI settings | Until removed/cleared | Keep-history toggle and Clear History |
| Temporary files | Intermediate conversion/rendering | `%TEMP%/LocalTools` | Startup/shutdown cleanup by default | Auto-clean setting and Clear Cache |
| Diagnostic logs | Local troubleshooting | Per-user application data, `app.log` and at most `app.previous.log` | Rotated at 512 KiB; at most two files | Manual profile deletion; no upload control because no upload occurs |
| FFmpeg process data | Local audio/video processing | Process memory and selected local paths | Process lifetime; output follows output rule | Cancel operation; bundled processes terminate on cancel |
| Decoded QR URL | Display and optional explicit browser launch | UI memory | Current task/session | User must invoke Open URL |

## External transmission

No automatic external transmission is implemented. `QDesktopServices` is used
only for local output/folder opening and for a decoded QR URL after explicit user
action. That browser navigation is outside LocalTools Suite and must not be
represented as application telemetry.

## Log minimization

Persistent logging records categories, numeric tool/navigation identifiers,
counts and bounded error categories. Full paths and extracted file contents are
not intentionally persisted. Tests must reject accidental path/content leakage.

## Release-gate caveat

The Store privacy declaration and published HTTPS privacy policy must match this
inventory. Any future analytics, crash upload, cloud storage or update-check
feature requires a new inventory review before implementation.
