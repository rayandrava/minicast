# Minicast

A small, keyboard-first Windows launcher. Native C++20, Win32, Direct2D and DirectComposition—no browser runtime, account or telemetry.

**Status:** preparing `0.1.0-alpha.1`. Source is public; no binary release has been published yet. This is an early preview, not a stable release. Windows 11 x64 is the initial supported target. Acrylic requires Windows 11 22H2 or newer with transparency enabled; the interface falls back to a solid surface when unavailable.

## What it does

- Launch installed apps, with real app icons and your five most-used Minicast launches first.
- Calculate and convert currencies, units and times in one search field.
- Find emoji and paste them into your previous app.
- Keep optional, locally encrypted text and image clipboard history for 7, 14 or 30 days.
- Open a webcam mirror with optional PNG snapshots.
- Configure global shortcuts for the launcher, features and individual apps.

Clipboard history has All Types, Text and Images filters, image previews, pixel dimensions and stored size. PNG and Windows bitmap clipboard formats are supported.

Overflowing result lists and the emoji grid have draggable scrollbars and smooth pixel scrolling. Scrolling keeps selection unchanged; arrow keys navigate selections.

The palette follows your Windows app theme. It can run from the tray, start at sign-in, and be dragged around the screen. The double-crescent logo and the application are original project assets. Raycast and Tinycast inspired the interaction; this project is independent and unaffiliated with either.

## Run

When a release is available, download its Windows x64 ZIP from [Releases](https://github.com/rayandrava/minicast/releases), extract it to a permanent folder, and open `Minicast.exe`. Keep the license files with it. No installer, terminal, administrator access or development runtime is required. Until then, use the build instructions below.

The first normal launch enables **Start with Windows**, as shown in Settings. You can turn it off. **Close to tray** controls the close button/Alt+F4; Escape and clicking away dismiss the palette. Use the tray menu's **Exit** to quit completely.

Default launcher shortcut: **Ctrl+Alt+Space**. Open Settings with the slider icon, `settings`, or **Ctrl+,**. Edit shortcuts directly in the list: click a shortcut, press the new combination, and press Enter. Escape cancels; the × button clears it. Ctrl+Space and Alt+Space are supported when Windows or another app does not already own them.

| Type | Result |
| --- | --- |
| `notepad` | Find an installed app |
| `(125 + 75) * 1.1` | Calculate |
| `25 c to f` | Convert units |
| `5 ft 10 in to cm` | Convert mixed feet/inches |
| `12eur` | Convert euros to your Windows home-country currency |
| `100 usd to idr` | Convert to an explicit currency |
| `time Tokyo` | Current time in a city |
| `9am Jakarta to London tomorrow` | Convert between time zones |
| `emoji happy` | Find an emoji |
| `clipboard invoice` | Search retained clipboard text or `image` |
| `camera` | Open the webcam mirror |
| `refresh apps` / `refresh rates` | Refresh the respective cache |
| `about minicast` | Show the build version |
| `check for updates` | Open the configured GitHub Releases page |

Enter opens a result or copies a conversion. Emoji and clipboard results also attempt to paste into the previously focused app; **Ctrl+Enter** copies only. **Ctrl+K** opens actions. The camera starts only when its command is opened and stops when hidden. Enter saves a snapshot; Ctrl+Enter copies it.

## Updates

The first alpha uses **manual portable updates**. Check for Updates opens GitHub Releases; it does not silently download or execute code. Local development builds without a configured repository explain that no release page is configured.

1. Download and extract the new ZIP into a new folder.
2. Exit the old Minicast using its tray menu.
3. Open the new `Minicast.exe`.
4. Keep the old folder until the new version works for you.

Preferences and history remain in `%LOCALAPPDATA%\Minicast`. An enabled startup registration follows the new executable on launch; a disabled registration stays disabled. Do not replace a running executable. Downgrades across future data-format changes may need the migration guidance in their release notes.

Maintainers: the [release pipeline](docs/RELEASING.md) tests and packages version tags, generates checksums and prepares a **draft release** for review. Automatic in-app installation, signed binaries and rollback are separate future work. SHA-256 checksums detect changed downloads; they are not an Authenticode publisher signature.

## Privacy and limits

- Clipboard history is **off by default**, supports text and bitmap images, limited to 500 entries / 128 MiB (64 MiB per image), and encrypted with Windows user-scoped DPAPI. Retention is configurable. Recognized clipboard exclusion markers are honored; universal password detection is not promised.
- App names/paths, usage ranking, preferences and daily exchange-rate caches are stored locally. Clipboard contents and search queries are not sent to the currency provider.
- Rates come from Frankfurter over HTTPS, refreshed once per local day at launch. Cached rates work offline. These are dated reference rates, not bank quotes.
- Camera frames stay in memory unless you explicitly save or copy a snapshot. Audio is not captured.
- The app adds no service, scheduled task or global keyboard hook. Updates are user-initiated.

See [privacy details](docs/PRIVACY.md), [security reporting](SECURITY.md) and [validation status](docs/VALIDATION.md). Known gaps include complete result-list accessibility, full emoji/CLDR coverage, code signing, and testing across more cameras, keyboard layouts and mixed-DPI monitor setups.

## Build and test

Use PowerShell on Windows x64. The bootstrap downloads a pinned Zig 0.14.1 compiler and verifies its SHA-256 checksum. The compiler is a development dependency and is not shipped with Minicast.

```powershell
.\scripts\bootstrap.ps1
.\build.ps1 -Test -Output 'dist\local\Minicast.exe'
```

Tests are console-only. Building does not launch the app, enable clipboard capture, activate the camera or change startup settings.

```powershell
# Optional: wire the release link for your own repository.
.\build.ps1 -Test -Repository 'OWNER/REPOSITORY' -Output 'dist\local-configured\Minicast.exe'

# Build and package a release in a fresh output directory.
.\scripts\package-release.ps1 -OutputDirectory 'dist\release-local'
```

For a manual development session, use a dedicated profile:

```powershell
.\dist\local\Minicast.exe --inspect --data-dir "$PWD\build\manual-profile"
```

An isolated profile uses its own instance, cache and preferences, starts with no global hotkeys, and skips automatic sign-in registration. Do not toggle Start with Windows during isolated tests; it controls the normal Windows startup registration. `--inspect` makes the window targetable by UI test tools. `--smoke` runs a short visible launch/conversion/hide measurement and exits; it is intentionally not part of CI.

See [the technical design](TECHNICAL_PLAN.md) and [contributing guide](CONTRIBUTING.md). Keep changes native, bounded and measurable; avoid adding services, idle polling or bundled UI runtimes.

## License

Minicast's original code and artwork are [MIT licensed](LICENSE). Vendored icons and linked runtime components retain their own licenses; see [third-party notices](THIRD_PARTY_NOTICES.md) and `licenses/`. System fonts and installed-app icons are obtained from Windows at runtime and are not redistributed as project artwork.
