# ios-math-notes-app

Personal iPad app, developed on Linux.

- `project.yml` defines the Xcode project ([XcodeGen](https://github.com/yonaskolb/XcodeGen)). The `.xcodeproj` is generated in CI and not tracked.
- GitHub Actions (`macos-26` runner) builds an **unsigned** IPA on each push to `main` and uploads it as the `MathNotes-unsigned` artifact.
- [SideStore](https://docs.sidestore.io/) on the iPad signs and installs the IPA with a free Apple Account and refreshes the 7-day signature on-device.

## Install on the iPad

1. One-time: install SideStore from Linux with `iloader` (see SideStore docs); enable Developer Mode on the iPad.
2. Download the latest IPA: `gh run download -n MathNotes-unsigned`.
3. Move `MathNotes.ipa` to the iPad and open it in SideStore.
