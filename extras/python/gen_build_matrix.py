#!/usr/bin/env python3
"""
Single source of truth for "which board is which" (BOARD_INFO/BOARD_ALIASES,
see iotsa-board-traits.ini/#222) and for "which examples/tests/sandbox entries build
with which board/flags" (every examples/*/iotsa-build.json, tests/*/iotsa-build.json,
and sandbox/*/iotsa-build.json). Turns these into:
  - the layer-1/layer-2 board.ini sections (--format=board-defs-ini)
  - the toplevel platformio.ini env list (--format=platformio-ini)
  - a GitHub Actions matrix, per CI backend (--format=github-matrix --backend=...)
  - a standalone platformio.ini for a single entry (--format=standalone-ini --dir=...)

See docs/module-interface-status.md and the #156/#222 discussions for background.
"""
import argparse
import io
import json
import os
import sys

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))

# Layer 3 processor/USB-wiring traits (see iotsa-board-traits.ini and #222) -> the
# build_flags that trait contributes. iotsa-board-traits.ini is hand-authored (rare
# changes, real pio-specific lib_deps/lib_ignore syntax not worth generalizing
# here) so this is the one place its *build_flags* are duplicated in Python --
# needed because emit_github_matrix/emit_standalone_ini don't go through any
# ini extends chain, so they can't pick a trait's flags up by inheritance the
# way a local `pio run` build does. Keep in sync with iotsa-board-traits.ini by hand.
TRAIT_BUILD_FLAGS = {
    "_esp8266": [],
    "_esp32_allvariants": [],
    "_esp32c3_extusb": ["-DESP32C3"],
    "_esp32c3_nativeusb": ["-DESP32C3", "-DARDUINO_USB_MODE=1", "-DARDUINO_USB_CDC_ON_BOOT=1",
                            "-DIOTSA_SERIAL_SPEED=460800", "-DIOTSA_DELAY_ON_BOOT=3"],
    "_esp32s3_nativeusb": ["-DARDUINO_USB_MODE=1", "-DARDUINO_USB_CDC_ON_BOOT=1",
                            "-DIOTSA_SERIAL_SPEED=460800", "-DIOTSA_DELAY_ON_BOOT=3"],
}

# Layer 2 board names (see iotsa-board-defs.ini and #222) -> the actual PlatformIO
# board ID, the arduino-cli FQBN where supported, which layer-3 trait it
# extends, which processor family (drives emit_standalone_ini's "platform ="
# line), and the hardware facts unique to this board (not already covered by
# its trait): build_flags (e.g. a neopixel pin), partitions/mcu/flash_size.
# build_flags here is *board-specific only* -- emit_board_defs_ini pulls its
# trait's flags in via ini extends/interpolation, while emit_github_matrix and
# emit_standalone_ini (no ini inheritance available to them) combine
# TRAIT_BUILD_FLAGS[trait] + this board's own build_flags themselves.
BOARD_INFO = {
    "iotsa_v4":         {"pio_board": "nodemcuv2",        "fqbn": "esp8266:esp8266:nodemcuv2", "family": "esp8266", "trait": "_esp8266",
                          "build_flags": []},
    "esp32thing":       {"pio_board": "esp32thing",       "fqbn": "esp32:esp32:esp32thing",    "family": "esp32", "trait": "_esp32_allvariants",
                          "build_flags": []},
    "esp32dev":         {"pio_board": "esp32dev",         "fqbn": None,                        "family": "esp32", "trait": "_esp32_allvariants",
                          "build_flags": []},
    "lolin32":          {"pio_board": "lolin32",          "fqbn": None,                        "family": "esp32", "trait": "_esp32_allvariants",
                          "build_flags": [], "partitions": "min_spiffs.csv"},
    "pico32":           {"pio_board": "pico32",           "fqbn": None,                        "family": "esp32", "trait": "_esp32_allvariants",
                          "build_flags": []},
    "esp32c3devkit":    {"pio_board": "esp32-c3-devkitm-1", "fqbn": None,                      "family": "esp32", "trait": "_esp32c3_extusb",
                          "build_flags": ["-DIOTSA_PIN_NEOPIXEL=8"]},
    "esp32c3lcd":       {"pio_board": "esp32-c3-devkitm-1", "fqbn": None,                      "family": "esp32", "trait": "_esp32c3_nativeusb",
                          "build_flags": [], "mcu": "esp32c3", "partitions": "bare_minimum_2MB.csv", "flash_size": "2MB"},
    "esp32c3supermini": {"pio_board": "esp32-c3-devkitm-1", "fqbn": None,                      "family": "esp32", "trait": "_esp32c3_nativeusb",
                          "build_flags": [], "mcu": "esp32c3", "partitions": "min_spiffs.csv", "flash_size": "4MB"},
    "crowpanel128":     {"pio_board": "esp32-c3-devkitm-1", "fqbn": None,                      "family": "esp32", "trait": "_esp32c3_extusb",
                          "build_flags": []},
    "esp32s3supermini": {"pio_board": "esp32-s3-devkitc-1", "fqbn": "esp32:esp32:esp32s3",      "family": "esp32", "trait": "_esp32s3_nativeusb",
                          "build_flags": ["-DIOTSA_PIN_NEOPIXEL=48"], "mcu": "esp32s3", "partitions": "min_spiffs.csv", "flash_size": "4MB"},
}

