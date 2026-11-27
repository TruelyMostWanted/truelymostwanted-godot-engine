#!/usr/bin/env python3
"""Checks the release files of .github/truely/package-release.sh before they are published.

Usage: python .github/truely/verify-release.py <release-dir> <name> <config>
  release-dir  folder with the zips and .tpz files
  name         e.g. Truely-v0.1.0_Godot4.8-beta5_a1b2c3d
  config       version.txt of the standard templates, e.g. 4.8.beta5

Checks:
  - every Mono editor zip carries Godot.NET.Sdk, Godot.SourceGenerators, GodotSharp and GodotSharpEditor
    in exactly the version that build_assemblies.py gives this engine (e.g. 4.8.0-beta.5),
    and the version inside each .nuspec matches its file name
  - executables match the platform of the zip: PE in win64, ELF in linux, Mach-O in macos
    (no Linux apphost such as GodotTools.OpenVisualStudio without .exe in a Windows zip)
  - the Windows Mono editor zip has GodotTools.OpenVisualStudio.exe
  - templates/version.txt of the .tpz files is <config> or <config>.mono

Runs with the standard library only, on Linux (CI) and on Windows. Exits with 1 when a check failed.
"""

import re
import sys
import zipfile
from pathlib import Path

PACKAGES = ["Godot.NET.Sdk", "Godot.SourceGenerators", "GodotSharp", "GodotSharpEditor"]

ELF = b"\x7fELF"
PE = b"MZ"
MACHO = {
    b"\xcf\xfa\xed\xfe",  # 64-bit
    b"\xce\xfa\xed\xfe",  # 32-bit
    b"\xca\xfe\xba\xbe",  # universal (also Java class files, which a release does not hold)
}

errors: list[str] = []


def fail(message: str) -> None:
    errors.append(message)
    print(f"  FAIL  {message}")


def ok(message: str) -> None:
    print(f"  ok    {message}")


def package_version(config: str) -> str:
    """GodotSharp package version as build_assemblies.py computes it: 4.8.beta5 -> 4.8.0-beta.5."""
    number, _, status = config.rpartition(".")
    parts = number.split(".")
    while len(parts) < 3:
        parts.append("0")
    version = ".".join(parts)
    if status == "stable":
        return version
    m = re.fullmatch(r"(.*?)(\d+)", status)
    return f"{version}-{m.group(1)}.{m.group(2)}" if m else f"{version}-{status}"


def kind(head: bytes) -> str:
    if head.startswith(ELF):
        return "ELF"
    if head.startswith(PE):
        return "PE"
    if head[:4] in MACHO:
        return "Mach-O"
    return ""


def check_binaries(z: zipfile.ZipFile, platform: str) -> None:
    expected = {"win64": "PE", "linux": "ELF", "macos": "Mach-O"}[platform]
    count = 0
    for info in z.infolist():
        if info.is_dir() or info.file_size < 4:
            continue
        with z.open(info) as f:
            k = kind(f.read(4))
        if not k:
            continue
        # .NET assemblies (.dll) are PE files on every platform.
        if k == "PE" and info.filename.endswith(".dll"):
            continue
        count += 1
        if k != expected:
            fail(f"{info.filename} is {k}, expected {expected}")
        elif platform == "win64" and not info.filename.endswith(".exe"):
            fail(f"{info.filename} is a Windows program without .exe")
    ok(f"{count} native executables checked ({expected})")


def check_packages(z: zipfile.ZipFile, version: str) -> None:
    nupkgs = {Path(n).name: n for n in z.namelist() if "/GodotSharp/Tools/nupkgs/" in n and n.endswith(".nupkg")}
    for package in PACKAGES:
        wanted = f"{package}.{version}.nupkg"
        if wanted not in nupkgs:
            found = sorted(n for n in nupkgs if n.startswith(package + ".") and n[len(package) + 1].isdigit())
            fail(f"{wanted} missing (found: {', '.join(found) or 'none'})")
            continue
        with zipfile.ZipFile(z.open(nupkgs[wanted])) as pkg:
            nuspec = next((n for n in pkg.namelist() if n.endswith(".nuspec")), None)
            text = pkg.read(nuspec).decode("utf-8") if nuspec else ""
        m = re.search(r"<version>([^<]+)</version>", text)
        if not m or m.group(1) != version:
            fail(f"{wanted}: .nuspec has version {m.group(1) if m else 'none'}")
        else:
            ok(wanted)
    others = sorted(n for n in nupkgs if not n.endswith(f".{version}.nupkg"))
    if others:
        fail(f"packages of another version: {', '.join(others)}")


def check_editor(path: Path, platform: str, mono: bool, version: str) -> None:
    with zipfile.ZipFile(path) as z:
        check_binaries(z, platform)
        if not any(n.endswith("._sc_") for n in z.namelist()):
            fail("._sc_ missing (self-contained mode)")
        if mono:
            check_packages(z, version)
            if platform == "win64":
                if any(n.endswith("/GodotSharp/Tools/GodotTools.OpenVisualStudio.exe") for n in z.namelist()):
                    ok("GodotTools.OpenVisualStudio.exe")
                else:
                    fail("GodotSharp/Tools/GodotTools.OpenVisualStudio.exe missing")


def check_templates(path: Path, config: str) -> None:
    with zipfile.ZipFile(path) as z:
        actual = z.read("templates/version.txt").decode("utf-8").strip()
        if actual != config:
            fail(f"templates/version.txt is {actual}, expected {config}")
        else:
            ok(f"version.txt {actual}")
        for info in z.infolist():
            name = Path(info.filename).name
            if name.startswith("windows_") or name.startswith("linux_"):
                with z.open(info) as f:
                    k = kind(f.read(4))
                want = "PE" if name.startswith("windows_") else "ELF"
                if k != want:
                    fail(f"{info.filename} is {k or 'unknown'}, expected {want}")
        ok("template executables checked")


def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    folder, name, config = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
    version = package_version(config)
    print(f"Release {name}, config {config}, .NET packages {version}")

    files = sorted(folder.glob(f"{name}_*"))
    if not files:
        print(f"No release files {name}_* in {folder}")
        return 1
    for path in files:
        print(f"== {path.name}")
        rest = path.name[len(name) + 1 :]
        mono = path.stem.endswith("_dotnet")
        if path.suffix == ".tpz":
            check_templates(path, config + (".mono" if mono else ""))
        elif rest.startswith("windows"):
            check_editor(path, "win64", mono, version)
        elif rest.startswith("linux"):
            check_editor(path, "linux", mono, version)
        elif rest.startswith("macos"):
            check_editor(path, "macos", mono, version)
        else:
            print("  skipped")

    print()
    if errors:
        print(f"{len(errors)} problem(s):")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("All release files passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
