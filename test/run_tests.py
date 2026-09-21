#!/usr/bin/env python3
"""Host-side tests for the OLED_Device sketch. See test/README.md.

    python test/run_tests.py                  run the button test and the simulation
    python test/run_tests.py --baseline HEAD~1  also compare screens and metrics
                                                with another git revision
"""
import argparse
import concurrent.futures
import hashlib
import io
import os
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TEST = ROOT / "test"
SKETCH = ROOT / "OLED_Device"
BUILD = TEST / ".build"
EXE = ".exe" if os.name == "nt" else ""

U8G2_VERSION = "2.36.19"
U8G2_URL = f"https://github.com/olikraus/U8g2_Arduino/archive/refs/tags/{U8G2_VERSION}.zip"

# Font files are huge (tens of MB); only the fonts the sketch uses are built.
U8G2_SKIP = {"u8g2_fonts.c", "u8x8_fonts.c"}


def run(cmd, **kw):
    result = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if result.returncode != 0:
        sys.stderr.write(" ".join(map(str, cmd)) + "\n" + result.stdout + result.stderr)
        sys.exit(f"command failed: {cmd[0]}")
    return result


# ------------------------------------------------------------------ toolchain

def find_toolchain():
    """Return (cc, cxx, ar) command prefixes."""
    if os.environ.get("CC") and os.environ.get("CXX"):
        ar = shutil.which(os.environ.get("AR", "ar")) or shutil.which("llvm-ar")
        if ar:
            return shlex.split(os.environ["CC"]), shlex.split(os.environ["CXX"]), [ar]
    for cc, cxx in (("gcc", "g++"), ("clang", "clang++"), ("cc", "c++")):
        ar = shutil.which("ar") or shutil.which("llvm-ar")
        if shutil.which(cc) and shutil.which(cxx) and ar:
            return [cc], [cxx], [ar]
    try:
        import ziglang  # noqa: F401  (pip install ziglang)
        zig = [sys.executable, "-m", "ziglang"]
        return zig + ["cc"], zig + ["c++"], zig + ["ar"]
    except ImportError:
        pass
    sys.exit("No C/C++ compiler found. Install gcc or clang, or run: pip install ziglang")


# ------------------------------------------------------------------ U8g2

def find_u8g2(explicit):
    candidates = []
    if explicit:
        candidates.append(Path(explicit))
    if os.environ.get("U8G2_DIR"):
        candidates.append(Path(os.environ["U8G2_DIR"]))
    home = Path.home()
    candidates += [home / "Documents" / "Arduino" / "libraries" / "U8g2",
                   home / "Arduino" / "libraries" / "U8g2"]
    for c in candidates:
        for clib in (c / "src" / "clib", c / "clib", c):
            if (clib / "u8g2.h").is_file():
                return clib
    if explicit:
        sys.exit(f"u8g2.h not found under {explicit}")
    return download_u8g2()


def download_u8g2():
    dest = BUILD / f"U8g2_Arduino-{U8G2_VERSION}"
    clib = dest / "src" / "clib"
    if not (clib / "u8g2.h").is_file():
        print(f"downloading U8g2 {U8G2_VERSION} ...")
        data = urllib.request.urlopen(U8G2_URL).read()
        zipfile.ZipFile(io.BytesIO(data)).extractall(BUILD)
    return clib


def sketch_fonts(sketch_dirs):
    fonts = set()
    for d in sketch_dirs:
        for p in d.glob("*.ino"):
            fonts |= set(re.findall(r"\bu8g2_font_\w+", p.read_text(encoding="utf-8", errors="replace")))
    return sorted(fonts)


