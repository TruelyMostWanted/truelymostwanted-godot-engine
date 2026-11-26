#!/usr/bin/env bash
# Assembles the release files of .github/workflows/truely_release.yml from the build artifacts.
#
# Usage: .github/truely/package-release.sh <artifacts-dir> <out-dir> <name> <config>
#   artifacts-dir  bin/ files of all build jobs (godot.<platform>.<target>...) and
#                  <platform>/GodotSharp/ for linuxbsd, windows and macos
#   name           e.g. Truely-v0.1.0_Godot4.8-beta5_a1b2c3d
#   config         version.txt of the standard templates, e.g. 4.8.beta5 (Mono gets ".mono")
#
# Editor zips hold one folder with the editor and ._sc_ (self-contained mode).
# The .tpz files hold templates/ with version.txt, named like the official Godot templates.
# PARTIAL=1: macOS builds that are missing are left out (published later by truely_release_update.yml).
set -euo pipefail

A="$(cd "$1" && pwd)"
mkdir -p "$2"
OUT="$(cd "$2" && pwd)"
NAME="$3"
CONFIG="$4"
DIST="$(cd "$(dirname "$0")/../../misc/dist" && pwd)"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

bin() { # bin <file>: path of a build, fails if missing
	if [ ! -e "$A/$1" ]; then
		echo "Missing build: $1" >&2
		exit 1
	fi
	echo "$A/$1"
}

zipdir() { # zipdir <dir> <zip>: zips the content of <dir>, keeps permissions and symlinks
	rm -f "$2"
	(cd "$1" && zip -q -r -y -9 "$2" .)
	echo "  $(basename "$2")"
}

sharp() { # sharp <platform> <dest>: copies GodotSharp/ of a platform
	if [ ! -d "$A/$1/GodotSharp" ]; then
		echo "Missing GodotSharp for $1" >&2
		exit 1
	fi
	cp -R "$A/$1/GodotSharp" "$2/GodotSharp"
}

echo "Editors"
for m in "" ".mono"; do
	p="${m:+mono_}"

	# Windows
	f="${NAME}_${p}win64"
	d="$STAGE/$f/$f"
	mkdir -p "$d"
	cp "$(bin "godot.windows.editor.x86_64$m.exe")" "$d/$f.exe"
	cp "$(bin "godot.windows.editor.x86_64$m.console.exe")" "$d/${f}_console.exe"
	touch "$d/._sc_"
	[ -z "$m" ] || sharp windows "$d"
	zipdir "$STAGE/$f" "$OUT/$f.zip"

	# Linux
	if [ -z "$m" ]; then f="${NAME}_linux.x86_64"; else f="${NAME}_mono_linux_x86_64"; fi
	d="$STAGE/$f/$f"
	mkdir -p "$d"
	cp "$(bin "godot.linuxbsd.editor.x86_64$m")" "$d/${NAME}_${p}linux.x86_64"
	chmod +x "$d/${NAME}_${p}linux.x86_64"
	touch "$d/._sc_"
	[ -z "$m" ] || sharp linuxbsd "$d"
	zipdir "$STAGE/$f" "$OUT/$f.zip"

	# macOS: Godot.app with ._sc_ next to it (the editor looks outside the bundle)
	if [ -n "${PARTIAL:-}" ] && [ ! -e "$A/godot.macos.editor.universal$m" ]; then
		echo "  (skipped: no macOS editor$m build, PARTIAL)"
		continue
	fi
	f="${NAME}_${p}macos.universal"
	d="$STAGE/$f/$f"
	mkdir -p "$d"
	cp -R "$DIST/macos_tools.app" "$d/Godot.app"
	mkdir -p "$d/Godot.app/Contents/MacOS"
	cp "$(bin "godot.macos.editor.universal$m")" "$d/Godot.app/Contents/MacOS/Godot"
	chmod +x "$d/Godot.app/Contents/MacOS/Godot"
	touch "$d/._sc_"
	[ -z "$m" ] || sharp macos "$d/Godot.app/Contents/Resources"
	zipdir "$STAGE/$f" "$OUT/$f.zip"
done

echo "Export templates"
for m in "" ".mono"; do
	p="${m:+mono_}"
	t="$STAGE/${p}tpz/templates"
	mkdir -p "$t"
	printf '%s' "$CONFIG$m" >"$t/version.txt"

	for target in debug release; do
		cp "$(bin "godot.windows.template_$target.x86_64$m.exe")" "$t/windows_${target}_x86_64.exe"
		cp "$(bin "godot.windows.template_$target.x86_64$m.console.exe")" "$t/windows_${target}_x86_64_console.exe"
		cp "$(bin "godot.linuxbsd.template_$target.x86_64$m")" "$t/linux_$target.x86_64"
		chmod +x "$t/linux_$target.x86_64"
		# Web: no Mono builds, both packs carry the standard Web templates.
		cp "$(bin "godot.web.template_$target.wasm32.zip")" "$t/web_$target.zip"
		cp "$(bin "godot.web.template_$target.wasm32.nothreads.zip")" "$t/web_nothreads_$target.zip"
	done

	# macOS: macos.zip with macos_template.app
	if [ -n "${PARTIAL:-}" ] && [ ! -e "$A/godot.macos.template_release.universal$m" ]; then
		echo "  (no macOS templates$m, PARTIAL)"
		zipdir "$STAGE/${p}tpz" "$OUT/${NAME}_${p}export_templates.tpz"
		continue
	fi
	app="$STAGE/${p}macos/macos_template.app"
	mkdir -p "$(dirname "$app")"
	cp -R "$DIST/macos_template.app" "$app"
	mkdir -p "$app/Contents/MacOS"
	for target in debug release; do
		cp "$(bin "godot.macos.template_$target.universal$m")" "$app/Contents/MacOS/godot_macos_$target.universal"
		chmod +x "$app/Contents/MacOS/godot_macos_$target.universal"
	done
	zipdir "$STAGE/${p}macos" "$t/macos.zip" >/dev/null

	zipdir "$STAGE/${p}tpz" "$OUT/${NAME}_${p}export_templates.tpz"
done
