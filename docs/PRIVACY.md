# Privacy and local data

This document describes the current source implementation, not a hosted service. Minicast has no account system, telemetry, analytics SDK, crash upload, cloud clipboard sync, or automatic updater.

## Network access

On launch, Minicast requests a USD-based daily currency snapshot from `https://api.frankfurter.dev/v2/rates?base=USD` when a successful snapshot has not already been fetched that local day. **Refresh Exchange Rates** can request another snapshot. Failed refreshes keep the existing cache and may be retried on a later launch.

These HTTPS requests send a Minicast user-agent and normal connection metadata to the service and its infrastructure. They do not send search text, conversion amounts, clipboard contents, app lists, camera frames, or Windows country settings. The source also contains a currency-pair request helper, but the current launcher uses the full daily snapshot. See [Frankfurter's API information](https://frankfurter.dev/) and [data terms](https://frankfurter.dev/license/).

Country and default currency come from Windows region settings, without GPS or IP geolocation. Unit conversion, time conversion, arithmetic, app search and emoji search run locally. Developer bootstrap downloads Zig from `ziglang.org`; that is a build-time operation, not normal app behavior.

**Check for Updates** explicitly opens the configured GitHub Releases page in your default browser. GitHub receives the normal browser request; Minicast does not send a profile or app data, check versions in the background, or download updates itself.

## Stored files

Normal data is stored in `%LOCALAPPDATA%\Minicast`. If Windows cannot provide that location, the app falls back to a `Minicast` folder in the Windows temporary directory. `--data-dir <path>` explicitly selects another profile directory.

| File | Contents | Protection |
| --- | --- | --- |
| `settings.txt` | Shortcuts, clipboard preference/retention, close behavior, startup setup marker and window position | Plain text; standard filesystem permissions |
| `apps.cache` | Indexed application names and local launch targets, which can include usernames in paths | Plain text; standard filesystem permissions |
| `usage.cache` | App launch counts and emoji usage counts for local ranking | Plain text; standard filesystem permissions |
| `rates.cache` | Exchange-rate values, dates and successful fetch day | Plain text; standard filesystem permissions |
| `clipboard.bin` | Captured text, bitmap images and timestamps | Windows DPAPI encryption for the current Windows user |
| `smoke-report.txt` | Local memory, handle, app-count and readiness measurements when the developer `--smoke` option is explicitly used | Plain text; standard filesystem permissions |

Temporary `.tmp` siblings may exist during atomic writes. App icons are cached in bounded memory; Minicast does not publish or upload them. Search queries are not stored as a query history, though text deliberately copied through the system clipboard may be captured when history is enabled.

## Clipboard history

History starts disabled. When enabled, Minicast listens for clipboard changes and captures text, PNG and Windows bitmap images. It skips entries marked `ExcludeClipboardContentFromMonitor` or explicitly disallowed by `CanIncludeInClipboardHistory` when those markers are supplied. This does not guarantee that password managers or other apps mark every sensitive item.

Retention is configurable to **7, 14, or 30 days**. The collection is additionally capped at 500 entries and approximately 128 MiB of text and bitmap image data, with a 64 KiB text capture-size limit and a 64 MiB image limit. Expiry is applied when history is loaded, as entries are captured, when retention changes, and by a timer while the loaded history is in memory. When Minicast is closed or history has not been loaded, it cannot continuously remove expired bytes from disk.

Pausing capture stops new entries; it does not erase existing history. **Clear history** removes entries from the active collection and queues an encrypted empty history for saving. File write failures are reported. The system clipboard, screenshots, external backups and filesystem recovery copies are not erased by this action.

DPAPI protects the stored file against casual reading outside the current Windows user context. It does not protect against all software running as that user, a compromised session, memory inspection or an administrator with suitable access. Use a dedicated password manager for secrets.

## Camera

The camera opens only when its command is invoked, subject to Windows camera permissions. Frames stay in memory for the mirror. The camera stops when the launcher hides, the session locks, the machine suspends or the app exits. A snapshot is saved only through an explicit save action and file dialog, or copied to the system clipboard through an explicit copy action. Minicast does not upload frames. Other apps and Windows clipboard settings can affect what happens after a snapshot is copied.

## Startup and removal

Normal first launch enables startup through the current user's `Software\Microsoft\Windows\CurrentVersion\Run` registry key, using the value `Minicast`. The command contains the executable path and `--background`. Settings can disable startup. An explicit `--data-dir` profile skips automatic registration, but manually changing Start with Windows still changes that shared registry value.

To remove local data, disable startup in Settings, exit Minicast from the tray menu, then remove its profile directory and executable. Deleting the profile removes preferences, ranking, indexes, rate cache and stored clipboard history. Files saved through the camera dialog remain wherever you chose to save them. Windows backups, security software, filesystem recovery and other apps may retain their own copies.

Do not attach a complete profile directory to bug reports. Use synthetic data and redact paths, app names, clipboard entries and unrelated desktop content from diagnostics.