def build_u8g2(clib, cc, ar, fonts):
    out = BUILD / ("u8g2-" + hashlib.sha1(str(clib.resolve()).encode()).hexdigest()[:10])
    out.mkdir(parents=True, exist_ok=True)

    lib = out / "libu8g2.a"
    if not lib.exists():
        sources = [p for p in sorted(clib.glob("*.c")) if p.name not in U8G2_SKIP]
        print(f"building U8g2 ({len(sources)} files, first run only) ...")

        def compile_one(src):
            obj = out / (src.stem + ".o")
            run(cc + ["-O1", "-w", "-c", str(src), "-I", str(clib), "-o", str(obj)])
            return obj.name

        with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as pool:
            objects = list(pool.map(compile_one, sources))
        run(ar + ["rcs", lib.name] + objects, cwd=out)

    # Only the fonts the sketch uses, cut out of u8g2_fonts.c
    font_obj = out / ("fonts-" + hashlib.sha1(",".join(fonts).encode()).hexdigest()[:10] + ".o")
    if not font_obj.exists():
        all_fonts = (clib / "u8g2_fonts.c").read_text(encoding="latin-1")
        parts = ['#include "u8g2.h"\n']
        for name in fonts:
            m = re.search(r"const uint8_t %s\[\d+\] U8G2_FONT_SECTION\(\"%s\"\) =.*?\";\s*\n" % (name, name),
                          all_fonts, re.S)
            if not m:
                sys.exit(f"font {name} not found in u8g2_fonts.c")
            parts.append(m.group(0))
        src = font_obj.with_suffix(".c")
        src.write_text("".join(parts), encoding="latin-1")
        run(cc + ["-O1", "-w", "-c", str(src), "-I", str(clib), "-o", str(font_obj)])
    return lib, font_obj


# ------------------------------------------------------------------ sketch

# Top-level function definitions (column 0), possibly spanning several lines
FUNC_RE = re.compile(r"^([A-Za-z_][\w \t*&:<>,]*?[\s*&])(\w+)\s*\(([^()]*)\)\s*\{", re.M)


def merge_sketch(sketch_dir, out_file):
    """Concatenate the .ino tabs and add prototypes, like the Arduino builder."""
    main = sketch_dir / f"{sketch_dir.name}.ino"
    others = sorted((p for p in sketch_dir.glob("*.ino") if p != main), key=lambda p: p.name.lower())
    texts = {p: p.read_text(encoding="utf-8", errors="replace") for p in [main] + others}

    prototypes = []
    for text in texts.values():
        for ret, name, params in FUNC_RE.findall(text):
            prototypes.append(f"{' '.join(ret.split())} {name}({' '.join(params.split())});\n")

    lines = texts[main].splitlines(keepends=True)
    after_includes = max(i for i, line in enumerate(lines) if line.lstrip().startswith("#include")) + 1

    parts = [f'#line 1 "{main.as_posix()}"\n', *lines[:after_includes], *prototypes,
             f'#line {after_includes + 1} "{main.as_posix()}"\n', *lines[after_includes:]]
    for p in others:
        parts += [f'\n#line 1 "{p.as_posix()}"\n', texts[p]]
    out_file.write_text("".join(parts), encoding="utf-8")


def export_revision(ref):
    """Extract OLED_Device/ from a git revision into the build folder."""
    safe = re.sub(r"[^\w.-]", "_", ref)
    dest = BUILD / f"rev-{safe}"
    if dest.exists():
        shutil.rmtree(dest)
    archive = subprocess.run(["git", "-C", str(ROOT), "archive", "--format=tar", ref, "OLED_Device"],
                             capture_output=True)
    if archive.returncode != 0:
        sys.exit(f"git archive {ref} failed: {archive.stderr.decode().strip()}")
    with tarfile.open(fileobj=io.BytesIO(archive.stdout)) as tar:
        if hasattr(tarfile, "data_filter"):
            tar.extractall(dest, filter="data")
        else:
            tar.extractall(dest)
    return dest / "OLED_Device"


# ------------------------------------------------------------------ tests

def run_button_test(cxx):
    out = BUILD / "button"
    out.mkdir(parents=True, exist_ok=True)
    wrapper = out / "button_main.cpp"
    wrapper.write_text(f'#define BUTTON_FILE "{(SKETCH / "Button.ino").as_posix()}"\n'
                       f'#include "{(TEST / "button_test.cpp").as_posix()}"\n')
    exe = out / ("button_test" + EXE)
    run(cxx + ["-std=c++17", "-O1", "-w", "-I", str(SKETCH), str(wrapper), "-o", str(exe)])
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    print(result.stdout, end="")
    return result.returncode == 0