# Layer 1 role aliases (see iotsa-board-defs.ini and #222): pure synonyms for
# "whichever layer-2 board is our current default for this chip family".
# Resolved once, right after an iotsa-build.json variant is read, so every
# emitter below only ever sees a real BOARD_INFO key.
BOARD_ALIASES = {
    "vanilla_esp8266": "iotsa_v4",
    "vanilla_esp32":   "esp32thing",
    "vanilla_esp32c3": "esp32c3devkit",
    "vanilla_esp32s3": "esp32s3supermini",
}


def resolved_build_flags(board, extra_flags=()):
    """Full build_flags for `board`: its trait's flags, then its own, then
    `extra_flags` (typically a variant's own list). Used by emitters that
    have no ini extends chain to inherit the trait's flags through
    (emit_github_matrix, emit_standalone_ini) -- emit_platformio_ini instead
    lets `${board.build_flags}` interpolation pick them up via inheritance."""
    info = BOARD_INFO[board]
    return list(TRAIT_BUILD_FLAGS.get(info["trait"], [])) + list(info["build_flags"]) + list(extra_flags)


def merge_patch(default, override):
    """RFC 7396 JSON Merge Patch: override replaces default per top-level key;
    a null value in override deletes that key."""
    result = dict(default or {})
    for k, v in (override or {}).items():
        if v is None:
            result.pop(k, None)
        else:
            result[k] = v
    return result


def load_entries():
    """Returns a list of dicts, one per (directory, variant), fully merged.

    Order matters here beyond readability: this is also the order jobs land in
    the generated GitHub Actions matrix, and GitHub schedules matrix jobs
    roughly front-to-back against the runner pool. tests/ (KitchenSink and
    friends -- broad, deliberately-stressed coverage) goes first so a basic,
    widespread breakage surfaces as an early red job instead of waiting behind
    the much longer examples/ tail; sandbox/ (still churning, #222) next;
    examples/ (doc-grade, most numerous, least likely to catch something novel)
    last.
    """
    entries = []
    for kind, base in (("test", "tests"), ("sandbox", "sandbox"), ("example", "examples")):
        base_dir = os.path.join(REPO_ROOT, base)
        if not os.path.isdir(base_dir):
            continue
        for name in sorted(os.listdir(base_dir)):
            build_json = os.path.join(base_dir, name, "iotsa-build.json")
            if not os.path.isfile(build_json):
                continue
            with open(build_json) as f:
                spec = json.load(f)
            source = spec.get("source", f"{base}/{name}")
            defaults = spec.get("defaults", {})
            for variant in spec["variants"]:
                merged = merge_patch(defaults, variant)
                if merged.get("board") in BOARD_ALIASES:
                    merged["board"] = BOARD_ALIASES[merged["board"]]
                merged["kind"] = kind
                merged["name"] = name
                merged["source"] = source
                # tests/ exists specifically to hold minimal-complete-coverage variants
                # (see #216) -- every test entry counts by default. sandbox/ (#222)
                # defaults the same way: it's under active development, so it wants
                # fast feedback on every develop push, not just full-tier runs. An
                # example only counts if it says so explicitly (it covers a
                # module/combination no test/sandbox entry does either).
                merged.setdefault("minimal", kind in ("test", "sandbox"))
                merged.setdefault("ci", {"platformio": True, "arduino": False})
                merged.setdefault("build_flags", [])
                merged.setdefault("lib_deps", [])
                entries.append(merged)
    return entries


