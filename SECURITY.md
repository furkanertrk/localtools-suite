# Security Policy

## Overview

LocalTools Suite 4.0.0 is a local Windows x64 application. All core file operations are performed entirely on the user's machine; the application does not communicate with any publisher-operated backend service. This document describes how to report security issues in LocalTools Suite and which issues are considered in scope.

## Reporting a Security Vulnerability

Please report security vulnerabilities **privately**. Do **not** open a public issue, discussion, or pull request describing a vulnerability.

Use one of the following channels:

1. **GitHub private vulnerability reporting** — if this feature is enabled on the repository, submit your report through the repository's security tab.
2. **Email** — if private vulnerability reporting is not available, write to **furkanerturk.dev@gmail.com** with the subject line **"LocalTools Suite security"**.

### What to Include

To help reproduce and assess your report, please include:

- The LocalTools Suite version you tested (**4.0.0**).
- Your exact Windows x64 environment (OS build and relevant configuration).
- Which tool or workflow in the suite is affected.
- Step-by-step reproduction instructions.
- Expected behavior versus actual behavior.
- Logs or crash dumps **only if they are free of sensitive data**.

### Test Data

When demonstrating an issue, use **synthetic or purpose-crafted test files**. Never attach or reference real private documents, customer data, or other confidential material in a report.

## Scope

### In Scope

- Malicious PDF, image, media, or QR input that causes a crash or memory corruption.
- Output file overwrite or silent corruption of user data.
- Privilege-related issues.
- Local path traversal.
- Mishandling of URLs encoded in QR codes.
- Regressions in third-party runtimes that are relevant to the application.

### Out of Scope

- Attacks requiring physical access to the user's machine.
- User-installed malicious DLLs that already run with the user's own privileges.
- Denial of service caused by ordinary, legitimately large files.
- Password-protected files, which are deliberately unsupported.
- Behavior of browsers or websites after a user chooses to open a decoded QR URL.
- Issues in third-party components (Qt, PDFium, FFmpeg) that require disclosure to their upstream projects. You are still encouraged to notify the maintainers so that pinned component updates can be reviewed.

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 4.0.x   | :white_check_mark: |
| < 4.0   | :x: (unsupported unless the publisher states otherwise) |

## Disclosure Policy

- There is **no committed response or fix timeline**; reports are handled on a best-effort basis.
- Coordinated disclosure is requested: please give the publisher the opportunity to investigate and address the issue before making it public.
- There is **no bug bounty program**.

Thank you for helping keep LocalTools Suite and its users safe.