def run_simulation(label, sketch_dir, cxx, clib, lib, font_obj, env_extra):
    out = BUILD / f"sim-{label}"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    merged = out / "sketch_merged.cpp"
    merge_sketch(sketch_dir, merged)
    wrapper = out / "sim_main.cpp"
    wrapper.write_text(f'#define SKETCH_FILE "{merged.as_posix()}"\n'
                       f'#include "{(TEST / "sim" / "harness.cpp").as_posix()}"\n')
    exe = out / ("sim" + EXE)
    run(cxx + ["-std=c++17", "-O2", "-w", "-I", str(TEST / "sim" / "mock"), "-I", str(clib),
               "-I", str(sketch_dir), str(wrapper), str(font_obj), str(lib), "-o", str(exe)])
    snapshots = out / "screens"
    snapshots.mkdir()
    env = dict(os.environ, SIM_OUT=str(snapshots), **env_extra)
    result = subprocess.run([str(exe)], capture_output=True, text=True, env=env)
    print(result.stdout, end="")
    if result.stderr:
        print(result.stderr, end="")
    return result.returncode == 0, snapshots


def read_metrics(folder):
    metrics = {}
    for line in (folder / "metrics.txt").read_text().splitlines():
        name, value = line.split()
        metrics[name] = float(value)
    return metrics


def compare(baseline_dir, current_dir, ref):
    print(f"\n== Compared with {ref}")
    names = sorted({p.name for p in baseline_dir.glob("*.bin")} | {p.name for p in current_dir.glob("*.bin")})
    differing = 0
    for name in names:
        a, b = baseline_dir / name, current_dir / name
        if not (a.exists() and b.exists()):
            print(f"  {name[:-4]:22s} only in {'baseline' if a.exists() else 'current'}")
            differing += 1
            continue
        pixels = sum(bin(x ^ y).count("1") for x, y in zip(a.read_bytes(), b.read_bytes()))
        if pixels:
            print(f"  {name[:-4]:22s} differs ({pixels} pixels)")
            differing += 1
    print(f"  screens: {len(names) - differing}/{len(names)} identical")

    old, new = read_metrics(baseline_dir), read_metrics(current_dir)
    print(f"  {'metric':24s} {'baseline':>10s} {'current':>10s}")
    for name in sorted(set(old) & set(new)):
        print(f"  {name:24s} {old[name]:10.1f} {new[name]:10.1f}")
    return differing == 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline", metavar="GIT_REF",
                        help="also simulate this revision and compare screens and metrics")
    parser.add_argument("--u8g2", metavar="PATH", help="U8g2 library folder (default: Arduino libraries or download)")
    parser.add_argument("--show-screens", action="store_true", help="print some screens as ASCII art")
    parser.add_argument("--txn-us", type=float, default=50.0,
                        help="assumed fixed cost of one I2C transaction in microseconds (default 50)")
    args = parser.parse_args()

    BUILD.mkdir(exist_ok=True)
    cc, cxx, ar = find_toolchain()
    clib = find_u8g2(args.u8g2)

    sketches = [SKETCH]
    baseline_sketch = export_revision(args.baseline) if args.baseline else None
    if baseline_sketch:
        sketches.append(baseline_sketch)
    lib, font_obj = build_u8g2(clib, cc, ar, sketch_fonts(sketches))

    env = {"SIM_TXN_US": str(args.txn_us)}
    if args.show_screens:
        env["SIM_SHOW"] = "1"

    print("== Button timing")
    ok = run_button_test(cxx)

    print("\n== Simulation (working tree)")
    sim_ok, current = run_simulation("current", SKETCH, cxx, clib, lib, font_obj, env)
    ok &= sim_ok

    if baseline_sketch:
        print(f"\n== Simulation ({args.baseline}, for comparison only: its checks do not affect the result)")
        _, baseline = run_simulation("baseline", baseline_sketch, cxx, clib, lib, font_obj, env)
        ok &= compare(baseline, current, args.baseline)

    print("\nALL TESTS PASSED" if ok else "\nTESTS FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