def env_name(e):
    # name-kind-label-board: name+kind+label together fully identify *what* is
    # being built (the program, which category, which flavor of it); board is
    # the one axis that's purely "which target to compile that same identity
    # for", so it trails as a qualifier -- same program+kind+label's board
    # variants then sort next to each other, sharing everything but the last
    # segment. See #222.
    parts = [e["name"], e["kind"]]
    if e.get("label"):
        parts.append(e["label"])
    parts.append(e["board"])
    return "-".join(parts)


def emit_board_defs_ini(out):
    """Layer 1 (vanilla_* aliases) and layer 2 (boards we use) sections,
    extending the hand-authored layer-3 traits in iotsa-board-traits.ini. See
    platformio.ini's extra_configs and #222."""
    out.write("; Generated by extras/python/gen_build_matrix.py from BOARD_INFO/BOARD_ALIASES --\n")
    out.write("; do not hand-edit, regenerate instead:\n")
    out.write(";   python3 extras/python/gen_build_matrix.py --format=board-defs-ini -o iotsa-board-defs.ini\n")
    out.write(";\n; Layer 2 -- boards we actually use. See iotsa-board-traits.ini for layer 3.\n\n")
    for board, info in BOARD_INFO.items():
        out.write(f"[{board}]\n")
        out.write(f"extends = {info['trait']}\n")
        out.write(f"board = {info['pio_board']}\n")
        if info.get("mcu"):
            out.write(f"board_build.mcu = {info['mcu']}\n")
        if info.get("partitions"):
            out.write(f"board_build.partitions = {info['partitions']}\n")
        if info.get("flash_size"):
            out.write(f"board_upload.flash_size = {info['flash_size']}\n")
        if info["build_flags"]:
            # ${trait.build_flags}, not a bare list: a plain `build_flags = ...`
            # here would *replace* the inherited trait value instead of adding
            # to it -- PlatformIO's extends does not auto-concatenate.
            out.write(f"build_flags = ${{{info['trait']}.build_flags}} {' '.join(info['build_flags'])}\n")
        out.write("\n")
    out.write("; Layer 1 -- vanilla_* role aliases, pure synonyms.\n\n")
    for alias, target in BOARD_ALIASES.items():
        out.write(f"[{alias}]\n")
        out.write(f"extends = {target}\n\n")


def emit_platformio_ini(entries, out):
    out.write("; Generated by extras/python/gen_build_matrix.py from examples/*/iotsa-build.json,\n")
    out.write("; tests/*/iotsa-build.json, and sandbox/*/iotsa-build.json -- do not hand-edit,\n")
    out.write("; regenerate instead:\n")
    out.write(";   python3 extras/python/gen_build_matrix.py --format=platformio-ini -o generated_envs.ini\n\n")
    skips = []
    for e in entries:
        if not e["ci"].get("platformio", True):
            continue
        board = e["board"]
        info = BOARD_INFO.get(board)
        if info is None:
            print(f"warning: unknown board {board!r}, skipping env for {env_name(e)}",
                  file=sys.stderr)
            continue
        out.write(f"[env:{env_name(e)}]\n")
        out.write(f"extends = {board}\n")
        out.write(f"build_src_filter = +<*> +<../{e['source']}>\n")
        if e["build_flags"]:
            out.write(f"build_flags = ${{{board}.build_flags}} {' '.join(e['build_flags'])}\n")
        if e.get("partitions"):
            out.write(f"board_build.partitions = {e['partitions']}\n")
        if e["lib_deps"]:
            out.write(f"lib_deps = \n")
            out.write(f"    ${{{board}.lib_deps}}\n")
            for dep in e["lib_deps"]:
                out.write(f"    {dep}\n")
        out.write("\n")
        skip_reason = (e.get("skip") or {}).get("platformio")
        if skip_reason:
            skips.append((env_name(e), "platformio", skip_reason))
    for name, backend, reason in skips:
        print(f"note: {name} generated for local dev, but CI skips it "
              f"({backend}): {reason}", file=sys.stderr)


