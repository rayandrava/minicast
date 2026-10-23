# Contributing to Minicast

Minicast is a native Windows launcher. Changes should preserve fast startup, low idle CPU use, bounded memory, keyboard accessibility, and clear system-themed UI.

## Build and check

Use Windows x64 and PowerShell. The compiler is pinned; no globally installed C++ toolchain is required.

```powershell
.\scripts\bootstrap.ps1
.\build.ps1 -Test -Output 'dist\dev\Minicast.exe'
```

Bootstrap downloads the compiler and verifies its checksum. The build runs console tests, generates icon resources, and checks the executable without launching the UI. Use a separate output directory when another Minicast executable may be running.

For an intentional manual test, run the test build with an isolated profile. It can coexist with your normal Minicast instance:

```powershell
.\dist\dev\Minicast.exe --data-dir '.\build\manual-profile'
```

A fresh isolated profile has its own preferences and caches, starts with no global shortcuts, and skips automatic startup registration. Reusing that profile restores its saved preferences and any shortcuts you assigned. Its shortcuts still share the Windows hotkey namespace, so avoid combinations already used by the normal instance.

Do not toggle **Start with Windows** during isolated tests: that control still changes the normal current-user Windows startup registration. Clipboard capture is opt-in; use synthetic text and deliberate camera access. The app still fetches its daily currency snapshot. Do not automate desktop interaction on another person's active desktop without their permission.

## Make a focused change

- Explain the user-visible problem, resulting behavior and validation in the pull request.
- Keep Windows calls, disk work and network work out of latency-sensitive rendering and search paths where practical. Bound caches and release camera/GPU resources when unused.
- Add meaningful regression checks for logic changes. For visual changes, record what was inspected, including theme and display scale; distinguish measured results from assumptions.
- Preserve IME composition, keyboard navigation, shortcut conflict handling, existing profiles and offline behavior.
- Avoid new runtime dependencies without a concrete benefit and an explanation of size, startup and memory costs.

`src/lucide_data.inc` is generated from the vendored SVGs with `scripts/generate-icons.ps1`. App and tray ICOs are generated from the selected SVG with `scripts/generate-brand-icons.ps1`; change the source geometry, not individual raster frames. Keep the vendor notices intact.

## Reports and contributions

Use the bug or feature template once this repository is hosted. Include the Minicast version, Windows version, display scale and a minimal reproduction. Use synthetic inputs; redact usernames, app lists, clipboard data and screenshots before sharing. See [SECURITY.md](SECURITY.md) for vulnerabilities and [the privacy notes](docs/PRIVACY.md) for local data locations.

Submit only material you have the right to contribute. Contributions to Minicast's own code and documentation are provided under [MIT](LICENSE). Identify the source, version and license of imported material in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Do not copy proprietary source, logos, screenshots, or third-party application icons into the project without appropriate rights.

AI-assisted contributions are welcome when reviewed by the contributor. You remain responsible for correctness, tests, provenance and licensing. Keep private prompts, machine paths, credentials, clipboard history and generated build directories out of commits.
