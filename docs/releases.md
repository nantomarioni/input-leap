# Fork releases & updates

Knowledge file (load on demand).

`.github/workflows/release.yml` builds macOS **arm64 only** (no Intel — the
fleet is Windows host + Apple Silicon satellites) and a Windows Inno installer.
Two channels:

- **Dev (default):** every push to `fork` rebuilds and force-updates the
  rolling `latest-build` prerelease with fresh installers. Versioning is
  commit-based (`INPUTLEAP_VERSION_DESC=git` → `3.0.3-git-<date>-<hash>`), so
  the version string in the app/dmg identifies the exact commit.
- **Stable (optional):** pushing a `fork-v*` tag creates a permanent Release —
  used only to bless known-good builds.

The macOS bundle is **codesigned with a stable self-signed identity**
("InputLeap Fork", secrets `MACOS_CERT_P12` / `MACOS_CERT_P12_PASSWORD`) so TCC
permissions (Accessibility / Input Monitoring) survive updates; the dmg is
repackaged from the signed app in CI. If the secrets are missing the pipeline
warns and ships unsigned. Cert material lives outside the repo
(`~/.inputleap-fork-signing/` on the authoring machine) — never commit it.