def emit_github_matrix(entries, backend, tier, out):
    matrix = []
    skips = []
    for e in entries:
        if not e["ci"].get(backend, False):
            continue
        if tier == "minimal" and not e["minimal"]:
            continue
        skip_reason = (e.get("skip") or {}).get(backend)
        if skip_reason:
            skips.append((env_name(e), skip_reason))
            continue
        board = e["board"]
        info = BOARD_INFO.get(board)
        if info is None:
            print(f"warning: unknown board {board!r}, skipping {env_name(e)}",
                  file=sys.stderr)
            continue
        # pio ci (platformio backend) and arduino-cli both build outside any ini
        # extends chain, so unlike a local `pio run` they can't pick up a
        # board's/trait's facts by inheritance -- resolve them explicitly here.
        # A variant's own "partitions" wins if set, else the board's default.
        partitions = e.get("partitions") or info.get("partitions") or ""
        item = {
            "name": env_name(e),
            "kind": e["kind"],
            "example": e["name"],
            "source": e["source"],
            "board": board,
            "build_flags": " ".join(resolved_build_flags(board, e["build_flags"])),
            # newline-joined, not a JSON list: GitHub Actions matrix values are consumed
            # as plain strings in the job's env block, so lists are awkward there. A
            # newline-joined string plugs directly into a multi-line platformio
            # --project-option="lib_deps=..." value, the same list syntax platformio.ini
            # itself uses.
            "extra_lib_deps": "\n".join(e["lib_deps"]),
            "partitions": partitions,
        }
        if backend == "platformio":
            item["pio_board"] = info["pio_board"]
        elif backend == "arduino":
            if not info["fqbn"]:
                print(f"warning: board {board!r} has no arduino-cli FQBN, "
                      f"skipping {env_name(e)}", file=sys.stderr)
                continue
            item["fqbn"] = info["fqbn"]
            if partitions == "min_spiffs.csv":
                item["partition_scheme"] = "min_spiffs"
        matrix.append(item)
    for name, reason in skips:
        print(f"note: skipping {name} for {backend}: {reason}", file=sys.stderr)
    # Compact, single-line: this is meant to be captured directly into a
    # $GITHUB_OUTPUT value, where a multi-line value needs extra heredoc ceremony.
    json.dump({"include": matrix}, out, separators=(",", ":"))
    out.write("\n")


def iotsa_lib_deps(target_dir):
    """Where should this standalone example's iotsa dependency come from?

    <target_dir>/iotsa-config.json can set a toplevel "iotsa" field to an
    explicit lib_deps-style value (a git URL#ref, to pin against a specific
    published branch/tag). Absent that, we default to a symlink at the local
    iotsa checkout this example lives in -- anyone who has this file at all
    has necessarily cloned the enclosing repo, so the relative path is always
    valid, and it means local framework edits are picked up without needing
    to push first.
    """
    config_path = os.path.join(target_dir, "iotsa-config.json")
    if os.path.isfile(config_path):
        with open(config_path) as f:
            config = json.load(f)
        if config.get("iotsa"):
            return config["iotsa"]
    return f"symlink://{os.path.relpath(REPO_ROOT, target_dir)}"


