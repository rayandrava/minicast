# Security

Minicast is pre-release software. Security fixes target the latest source revision and newest published preview; older preview builds are not maintained separately. There is no promised response or patch timeline.

## Reporting a vulnerability

Use [GitHub's private vulnerability reporting](https://github.com/rayandrava/minicast/security/advisories/new), available through **Security → Report a vulnerability**. Private reporting is enabled for this repository.

If no private reporting action is available, open an issue asking the maintainer to establish a private contact channel. Include only a general description of the affected feature; do not post exploit details, credentials, clipboard data or private files publicly.

A private report should include the affected version or commit, Windows version, impact, a minimal reproduction using synthetic data, and any proposed fix. Avoid testing on other people's systems or accounts.

## Scope and limitations

Useful reports include unsafe clipboard handling, unintended camera access, untrusted data causing code execution, unsafe launch-target handling, or downloaded/compiler artifacts bypassing integrity checks.

Clipboard history is encrypted at rest with Windows DPAPI for the current user. It is not a password vault: text is present in memory when used, and software running as the same Windows user can potentially read it. Minicast honors supported clipboard exclusion markers, but cannot identify every sensitive item. See [privacy and storage details](docs/PRIVACY.md).

Minicast does not install an elevation service, execute downloaded extensions, or provide an automatic updater. It launches local apps through Windows Shell APIs and requests currency data over HTTPS. Operating-system, driver and application vulnerabilities should also be reported to their respective vendors.
