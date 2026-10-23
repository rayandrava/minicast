# Validation and remaining checks

Review target: `0.1.0-alpha.1`, Windows 11 x64, 2026-10-23. This is an alpha with limited hardware coverage, not a claim that every Windows configuration works.

The local packaging pass completed with **422 assertions and zero failures** across nine console test programs. The x64 GUI executable is **1,122,304 bytes**. Its version metadata and all 27 embedded icon images passed verification. Missing profile paths and unsupported command-line flags were also checked to exit before opening a window or using the normal profile. The public-file scan found no credential patterns, private home paths or runtime profile files; all 26 relative Markdown links resolved. These are scoped checks, not a security certification or a complete desktop test.

## Automated package gates

`scripts/package-release.ps1` runs all of these before producing a ZIP. A failed check stops packaging; a successfully generated package has completed the gates. Console output supplies the assertion counts and any compiler warnings.

| Area | Checks |
| --- | --- |
| Core behavior | Arithmetic, units, currency parsing/formatting, query parsing, JSON and emoji categorization |
| Ranking | Deterministic app matching and the five most-used launches |
| Shortcuts | Key/modifier validation, including Ctrl+Space and Alt+Space |
| Retention | 7/14/30-day policy, expiry, bounds and timestamp edge cases |
| Search typography | DPI and font/caret metric calculations |
| Application targets | Windows AppsFolder IDs, prefixed shell targets and ordinary filesystem paths |
| Profile isolation | Stable normal-instance identity; distinct, bounded identifiers for explicit profiles |
| Storage | Real DPAPI round-trips with synthetic Unicode, maximum entry sizes, failed atomic replacement, clear retry and newer-save races |
| Snapshots | Synthetic RGB32-to-PNG encode/decode, dimensions, colors, opaque output, unchanged input and invalid buffers |
| Binary | Windows GUI subsystem, x64 architecture, embedded version and three icon groups with 27 images |
| Package | Pinned compiler archive/extracted-file integrity, explicit payload allowlist and SHA-256 manifests |

The console tests do not read the system clipboard, open a camera, change startup registration or register global hotkeys. Storage/image fixtures live under the ignored build directory. The compiler emits warnings for Windows dynamic-function pointer casts in the RichEdit/UI Automation loader; the release build does not treat these as errors.

## Completed review observations

- Corrected packaged-app icon resolution by normalizing bare application IDs to `shell:AppsFolder` targets. The production icon worker returned real ChatGPT and Figma images, and those icons were visually checked in the preceding development preview.
- Checked search text, caret/selection and `12eur` rendering in the preceding development preview after the RichEdit DPI correction.
- Corrected clipboard reload limits: valid long CJK entries previously exceeded the UTF-8 reader's smaller limit and could discard the loaded history. The reader now matches capture limits.
- Corrected persistence feedback: clear/delete success waits for a completed disk write; failed saves remain available for retry. Launch ranking now records a copied stable target across Shell activation, which can replace the app index through message dispatch.
- Corrected PNG snapshots: the encoder can negotiate a different format from RGB32. Synthetic encode/decode checks now verify the actual PNG pixels without camera access. Synchronous camera-stream errors also clear the misleading Live status.
- Built the current release preview and observed its light-theme launcher, search focus, result rows and footer in a separate profile. The normal instance continued running. Desktop input checks stopped when the test tool detected concurrent user input; the isolated preview was closed afterward.
- Reviewed public source/documentation for local profiles, private paths and credentials; added vendor/runtime notices and excluded build outputs, toolchains and profile data. No GitHub repository, release, remote or tag was created during local preparation.

## Before publishing the first alpha

These checks remain manual. Record the Windows build, display scale and devices actually used; do not mark them complete based on console tests.

1. Check the current Settings layout, version label and update command. A build without `-Repository` must explain that the release page is unconfigured. A configured build should open the intended repository's Releases page.
2. Record, save, cancel and clear launcher/feature/app shortcuts. Exercise Ctrl+Space and Alt+Space with and without another app owning the chord. Reopen Settings and restart the isolated profile to check persistence.
3. Exercise app launch, usage ranking, emoji insertion, conversion copy, actions and focus return in a disposable document. Use synthetic clipboard text for capture, pause, delete, clear and retention UI checks.
4. Inspect light/dark theme transitions, acrylic with transparency enabled/disabled, rounded corners, dragging and 100/125/150/200% scaling, including mixed-DPI monitors and IME input.
5. Deliberately test a physical webcam: mirror, save/copy, device switch, busy device, unplug, hide, lock and suspend. Do not grant camera permissions implicitly. Synthetic PNG tests do not validate hardware capture or its file dialog.
6. On a suitable test account, verify first-launch startup, disabling it, launching at sign-in, tray restoration and upgrading from one permanent folder to another. Isolated profiles skip automatic startup; their manual Startup toggle still changes the shared user registration.
7. Run CI and the draft-release workflow after repository creation. Download and verify the generated package. Hosted Actions, publication, Authenticode signing and an automatic installer/updater have not been tested or implemented locally.

Do not reuse earlier memory or launch-time measurements as current release benchmarks. Use the explicit `--smoke --data-dir <dedicated-path>` mode in a quiet desktop session for a fresh measurement. The binary size is measured by each build; it is not an idle-RAM measurement.

## Known alpha limits

Clipboard history is text-only; the emoji catalog is curated rather than complete CLDR coverage. Custom-rendered results do not yet expose full screen-reader semantics. The current update command opens a release page and does not compare versions, download or install. Binaries are unsigned. These limitations should remain visible in release notes.