def emit_standalone_ini(entries, target_dir, out):
    rel = os.path.relpath(target_dir, REPO_ROOT)
    # Kind-agnostic (source, not a hardcoded "examples/" prefix) so this also works
    # for a self-contained sandbox/ entry (#222), not just examples/.
    own = [e for e in entries if e["source"] == rel and e["ci"].get("platformio", True)]
    if not own:
        print(f"note: {rel} has no platformio-enabled variants, no standalone "
              f"platformio.ini to generate", file=sys.stderr)
        return
    # entries is ordered test/sandbox/example (see load_entries -- deliberate for
    # CI scheduling, #222), but that's the wrong default *here*: a standalone
    # example's own plain variant, not some test-only flag combination that
    # happens to share its source, is what "just run pio run" should build.
    # Stable sort: only promotes kind=="example" ahead, doesn't otherwise
    # reorder.
    own = sorted(own, key=lambda e: e["kind"] != "example")
    out.write("; PlatformIO Project Configuration File\n")
    out.write("; Generated by extras/python/gen_build_matrix.py -- do not hand-edit,\n")
    out.write("; regenerate instead:\n")
    out.write(";   python3 extras/python/gen_build_matrix.py --format=standalone-ini \\\n")
    out.write(f";     --dir={rel} -o {rel}/platformio.ini\n")
    out.write("; To pin this example against a specific published iotsa branch/tag\n")
    out.write("; instead of the local checkout, add an iotsa-config.json here:\n")
    out.write(';   {"iotsa": "https://github.com/cwi-dis/iotsa.git#develop"}\n')
    out.write(";\n")
    # A board can appear more than once (e.g. a plain variant plus a differently
    # labeled one, as with sandbox/BLEClient's merged matrix, #222) -- suffix the
    # env name with the label whenever one is present, so two variants sharing a
    # board don't collide into the same [env:xxx] section.
    env_names = [e["board"] if not e.get("label") else f"{e['board']}-{e['label']}"
                 for e in own]
    out.write("[platformio]\n")
    out.write("src_dir = .\n")
    out.write(f"default_envs = {env_names[0]}\n\n")
    out.write("[common]\n")
    out.write("framework = arduino\n")
    out.write("lib_ldf_mode = deep+\n")
    out.write("lib_compat_mode = strict\n")
    out.write(f"lib_deps = {iotsa_lib_deps(target_dir)}\n")
    out.write("monitor_speed = 115200\n\n")
    for e, env in zip(own, env_names):
        board = e["board"]
        info = BOARD_INFO[board]
        out.write(f"[env:{env}]\n")
        out.write("extends = common\n")
        out.write(f"platform = {'espressif8266' if info['family'] == 'esp8266' else 'espressif32'}\n")
        out.write(f"board = {info['pio_board']}\n")
        # No ini extends chain to a board/trait section here (this file is
        # self-contained), so resolve the board's/trait's facts explicitly --
        # same reasoning as emit_github_matrix.
        if info.get("mcu"):
            out.write(f"board_build.mcu = {info['mcu']}\n")
        flags = resolved_build_flags(board, e["build_flags"])
        if flags:
            out.write(f"build_flags = {' '.join(flags)}\n")
        partitions = e.get("partitions") or info.get("partitions")
        if partitions:
            out.write(f"board_build.partitions = {partitions}\n")
        if info.get("flash_size"):
            out.write(f"board_upload.flash_size = {info['flash_size']}\n")
        if e["lib_deps"]:
            out.write("lib_deps = \n")
            out.write("    ${common.lib_deps}\n")
            for dep in e["lib_deps"]:
                out.write(f"    {dep}\n")
        out.write("\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--format", required=True,
                     choices=["board-defs-ini", "platformio-ini", "github-matrix", "standalone-ini"])
    ap.add_argument("--backend", choices=["platformio", "arduino"],
                     help="required for --format=github-matrix")
    ap.add_argument("--tier", choices=["full", "minimal"], default="full",
                     help="--format=github-matrix only: 'minimal' builds just the "
                          "tests/ tree plus examples explicitly marked minimal:true "
                          "(see #216); default 'full' builds everything")
    ap.add_argument("--dir", help="examples/<Name> dir, required for --format=standalone-ini")
    ap.add_argument("-o", "--output", help="write to file instead of stdout")
    args = ap.parse_args()

    entries = load_entries()

    if args.format == "standalone-ini":
        # Buffered: a directory with no platformio-enabled variants emits nothing,
        # and any stale output file (e.g. from before a variant dropped platformio
        # coverage) must be removed rather than left behind as an empty/stale file.
        if not args.dir:
            ap.error("--format=standalone-ini requires --dir")
        buf = io.StringIO()
        emit_standalone_ini(entries, os.path.join(REPO_ROOT, args.dir), buf)
        content = buf.getvalue()
        if not content:
            if args.output and os.path.exists(args.output):
                os.remove(args.output)
            return
        if args.output:
            with open(args.output, "w") as out:
                out.write(content)
        else:
            sys.stdout.write(content)
        return

    out = open(args.output, "w") if args.output else sys.stdout
    try:
        if args.format == "board-defs-ini":
            emit_board_defs_ini(out)
        elif args.format == "platformio-ini":
            emit_platformio_ini(entries, out)
        elif args.format == "github-matrix":
            if not args.backend:
                ap.error("--format=github-matrix requires --backend")
            emit_github_matrix(entries, args.backend, args.tier, out)
    finally:
        if args.output:
            out.close()


if __name__ == "__main__":
    main()
