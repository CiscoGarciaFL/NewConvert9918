# Release and Packaging Process

This document defines how New Convert 9918 becomes a downloadable,
cross-platform product. It is the detailed contract for Phase 8 in
`PROJECT_PLAN.md`; the project plan remains the short progress checklist.

## User-facing release promise

A release user must not need to install Qt, CMake, a C++ compiler, MinGW,
Xcode, an image library, or another development tool. Every package carries
the Qt libraries, QML modules, platform and image-format plugins, compiler
runtime, application resources, and notices required by that package.

“Standalone” means a self-contained user installation or portable package. It
does not require one literal application binary. The shared-library Qt model
is the release standard because this application uses Qt Quick, QML modules,
platform plugins, and image-format plugins. Static Qt linking is not a Phase 8
goal.

An operating system still provides its normal kernel, desktop/windowing
system, graphics driver, fonts, and other base facilities. Linux compatibility
is bounded by the oldest distribution and C library against which the release
is intentionally built and tested.

## Distribution channel

GitHub Releases is the canonical public distribution channel. A release is
created from an immutable version tag whose commit is contained in `main` and
contains release notes plus all approved binary assets. GitHub's automatically
generated source archives are not substitutes for the user packages.

Stores and package repositories—including Microsoft Store, Mac App Store,
Flathub, Homebrew, and distribution repositories—are optional later channels.
They must consume the same versioned source and pass the same tests; none is a
prerequisite for the first release.

GitHub Releases is also the source of update information, but it does not
update an installed application by itself. The first preview uses manual
updates. An in-application update check may notify the user and open the
appropriate GitHub Release, while automatic download and installation remains
deferred until package signing and update verification are established.

## Preview and beta release policy

The first packaging target, `v0.1.0-beta.1`, was published as a GitHub
prerelease. Tester feedback is delivered through corrective prereleases such
as `v0.1.0-beta.2`, and later builds use monotonically increasing identifiers
such as `v0.1.0-beta.3` and
`v0.1.0-rc.1`; the first stable build then uses the approved stable version.

Preview packages may be unsigned. This is acceptable for testing on Windows,
Linux, and macOS provided that the release notes:

- label every package as preview software rather than a normal production
  download;
- state exactly which Windows or macOS trust warning the tester should expect
  and how to authorize that specific download;
- identify Linux execution or installation steps without weakening system-wide
  security settings;
- disclose that unsigned packages cannot provide the same publisher identity
  or tamper assurance as the later signed release; and
- include SHA-256 checksums for every binary asset.

The preview must use the same intended application names, package identifiers,
settings locations, recipe format, and installation locations as the future
stable release. Adding Windows and Apple signatures later changes package
trust, not application identity. Preview tags and published assets remain
immutable; a correction receives a new prerelease tag.

## Update discovery and installation policy

The initial beta requires a manual download and install from GitHub Releases.
This keeps the first packaging exercise independent of an updater and allows
the packages themselves to be tested before more release infrastructure is
added.

The preferred next step is a notification-only `Help -> Check for Updates`
feature. It should:

1. compare the running application version with published GitHub Releases;
2. offer `Stable only`, `Preview and stable`, and `Do not check
   automatically` preferences;
3. ignore drafts in every channel and ignore prereleases in the stable channel;
4. show the newer version, release date, signing state, important notes, and
   supported platform asset;
5. require an explicit user action before opening the release page or starting
   any download; and
6. never replace files silently or claim an update succeeded before the newly
   installed application is launched and verified.

