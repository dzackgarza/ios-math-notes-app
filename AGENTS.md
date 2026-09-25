# AGENTS.md

Read README.md (pipeline, layout) and TRAPS.md (known failures) first.

Work plan: the GitHub issue tree rooted at #11, one milestone per sub-issue. `uvx --from git+https://github.com/dzackgarza/itree itree next dzackgarza/math-notes-app` names the next work unit in tree order. Each work unit lists the reference code to port before writing any; docs/ARCHITECTURE.md indexes the references and dependencies.

Invariants:
- The IPA's `CFBundleShortVersionString`, `CFBundleVersion`, `CFBundleIdentifier`, and byte size must equal the `source.json` entry. The workflow checks all of these before and after publishing; keep those checks when editing it.
- Each release tag is immutable and never deleted; `source.json` download URLs point at the per-build tag.
- Bundle ID `dev.zack.mathnotes` is fixed. Changing it makes SideStore treat the app as new and uses a free-account App ID slot.
- CI holds no Apple credentials and does not sign. SideStore signs on the device.
- Swift cannot build on the Linux host; verify Swift changes through a CI run (`gh run watch`).
