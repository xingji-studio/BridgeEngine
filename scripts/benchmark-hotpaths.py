#!/usr/bin/env python3
"""Compare text/pack modules against a git revision using identical C harnesses.

Requires a C11 compiler and an already configured CMake build directory.
Text uses a mock platform: its call counts do not measure SDL/GPU frame rates.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, **kwargs).stdout.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-ref", required=True)
    parser.add_argument("--build-dir", default="build/check")
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--runs", type=int, default=3)
    args = parser.parse_args()
    if args.runs < 1:
        parser.error("--runs must be positive")
    root = Path(__file__).resolve().parent.parent
    generated = (root / args.build_dir / "generated").resolve()
    if not (generated / "bridgeengine_version.h").is_file():
        parser.error("configure CMake first, then pass its build directory via --build-dir")
    baseline = run(["git", "rev-parse", "--verify", args.baseline_ref + "^{commit}"], cwd=root)
    print("Baseline:", baseline)
    print("Compiler:", run([args.cc, "--version"]).splitlines()[0])
    with tempfile.TemporaryDirectory(prefix="bridgeengine-bench-") as directory:
        work = Path(directory)
        common = [args.cc, "-std=c11", "-O2", "-D_POSIX_C_SOURCE=200809L",
                  "-I" + str(root / "include"), "-I" + str(generated),
                  "-I" + str(root / "src"), "-I" + str(root / "thirdparty/rzip")]
        for version in ("baseline", "optimized"):
            for module in ("text", "pack"):
                source = root / "src" / (module + ".c")
                if version == "baseline":
                    source = work / (module + ".c")
                    source.write_text(run(["git", "show", baseline + ":src/" + module + ".c"], cwd=root),
                                      encoding="utf-8")
                executable = work / (version + "-" + module)
                if module == "text":
                    harness = ["tests/text_cache_test.c", "src/core/render_context.c", "src/platform/platform.c"]
                    defines = ["-DBAPI_TEXT_CACHE_CAPACITY=8", "-DBAPI_TEXT_CACHE_BYTES=4096"]
                    invocation = [str(executable), "--benchmark"]
                else:
                    harness = ["tests/pack_index_test.c", "thirdparty/rzip/rz_lib.c"]
                    defines = []
                    invocation = [str(executable), str(work / "pack.rz"), "--benchmark"]
                run(common + defines + [str(source)] + [str(root / f) for f in harness]
                    + ["-o", str(executable)], cwd=root)
                for iteration in range(args.runs):
                    print(f"{version} run {iteration + 1}: {run(invocation)}")


if __name__ == "__main__":
    main()
