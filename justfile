# Swift builds only on the macOS CI runner; locally, validate the YAML specs.
test-commit:
    uvx yamllint -s -d '{extends: relaxed, rules: {line-length: disable}}' project.yml .github/workflows/ios.yml

test-push: test-commit

# Stylus Labs Write fork with the replay harness (dzackgarza/Write, branch replay-harness).
write_dir := env_var_or_default("WRITE_DIR", env_var("HOME") / ".cache/math-notes/Write")
write_rev := "876de97"

# Build the Write fork and regenerate every case in tests/fixtures/write (see its README.md).
write-fixtures:
    #!/usr/bin/env bash
    set -euo pipefail
    W="{{write_dir}}"
    F="{{justfile_directory()}}/tests/fixtures/write"
    if [ ! -d "$W" ]; then
      git clone --recurse-submodules -b replay-harness https://github.com/dzackgarza/Write "$W"
    fi
    if [ "$(git -C "$W" rev-parse --short=7 HEAD)" != "{{write_rev}}" ]; then
      echo "$W is not at write_rev {{write_rev}}; check it out or update write_rev" >&2
      exit 1
    fi
    make -C "$W/syncscribble" USE_SYSTEM_SDL=1 DEBUG=1 SANITIZE=0 -j"$(nproc)" >/dev/null
    cp "$W"/scribbleres/fonts/* "$W/syncscribble/Debug/"
    tmp=$(mktemp -d)
    trap 'trash "$tmp"' EXIT
    run() { env -u WAYLAND_DISPLAY SDL_VIDEODRIVER=x11 xvfb-run -a -s "-screen 0 1280x1024x24" "$W/syncscribble/Debug/Write" "$@"; }
    # convert the upstream tests: runAll records each test as a trace (and must still pass)
    mkdir "$tmp/rec" "$tmp/replay"
    out=$(cd "$W/syncscribble" && WRITE_RECORD_DIR="$tmp/rec" run --test 2>&1) || true
    # runAll leaves *_out.html and thumbnail PNGs next to the refs when thumbnails differ
    trash "$W"/scribbletest/test*_out.html "$W"/scribbletest/test*_{out,ref,diff}.png 2>/dev/null || true
    grep -q "with 0 failed tests" <<<"$out" || { echo "$out" >&2; exit 1; }
    for trace in "$tmp"/rec/test*.trace; do
      n=$(basename "$trace" .trace)
      mkdir -p "$F/upstream-$n"
      cp "$trace" "$F/upstream-$n/trace.txt"
      if [ -f "$W/scribbletest/${n}_in.html" ]; then cp "$W/scribbletest/${n}_in.html" "$F/upstream-$n/input.html"; fi
    done
    # replay every case; upstream-test<N> cases are also compared with Write's test<N>_ref.html
    (cd "$W/syncscribble" && WRITE_REPLAY_DIR="$F" WRITE_REPLAY_TMP="$tmp/replay" run --replaytest)
