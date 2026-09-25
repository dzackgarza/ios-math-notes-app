# AGENTS.md

Read README.md (pipeline, layout) and TRAPS.md (known failures) first.

Invariants:
- The IPA's `CFBundleShortVersionString`, `CFBundleVersion`, `CFBundleIdentifier`, and byte size must equal the `source.json` entry. The workflow checks all of these before and after publishing; keep those checks when editing it.
- Each release tag is immutable and never deleted; `source.json` download URLs point at the per-build tag.
- Bundle ID `dev.zack.mathnotes` is fixed. Changing it makes SideStore treat the app as new and uses a free-account App ID slot.
- CI holds no Apple credentials and does not sign. SideStore signs on the device.
- Swift cannot build on the Linux host; verify Swift changes through a CI run (`gh run watch`).
