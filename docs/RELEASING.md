# Releasing Minicast

The initial version is `0.1.0-alpha.1`. The source repository is [rayandrava/minicast](https://github.com/rayandrava/minicast). Branch pushes run CI; a version tag starts the separate draft-release workflow. Source publication does not publish a binary release. There is no installer, automatic download, executable replacement, or signing service.

## Local package

Use Windows x64 and PowerShell 7 or Windows PowerShell 5.1. Bootstrap obtains the pinned compiler from Zig's official download server; it checks the archive SHA-256 on every run, including cached downloads, then verifies the extracted compiler and libraries against that archive. `-VerifyOnly` performs the same checks without downloading or extracting anything.

```powershell
.\scripts\bootstrap.ps1
.\scripts\package-release.ps1 -OutputDirectory dist\release-review-1
```

Choose a new output directory for every package. Packaging refuses to overwrite an existing directory or running build. It verifies the compiler, runs the complete console test suite, builds the application, and checks the x64 PE architecture, Windows GUI subsystem, version metadata, and embedded icons. It never launches Minicast or tests the desktop. A failed package leaves its folder available for diagnosis; use another output directory after fixing the failure.

When a repository exists, pass its actual `owner/name` to include the manual **Check for Updates** destination:

```powershell
.\scripts\package-release.ps1 -OutputDirectory dist\release-review-2 -Repository owner/name
```

Omitting `-Repository` leaves the destination unconfigured. Builds never guess a repository URL. The generated destination is `https://github.com/owner/name/releases`, which includes prereleases. It is an ordinary browser link, not an update API or background poll.

The output contains:

- `Minicast-<version>-windows-x64.zip`, a portable archive.
- `SHA256SUMS.txt`, checksums for that ZIP and `release.json`.
- `release.json`, version, architecture, optional repository and Git commit, package filename and checksum, and unsigned status. It contains no local paths, user names, build-machine details, or environment dump.
- `stage/`, the exact files used to create the ZIP, retained locally for review. This directory is not uploaded by the workflows.

The ZIP allowlist includes `Minicast.exe`, `LICENSE`, `THIRD_PARTY_NOTICES.md`, `LUCIDE-LICENSE.txt`, `README.md`, `VERSION`, a separate `SHA256SUMS.txt` for the contained files, the project's security/contribution/technical documentation, and the three documents under `docs/`. Lucide and Feather notices retain their relative paths under `assets/lucide/`; seven explicitly listed runtime notices retain their paths under `licenses/`. These are named files, not a recursive copy of the checkout. No user profile, clipboard history, settings, compiler, cache, source checkout, debug symbols, or test executable is included.

## Version metadata

Edit the root `VERSION` file using `major.minor.patch` with an optional SemVer prerelease suffix. Numeric components must fit the Windows version resource (0–65535). A release tag must exactly equal `v` followed by `VERSION`, including case. Use a new version and tag for corrected published packages; do not move existing tags.

`build.ps1` generates the ignored `build/version.h` before compiling C++ or resources:

- `MINICAST_VERSION_STRING` and `MINICAST_VERSION_WSTRING` contain the full version.
- `MINICAST_VERSION_TUPLE` is the numeric `major,minor,patch,0` resource tuple.
- `MINICAST_VERSION_RESOURCE_STRING` is the null-terminated resource string.
- `MINICAST_VERSION_FILEFLAGS` marks prerelease versions.
- `MINICAST_RELEASES_URL_WSTRING` is the configured Releases page, or an empty string.

The file and product version strings retain the complete alpha suffix. Never edit or commit the generated header. The build verifies the resulting metadata instead of assuming the resource compiler accepted it.

## Future GitHub setup

Before enabling releases, protect the default branch and release tags, require reviews for workflow changes, and restrict who may push `v*` tags. Configure the repository's default Actions token permission as read-only. The workflows grant release write access only to the final draft job.

CI runs on ordinary `pull_request` and branch `push` events with read-only repository access, no custom secrets, no credential persistence, and no `pull_request_target` execution. It builds on a fresh GitHub-hosted Windows runner and uploads only the three release files for short-lived review. No compiler or build cache is shared between trusted and untrusted runs.

After reviewing source and desktop behavior, commit the selected version and create an annotated tag. Push only when publication preparation has been explicitly authorized. A `v*` tag triggers the release workflow, which:

1. Checks the tag against `VERSION` and builds/tests/packages with read-only access.
2. Transfers only the ZIP, manifest, and release metadata to a separate job.
3. Checks the version, repository, commit, filenames, and checksums again.
4. Creates a **draft prerelease** with `gh release create --verify-tag --draft --prerelease`.

The write-token job does not check out or execute project code, run the executable, or extract its ZIP. It never publishes the draft or overwrites an existing release. A rerun that encounters an existing release fails; inspect and resolve that state explicitly instead of silently replacing assets. A draft is reviewable by maintainers, but regular users cannot download it until it is published.

All GitHub actions are pinned to full commit hashes from the official `actions` repositories. Dependabot proposes monthly updates; review those changes before merging. Revalidate a new Zig version and its official archive checksum before changing the compiler pin.

## Publication review

Download the candidate ZIP, compare its SHA-256 with the attached manifest, and review the release notes and license notices. Complete the desktop checks in [VALIDATION.md](VALIDATION.md), including an isolated test profile, DPI and theme changes, shortcuts, tray/startup behavior, clipboard retention, and explicit camera use. Console checks do not prove these visual or desktop behaviors.

Only then publish the draft manually. Keep the initial build marked as a prerelease. The executable is **not Authenticode-signed**; Windows may show an unknown-publisher warning. SHA-256 verifies integrity against the supplied manifest, not publisher identity. The pipeline does not claim signing, artifact attestation, or byte-for-byte reproducibility.

Users update manually: open **Check for Updates**, download and verify a published ZIP, exit Minicast from its tray menu, and extract it to the chosen permanent application folder. Preserve the user's existing data directory. Never replace a running executable. A future automatic updater should require authenticated release metadata, publisher verification, safe replacement after exit, and rollback before it is enabled.

## Verification sources

- [Zig's official download index](https://ziglang.org/download/index.json): Zig 0.14.1 Windows x64 SHA-256 `554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c`.
- [GitHub Actions secure-use guidance](https://docs.github.com/en/actions/reference/security/secure-use): least privilege, untrusted code boundaries, and immutable action pins.
- [GitHub CLI release creation](https://cli.github.com/manual/gh_release_create): explicit tag verification, drafts, and prereleases.
- Action pins were checked against the official [checkout](https://api.github.com/repos/actions/checkout/git/ref/tags/v4), [upload-artifact](https://api.github.com/repos/actions/upload-artifact/git/ref/tags/v4), and [download-artifact](https://api.github.com/repos/actions/download-artifact/git/ref/tags/v4) refs on 2026-10-23.
