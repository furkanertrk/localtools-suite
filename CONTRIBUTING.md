# Contributing to LocalTools Suite

Thank you for your interest in improving LocalTools Suite. This guide explains what kinds of contributions are in scope, the technical rules the project follows, and how to prepare a change for review.

Please read the [licensing status](#licensing-status-important) section before investing significant effort.

## Scope of contributions

The following contributions are welcome:

- **Bug fixes** — crashes, incorrect output, regressions, and edge cases in existing functionality.
- **Documentation** — README, in-code documentation, build and usage guides.
- **Tests** — focused unit or integration tests that cover existing behavior or reproduce a fixed bug.
- **Accessibility and localization improvements** — keyboard navigation, screen-reader support, clearer strings, translation completeness.

**Major feature changes should be discussed in a repository issue before implementation.** This avoids large changes that do not fit the project's direction, architecture, or privacy goals. Describe the problem, a proposed approach, and its impact on existing code before writing the feature.

## Licensing status (important)

**The project's own code is licensed under the [Apache License, Version 2.0](LICENSE). The repository root also carries a [NOTICE](NOTICE) file, which must be preserved in redistributed derivative works (Apache-2.0 §4(d)).**

The license applies to project-owned code only. Third-party components keep their own licenses and notices; nothing in this repository relicenses them.

There is currently **no Contributor License Agreement (CLA) and no DCO** in place, and the project owner has not yet documented an explicit inbound=outbound policy. In its absence:

- By submitting a contribution you are asked to confirm that you may license it under the repository's Apache-2.0 license, so that the project and downstream users can redistribute it.
- Do not submit code you cannot license under Apache-2.0, and do not paste in third-party code without checking its license (see [third-party dependencies](#third-party-dependencies)).
- A formal contribution license policy (for example, an inbound=outbound rule or a contributor license agreement) may be documented here later; if one is added, it will be stated in this section.

Bug reports, issue discussions, and feedback are always useful and are not affected by this.

## Building and testing

The authoritative build and test instructions live in **[BUILD.md](BUILD.md)**. Read it before making changes. It covers:

- The required toolchain: **Qt 6.11.1 with MinGW**.
- The build system: **CMake with Ninja**.
- How to provision the **external PDFium and FFmpeg runtimes**. Their binaries are not
  committed to the repository; only headers, CMake configuration, license texts and
  provenance notes are tracked.

Build and run the test suite (CTest) before submitting any change, and make sure your change does not break existing tests. The steps in BUILD.md are the same ones reviewers expect you to have followed.

## Architecture rules

These rules keep the codebase layered and testable. Follow them in all new code:

- **Dependency direction is strictly:** UI → application/services → operations → backend. A layer may depend on layers below it, never upward.
- **UI code must not call PDFium directly.** All PDF access goes through the operations and backend layers.
- **Keep UI logic out of widgets.** Widgets should present state and forward intents; behavior belongs in the application/service layer where it can be tested.
- **The core must not depend on Qt UI** where it can be avoided. Core and operations code should stay independent of widgets and QML.
- **Long-running operations run on async workers.** Never block the UI thread with PDF, media, or file processing work.
- **Handle errors explicitly and user-friendly.** Check and propagate errors instead of ignoring them; surface failures to the user in plain language with actionable information, and keep detailed diagnostics for logs (subject to the privacy rules below).

## Privacy rules

LocalToolsSuite is a local-first tool. These rules are non-negotiable:

- **Never add network calls, telemetry, analytics, cloud upload, or external AI APIs** without an explicit product decision recorded in an issue. The app must not phone home.
- **Never log document contents or extracted text.**
- **Never log passwords, full sensitive file paths, or document metadata.**

If a change seems to require any of the above, stop and discuss it in an issue first.

## Output safety

- **Operations must not silently overwrite source files.** Input files are never valid output targets without explicit user action.
- **Preserve the existing output-protection behavior.** If your change touches file writing, verify that the existing overwrite guards still apply and are covered by tests.

## Testing

- **Add focused tests where feasible** for bug fixes and new behavior. One test per behavior, with a clear name.
- **Run the test suite with CTest** (see BUILD.md) before submitting, and report the commands you ran in the pull request.
- **Do not weaken, skip, or delete existing tests to make a change pass**, and do not build in release mode to hide assertion failures and claim success.
- Tests are expected to run with **assertions explicitly enabled** (debug/developer builds). A result from a build where assertions are compiled out does not count as verification.

## Code style

- **C++20.** Use modern, idiomatic C++.
- **RAII and smart pointers; no raw owning pointers.** Resources are owned by objects with clear lifetimes.
- **Use `std::filesystem` paths in core code.** Qt types (such as `QString` and `QUrl`) belong at the UI boundary; convert at the boundary.
- **Keep operations small and focused.** One operation, one responsibility.
- **No giant `MainWindow` classes.** Extract functionality into services and operations rather than accumulating code in UI classes.
- **Follow the existing style of the files you touch.** Do not perform broad reformatting; incidental formatting churn makes changes hard to review. If a file genuinely needs reformatting, do it in a separate, dedicated commit.

## Translations

- **Keep English and Turkish UI strings complete.** Every user-visible string must exist in both languages.
- All user-facing strings must go through `tr()`. When you add or change strings, **update `tr_TR.ts`** and **run the translation catalog test** to verify completeness.
- **No unreviewed machine-translated text.** Turkish strings must read naturally; if you are not confident in a translation, say so in the pull request and ask for review.

## Pull request guidelines

- **Keep pull requests small and focused.** One fix, feature slice, or documentation topic per PR.
- **Describe in the PR description:**
  - What behavior changes and why.
  - Any risks or edge cases reviewers should look at.
  - The exact build and test commands you ran, and their results.
- **Include screenshots for UI changes** (before/after where helpful).
- **Do not commit build outputs, binaries, generated artifacts, or secrets.**
- **Branch naming is not prescribed** — the repository's branch and merge process is not finalized yet, so any reasonable descriptive branch name is fine.

## Third-party dependencies

**Do not add a third-party dependency without a license and provenance review.** Before adding any library:

1. Open an issue proposing it, explaining why existing dependencies cannot do the job.
2. Identify the dependency's license and its source/provenance.
3. Wait for the maintainers to confirm it is acceptable alongside the project's own Apache-2.0 license (see [licensing status](#licensing-status-important)).

## Support

Questions about contributing, the build, or anything in this guide: **furkanerturk.dev@gmail.com**
