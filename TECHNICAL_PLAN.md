# Technical design

Minicast is a Windows x64 C++20 application using Win32 for lifecycle/input, Direct2D/DirectWrite for rendering, DirectComposition for the translucent surface, and Media Foundation for camera capture. The initial public target is an alpha release, not full Raycast feature parity.

## UI and input

The palette is 750 × 475 DIPs. Windows app-theme notifications select the tint; documented Desktop Acrylic supplies the backdrop on supported systems. Native rounded corners and shadow are requested without a painted outer border. GPU surfaces are released after the palette has been hidden for one second.

System RichEdit text services draw the transparent search field, retaining native text selection, IME and accessibility support. HWND metrics are physical pixels, font sizes are twips and ordinary Direct2D coordinates are DIPs. The editor and drawing target synchronize their DPI explicitly. An opaque native EDIT is the fallback when text services cannot initialize.

Settings use native controls and a bounded pair of reusable hotkey-recorder/clear controls. Global shortcuts use RegisterHotKey rather than a hook. Replacement bindings are registered and saved before the previous binding is retired. Unsupported or occupied chords leave the previous binding intact.

## Work and storage

- App discovery runs on a worker. Search ranks pre-normalized names, constructs at most 100 app rows and promotes five successfully launched apps when the query is empty.
- Icon extraction is lazy and off the UI thread. CPU and GPU caches each cap at 64 images; queued work is bounded. Bare app IDs resolve inside AppsFolder without changing stable keys used by shortcuts or ranking.
- Arithmetic, units and currency calculations run locally. Daily Frankfurter rates refresh on a bounded-timeout HTTPS worker; the UI uses the cached snapshot immediately. Local currency comes from Windows geography, not network geolocation.
- Clipboard capture is opt-in and event-driven. Data is bounded, retained for 7/14/30 days and encrypted using user-scoped DPAPI. A worker persists snapshots atomically. Failed writes retain the latest pending snapshot; destructive UI actions confirm persistence only after acknowledgement.
- Camera capture uses an asynchronous Media Foundation source reader and only starts on command. Frames are shared with rendering; snapshots explicitly copy them. No audio stream is initialized.
- Test profiles use separate instance identities and storage. The normal identity remains stable between builds so opening a second normal copy activates the existing one.

## Distribution and updates

VERSION is the single release version input. Build-generated metadata embeds it in the executable and Settings. The pinned compiler and vendor assets are development inputs; the distributable contains the executable and required notices.

GitHub Actions checks changes on Windows. A version tag produces a portable ZIP, checksums and a draft GitHub Release after tests pass. Publishing remains a maintainer action. The release workflow configures the app's repository URL from the actual GitHub repository; local builds can omit it.

The alpha's update action opens the repository's Releases page in the user's browser. There is no network version polling, automatic download or self-replacement. A future updater must cover trusted metadata, package/signature verification, an explicit install decision, exiting the old process, same-volume atomic replacement, failed-update recovery, data migrations and rollback. An updater must not run as administrator or add a resident service merely to replace a per-user portable app. See [release operations](docs/RELEASING.md).

## Source map

| Area | Files |
| --- | --- |
| Lifecycle, queries, hotkeys, storage coordination | `src/main.cpp`, `src/results.inl` |
| Rendering and actions | `src/palette.inl`, `src/palette_actions.inl`, `src/surface.cpp` |
| Settings and hotkey capture | `src/settings.inl`, `src/shortcut_capture.hpp` |
| Input and DPI | `src/search_input.cpp`, `src/search_metrics.hpp` |
| Application icons and namespace lookup | `src/app_icons.cpp`, `src/app_shell.hpp` |
| Branding | `src/brand_icons.cpp`, `src/minicast.rc`, `assets/brand/` |
| Core conversions, parsing and emoji | `src/core.cpp`, `src/calculator.cpp` |
| Windows integration and persistence | `src/platform.cpp`, `src/startup.cpp`, `src/local_currency.cpp` |
| Camera | `src/camera.cpp` |
| Limits and invariants | Shared headers and console tests in `tests/` |

## Validation policy

Use console regression checks for parsers, data boundaries and pure state transitions. Use isolated profiles for integration tests, never real clipboard history as a fixture. UI checks cover native keyboard interaction, selection, focus, scaling, appearance and errors that console tests cannot establish. Do not claim latency or memory targets based on a single sample, or claim untested Windows versions and devices are supported.

Current results and remaining checks are in [VALIDATION.md](docs/VALIDATION.md).