GitHub's
["latest release" endpoint](https://docs.github.com/en/rest/releases/releases#get-the-latest-release)
excludes prereleases, so the preview channel must enumerate releases and
select the highest compatible non-draft version. Version comparison must use
semantic-version precedence rather than lexical string order. Network or API
failure must be non-fatal and must not interrupt normal editing or conversion.

Full automatic installation is a later feature with separate platform work:

- a Windows NSIS package can perform an explicit user-approved upgrade; a
  future [MSIX App Installer](https://learn.microsoft.com/en-us/windows/msix/app-installer/auto-update-and-repair--overview)
  channel would have a different identity and update contract;
- a macOS updater requires signed update metadata and signed/notarized
  replacement bundles;
- an AppImage may later embed
  [compatible update information](https://docs.appimage.org/packaging-guide/optional/updates.html),
  while Debian packages should normally update through an authenticated
  package repository; and
- portable ZIP users continue to replace the extracted folder unless a safe,
  separately designed portable updater is introduced.

An unsigned beta must not silently install executable updates. A complete
automatic updater requires authenticated metadata, package signature or digest
verification, interrupted-update recovery, rollback behavior, and a tested
settings/recipe migration policy.

## Stable identity and beta-to-official upgrades

The following identities are frozen beginning with `v0.1.0-beta.1`:

| Identity | Frozen value and policy |
| --- | --- |
| Qt organization name | `CiscoGarciaFL`; keep it so existing per-user settings remain discoverable. |
| Qt application name | `NewConvert9918`; keep it so the settings key does not split between preview and stable builds. |
| Windows installer identity | `{9D53324F-AE3A-42FC-96C6-0B2256798410}`; never regenerate it for later versions. |
| Windows install path | `%LOCALAPPDATA%\NewConvert9918`, with `NewConvert9918.exe` and `newconvert9918-cli.exe` under `bin`. |
| macOS bundle identifier | `io.github.ciscogarciafl.NewConvert9918` for both unsigned previews and signed stable bundles. |
| Linux desktop/application ID | `io.github.ciscogarciafl.NewConvert9918`, shared by the desktop file and installed icons. |
| Debian package name | Keep `newconvert9918` so package-manager upgrades replace earlier versions. |
| Recipe and settings schema | Version persisted data and preserve backward compatibility or provide an explicit migration. |
| Application version source | `NEWCONVERT9918_VERSION` in the root `CMakeLists.txt`; GUI, CLI, packages, tags, asset names, and release titles must match it. |

With those identities held stable, the official signed Windows installer can
upgrade the unsigned beta, the official macOS application can replace the
unsigned bundle in Applications, and newer Linux artifacts can replace or
upgrade their matching preview forms. Portable ZIP and AppImage users still
replace the old artifact manually. User settings and recipes must survive all
of these transitions.

The current source uses `CiscoGarciaFL` and `NewConvert9918` for Qt settings
identity. The GUI, CLI, packages, and release workflow derive their version
from `NEWCONVERT9918_VERSION` in the root `CMakeLists.txt`.

## Planned release assets

`<version>` below is the semantic version without the leading tag `v`.

| Platform | Required artifact | Purpose |
| --- | --- | --- |
| Windows x64 | `NewConvert9918-<version>-Windows-x64-Setup.exe` | Recommended per-user graphical installer with shortcuts and uninstall support. |
| Windows x64 | `NewConvert9918-<version>-Windows-x64-Portable.zip` | Installer-free folder containing GUI, CLI, Qt, plugins, runtime, and notices. |
| macOS Apple Silicon | `NewConvert9918-<version>-macOS-arm64.dmg` | Signed and notarized drag-to-Applications GUI bundle. |
| macOS Intel | `NewConvert9918-<version>-macOS-x86_64.dmg` | Signed and notarized drag-to-Applications GUI bundle. |
| Linux x64 | `NewConvert9918-<version>-Linux-x86_64.AppImage` | Primary portable GUI package. |
| Debian/Ubuntu x64 | `newconvert9918_<version>_amd64.deb` | Native package containing the GUI, CLI, desktop entry, icons, and declared dependencies. |
| All binary releases | `SHA256SUMS.txt` | Digest of every published binary artifact. |

Intel and Apple Silicon DMGs are separate initially because both architectures
already have independent CI coverage. A universal macOS bundle can replace
them only after a universal build and package are tested on both architectures.

The filenames and contents above apply to preview and stable releases. Preview
DMGs and Windows executables may be unsigned when the release is clearly
labeled as described above. Signing and notarization become mandatory gates
for the official stable release.

The Windows installer and portable ZIP contain `NewConvert9918.exe` and
`newconvert9918-cli.exe`. The installer does not modify `PATH` by default. The
desktop executable uses the Windows GUI subsystem so Explorer and installer
shortcuts do not open a console window; the CLI deliberately uses the Windows
console subsystem. The release workflow inspects both PE subsystem values. The
Debian package installs both programs in conventional system locations. The
macOS DMG and Linux AppImage focus on the GUI; matching, versioned CLI archives
may be attached when terminal installation instructions and architecture
coverage are finalized.

ARM Linux, 32-bit Windows, and other architectures are not implied by the
first release. They require explicit build, package, and clean-system test
coverage before being advertised.

## Package contents

Each package includes only the runtime files it needs:

- the optimized Release GUI and, where specified, CLI executable;
- Qt Core, GUI, Quick, Quick Controls, and other actually used runtime
  libraries;
- required QML modules;
- the platform plugin for the target package;
- required image-format, icon, accessibility, and style plugins;
- the applicable compiler runtime;
- application icons and embedded resources;
- `LICENSE`, `NOTICE.md`, Qt license text, and third-party notices; and
- release/package metadata sufficient to identify the exact application and
  Qt versions.

Tests, object files, static libraries, source-only development files, CMake
metadata, debug runtimes, developer tools, and unused Qt plugins are excluded.
Symbols may be retained as private CI artifacts or published separately for
diagnostics, but are not placed in normal user packages.

## CMake deployment and packaging design

The installed tree is the source of every package. Packaging must not copy an
arbitrary developer build directory.

1. `install(TARGETS ...)` installs the GUI, CLI, icons, license files, and
   platform metadata into a staging prefix.
2. Qt's `qt_generate_deploy_qml_app_script()` deployment API gathers QML
   modules, Qt libraries, plugins, and supported runtime dependencies for the
   GUI.
3. CLI runtime dependencies not already supplied by the GUI deployment are
   collected explicitly.
4. CPack or a narrowly scoped platform packaging script consumes only the
   staged install tree.
5. A package-content audit rejects debug libraries, build-machine paths,
   unintended plugins, missing notices, and unresolved dynamic dependencies.

Windows uses the Qt deployment result to produce an NSIS installer and a ZIP
from the same staged tree. macOS uses a proper `.app` bundle and DMG. Linux uses
an AppImage tool selected and pinned during implementation; CPack produces the
Debian package. Packaging tool versions are pinned in release automation.

## Version and release policy

- CMake's project version, application version metadata, package version, tag,
  artifact names, and release title must agree.
- Tags use `vMAJOR.MINOR.PATCH`, with prerelease tags such as
  `v0.1.0-alpha.1` or `v1.0.0-rc.1` where appropriate.
- Alpha, beta, and release-candidate builds are marked as GitHub prereleases.
- Stable releases are never built from an unreviewed feature branch.
- A published artifact is never silently replaced. Corrections receive a new
  version and tag.
- Release notes identify supported operating systems and architectures,
  conversion compatibility changes, known issues, and any recipe or settings
  migration concern.

The first packaging exercise was published as `v0.1.0-beta.1`. Corrective
package changes are released under a new tag rather than replacing its assets.
Preview builds may be unsigned under the policy above. `v1.0.0` remains gated
by the v1.0 criteria in `PROJECT_PLAN.md`.

## Prerelease readiness checklist

Before tagging each prerelease:

- make CMake the single application-version source and verify that the GUI,
  CLI, package metadata, tag, asset names, and release title agree;
- freeze the Windows installer ID, macOS bundle ID, Linux desktop/application
  ID, executable names, install locations, Debian package name, and per-user
  settings identity;
- generate the Windows setup and portable ZIP, Linux AppImage and Debian
  package, and both macOS DMGs from clean CI checkouts;
- audit each package for required Qt/QML/runtime files, missing dependencies,
  debug files, build-machine paths, licenses, and notices;
- run the package smoke tests on clean systems with no development Qt install;
- verify beta-to-beta upgrade or replacement without losing settings or saved
  recipes;
- create and verify `SHA256SUMS.txt`;
- prepare release notes with preview status, signing state, platform support,
  expected trust prompts, installation steps, known issues, and manual update
  instructions; and
- publish a complete GitHub prerelease only after every required packaging job
  and final asset-set check succeeds.

The notification-only update checker is recommended before later prereleases
or the first stable release, but it does not block `v0.1.0-beta.1`. A full
automatic installer is not a pre-beta requirement.

## GitHub Actions release workflow

The existing build workflow remains the pull-request and push validation path.
A separate `.github/workflows/release.yml` performs packaging and publishing.
It is triggered by an approved version tag or an explicit manual prerelease
dispatch.

The release flow is:

```text
version/tag validation
    -> native Windows, Linux, Intel Mac, and Apple Silicon Mac builds
    -> complete automated tests
    -> staged Qt/runtime deployment
    -> package creation
    -> package-content and unresolved-dependency audit
    -> signing/notarization where required
    -> clean-runner package smoke tests
    -> SHA-256 generation
    -> exact final asset-set and checksum verification
    -> immutable GitHub prerelease with all assets
```

The workflow uses least-privilege permissions. Signing identities, tokens, and
notarization credentials live in protected GitHub environment secrets, are not
available to pull-request jobs, and are not printed in logs. Packaging jobs
upload intermediate artifacts for diagnosis, but only reviewed final packages
are attached to the release.

The release is created only after all platform jobs succeed. The publish job
verifies names, versions, checksums, notices, release notes, and the exact
asset set before creating the visible prerelease. If the tag already has a
release, the workflow refuses to replace it.

## Signing and platform trust

### Windows

Stable Windows installer and executable files are code-signed before public
release. The signing service or certificate is selected before the first
stable release and held outside the repository. Timestamping is required so a
valid release remains verifiable after certificate expiration.

Unsigned developer and prerelease packages may be produced for controlled
testing, but must be labeled clearly and are not promoted as the normal public
download. Self-signing is not treated as equivalent to a publicly trusted
signature.

### macOS

Public DMGs require an Apple Developer ID signing identity. The application,
embedded frameworks, plugins, and CLI/helper executables are signed with the
hardened runtime, submitted to Apple's notarization service, and stapled. CI
verifies both the code signature and Gatekeeper assessment before publishing.

### Linux

Linux artifacts receive SHA-256 checksums. GPG signing may be added when a
maintainer key and rotation policy are established. The Debian package records
its dependencies and package metadata and is published with mode `0644`; the
AppImage is checked for unresolved libraries and tested outside the build
directory. Desktop package installers depend on a functioning authorization
agent and may fail silently when it is unavailable. Release notes therefore
document `sudo apt install ./<package>.deb` as the reliable fallback without
weakening system-wide security settings.

## Supported-system policy

The first beta supports Windows 10 version 1809 or newer on x64, macOS 13 or
newer on Intel x86_64 and Apple Silicon arm64, and Linux x86_64 with glibc 2.34
or newer. Ubuntu 22.04 is the pinned Linux build and package-test baseline. X11
is the declared Linux display target for this beta; Wayland through Qt may work
but is not yet part of the support promise. No architecture-specific CPU
instructions beyond each platform's normal x86_64 or arm64 baseline are
required.

The CI operating system used to produce a Linux release is pinned rather than
`ubuntu-latest`; it must be old enough for the declared compatibility target.
The macOS deployment target is explicit in CMake. Packages are tested on the
oldest supported system and at least one current system.

## Clean-system acceptance tests

Every release package is tested from the package itself, not from the build
tree. At minimum, each supported platform verifies:

1. Download or transfer the final artifact and verify its SHA-256 digest.
2. Install, mount, extract, or enable the package exactly as documented.
   For Debian packages, verify the normal desktop installer where available
   and the documented terminal fallback when its authorization agent fails.
3. Launch the GUI with no Qt or compiler installed separately. On Windows,
   launching from Explorer or an installer shortcut must not open a console
   window, while the separate CLI must retain console behavior.
4. Confirm icons, fonts, theme, file dialogs, menus, QML controls, and image
   plugins load without missing-module warnings.
5. Open representative PNG, JPEG, PCX, and supported retro-format inputs.
6. Run representative TMS9918A and F18A conversions.
7. Export at least one raw and one wrapped output and verify the manifest.
8. Save and reload a recipe containing current editor data.
9. Run a representative CLI conversion where the package includes the CLI.
10. Relaunch after reboot or logout where installation state is relevant.
11. Upgrade from the preceding released version without losing user settings.
12. Uninstall or remove the application and verify that only documented user
    settings remain.

Automated smoke tests cover everything practical on fresh hosted runners.
Signing, Gatekeeper, SmartScreen, desktop integration, and real installer UX
also receive a recorded manual check on physical or clean virtual systems.

## Licensing and attribution gate

Before publication:

- confirm that the original author's permission covers public binary
  distribution of this derived cross-platform application and its package
  formats;
- retain the complete project `LICENSE` and `NOTICE.md` in every package;
- record the exact Qt edition, version, modules, and applicable LGPL/GPL or
  commercial terms;
- include Qt and bundled third-party notices and available SBOM information;
- verify every added packaging/runtime dependency against the project's custom
  license constraints; and
- exclude GPL-only optional components unless a separately approved licensing
  decision permits them.

This checklist is a release gate, not legal advice. Unresolved redistribution
or notice questions block publication.

## Release notes and support information

Each GitHub Release states:

- whether it is alpha, beta, release candidate, or stable;
- supported platforms and architectures;
- which asset ordinary users should download;
- installation or portable-launch instructions;
- major changes and compatibility differences;
- known limitations and deferred features;
- checksum-verification instructions;
- links to the user documentation, license, attribution, and issue tracker; and
- whether packages are signed/notarized.

Release notes must not call an untested architecture or operating-system
version supported.

## Current implementation status

The repository builds and tests Release code on Windows, Ubuntu 22.04, Intel
macOS, and Apple Silicon macOS. CMake owns the version and frozen application
identities, stages the GUI, CLI, notices, exact LGPL/GPL texts, Qt/QML modules,
plugins, and runtime dependencies, and supplies CPack metadata. The tag-driven
release workflow builds Windows setup/portable, Linux AppImage/DEB, and native
Intel and Apple Silicon DMG assets, audits staged contents and dependencies,
runs GUI and representative CLI package smoke tests, verifies the complete
asset set, generates SHA-256 checksums, and publishes an immutable GitHub
prerelease.

The Windows portable tree has also passed the package audit and GUI/CLI smoke
test locally. Feedback from `v0.1.0-beta.1` identified a Windows GUI-subsystem
error and an unreliable graphical Debian installation path; `v0.1.0-beta.2`
corrects the former, normalizes Debian package permissions, and documents the
terminal fallback for the latter. The notification-only update checker remains
optional for a later prerelease, and automatic installation remains
intentionally deferred. Stable Windows and macOS publication still requires
the signing and notarization gates described above.

## Phase 8 completion definition

Phase 8 is complete only when:

- every required artifact is reproducibly generated from a clean tagged
  checkout;
- each package runs on clean supported systems without development software;
- the complete GUI and CLI test suites pass before packaging;
- package smoke tests and dependency audits pass;
- required Windows and macOS trust checks pass;
- notices and checksums are present and verified;
- GitHub creates a complete immutable prerelease automatically; and
- a maintainer can publish using this document without relying on undocumented
  workstation state or conversation history.
