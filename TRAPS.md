# Traps

## Build / release

- **XcodeGen writes fixed `1.0` / `1` into Info.plist** when `info.properties` is used, ignoring `MARKETING_VERSION` / `CURRENT_PROJECT_VERSION`. `project.yml` must map `CFBundleShortVersionString: $(MARKETING_VERSION)` and `CFBundleVersion: $(CURRENT_PROJECT_VERSION)`. Symptom: SideStore "does not match the build number specified by the source".
- **Never publish the IPA at a reused URL.** A rolling `latest` tag let the iPad install a cached older IPA (build mismatch). Deleting a tag breaks every cached `source.json` that points at it ("MathNotes.ipa doesn't exist"). Each build gets its own tag `v<run>`; old releases are kept.
- **Size in `source.json` must be the exact IPA byte count.** SideStore verifies it. macOS `stat` is `stat -f %z`, not `-c %s`.
- **Workflow re-runs reuse `github.run_number`,** so the release step uploads with `--clobber` when the tag exists.

## Issue tree

- **The issue-dependencies API takes the blocker's database id, not its number.** `POST repos/{owner}/{repo}/issues/{n}/dependencies/blocked_by` with `issue_id=<number>` returns success and links whatever issue has that database id. Get the id with `gh api repos/{owner}/{repo}/issues/{n} --jq .id`, and pass it typed (`-F`).
- **`itree milestone` refuses a forest.** Its preflight rejects the whole command while parentless issues exist. Attach them under the root ledger first (`itree attach`).

## Linux host

- **iloader AppImage aborts with `Could not create surfaceless EGL display: EGL_BAD_ALLOC`** on this machine. The AppImage bundles an old libwayland (upstream nab138/iloader#576); `WEBKIT_DISABLE_DMABUF_RENDERER`, `GDK_BACKEND=x11` do not help. Use the `.deb` asset's `usr/bin/iloader` against system `webkit2gtk-4.1`.
- **`usbmuxd` started after the iPad was plugged in does not see it.** Re-plug. `Pairing dialog response pending (-19)` means tap Trust on the iPad.

## SideStore on the iPad

- **Developer Mode switch is hidden** until a development-signed app (SideStore) is on the device and launched once.
- **"VPN connection error / no utun interface"**: LocalDevVPN is not connected.
- **Operation error 28** = `notAuthenticated` (Swift's implicit `CustomNSError` code for `OperationError`, payload cases first). Sign in inside SideStore's Settings; signing in to iloader is not enough.
- **A changed source is cached.** After changing download URLs, remove and re-add the source.
