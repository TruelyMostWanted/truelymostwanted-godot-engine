#!/usr/bin/env bash
# Writes the Markdown release notes of .github/workflows/truely_release.yml to stdout.
#
# Usage: .github/truely/release-notes.sh <sha> <tag> <config> [<previous-tag>]
#   sha           the commit the release is built from
#   tag           the new release tag, e.g. truely-godot4.8-beta5-a1b2c3d
#   config        the Godot base, e.g. 4.8.beta5
#   previous-tag  optional, overrides the search for the previous release (any revision works)
#
# Needs git (full history with tags) and gh (GH_TOKEN or GITHUB_TOKEN in CI). The repository comes
# from GITHUB_REPOSITORY, else from the origin remote.
#
# Changes: the first-parent commits since the previous truely-godot* tag (without one: the last
# 50), merge commits and "[skip ci]" commits left out. The issue number "(#N)" at the end of each
# subject is looked up with gh and sorted by its "type: ..." label.
set -euo pipefail

SHA="$(git rev-parse "$1^{commit}")"
TAG="$2"
CONFIG="$3"

REPO="${GITHUB_REPOSITORY:-}"
if [ -z "$REPO" ]; then
	REPO="$(git remote get-url origin | sed -E 's#^.*github\.com[:/]##; s#\.git$##')"
fi
URL="https://github.com/$REPO"

# Previous release: the newest truely-godot* tag reachable from SHA that does not point at SHA itself.
PREV="${4:-}"
[ -n "$PREV" ] || while read -r t; do
	[ -n "$t" ] || continue
	if [ "$(git rev-parse "$t^{commit}")" != "$SHA" ]; then
		PREV="$t"
		break
	fi
done < <(git tag --merged "$SHA" --sort=-committerdate --list 'truely-godot*')

if [ -n "$PREV" ]; then
	RANGE=("$PREV..$SHA")
else
	RANGE=(-n 50 "$SHA")
fi

declare -A seen=()
order=()
others=()
while IFS= read -r subject; do
	case "$subject" in *"[skip ci]"*) continue ;; esac
	if [[ "$subject" =~ \(#([0-9]+)\)( \(#([0-9]+)\))?$ ]]; then
		for n in "${BASH_REMATCH[1]}" "${BASH_REMATCH[3]}"; do
			[ -n "$n" ] || continue
			if [ -z "${seen[$n]:-}" ]; then
				seen[$n]=1
				order+=("$n")
			fi
		done
	elif [ -n "$PREV" ]; then
		others+=("$subject")
	fi
done < <(git log --first-parent --no-merges --reverse --format='%s' "${RANGE[@]}")

features=()
fixes=()
tasks=()
for n in "${order[@]}"; do
	# One call for issues and pull requests; squash merges may append the PR number after the issue.
	if ! info="$(gh api "repos/$REPO/issues/$n" --jq '[(if .pull_request then "pr" else "issue" end), .title, .html_url, ([.labels[].name] | join(","))] | @tsv' 2>/dev/null)"; then
		tasks+=("- [#$n]($URL/issues/$n)")
		continue
	fi
	IFS=$'\t' read -r kind title link labels <<<"$info"
	[ "$kind" = "issue" ] || continue
	# Drop the template prefix "[Feature] ", "[Bug] ", ...
	title="$(sed -E 's/^\[[^]]*\] *//' <<<"$title")"
	line="- $title ([#$n]($link))"
	case ",$labels," in
		*",type: feature,"*) features+=("$line") ;;
		*",type: bug,"*) fixes+=("$line") ;;
		*) tasks+=("$line") ;;
	esac
done

section() {
	local heading="$1"
	shift
	[ "$#" -gt 0 ] || return 0
	printf '### %s\n\n' "$heading"
	printf '%s\n' "$@"
	printf '\n'
}

# Godot base: number and status at SHA and at the previous release.
base_of() {
	local rev="$1" major minor patch status
	major="$(git show "$rev:version.py" | sed -nE 's/^major = ([0-9]+).*/\1/p')"
	minor="$(git show "$rev:version.py" | sed -nE 's/^minor = ([0-9]+).*/\1/p')"
	patch="$(git show "$rev:version.py" | sed -nE 's/^patch = ([0-9]+).*/\1/p')"
	status="$(git show "$rev:truely/godot-status.txt" 2>/dev/null | tr -d '[:space:]' || true)"
	[ -n "$status" ] || status="$(git show "$rev:version.py" | sed -nE 's/^status = "([^"]*)".*/\1/p')"
	if [ "${patch:-0}" != "0" ]; then
		printf '%s.%s.%s %s' "$major" "$minor" "$patch" "$status"
	else
		printf '%s.%s %s' "$major" "$minor" "$status"
	fi
}

NOW="$(base_of "$SHA")"
HASH="${SHA:0:9}"

printf 'Truely Godot Engine at commit [`%s`](%s/commit/%s), based on Godot %s.\n' "${SHA:0:7}" "$URL" "$SHA" "$NOW"
printf 'Version string: `%s.truely.%s` (Mono: `%s.mono.truely.%s`).\n\n' "$CONFIG" "$HASH" "$CONFIG" "$HASH"

printf '## Changes\n\n'
if [ -n "$PREV" ]; then
	printf 'Since [%s](%s/releases/tag/%s).\n\n' "$PREV" "$URL" "$PREV"
else
	printf 'First release: changes of the last 50 commits.\n\n'
fi
section "Features" "${features[@]+"${features[@]}"}"
section "Fixes" "${fixes[@]+"${fixes[@]}"}"
section "Refactoring and tasks" "${tasks[@]+"${tasks[@]}"}"
if [ "${#others[@]}" -gt 0 ]; then
	others_lines=()
	for s in "${others[@]}"; do others_lines+=("- $s"); done
	section "Other commits" "${others_lines[@]}"
fi
printf 'How to use the features from GDScript, C# and GDExtension: [wiki](%s/wiki).\n\n' "$URL"

printf '## Godot base\n\n'
if [ -n "$PREV" ]; then
	BEFORE="$(base_of "$PREV")"
	if [ "$BEFORE" != "$NOW" ]; then
		if [ "${BEFORE%% *}" = "${NOW%% *}" ]; then
			printf 'Godot %s → %s.\n\n' "$BEFORE" "${NOW#* }"
		else
			printf 'Godot %s → %s.\n\n' "$BEFORE" "$NOW"
		fi
	else
		printf 'Godot %s (unchanged).\n\n' "$NOW"
	fi
else
	printf 'Godot %s.\n\n' "$NOW"
fi
carried=()
while IFS=$'\t' read -r branch pr issue title; do
	case "$branch" in '' | '#'*) continue ;; esac
	carried+=("- $title: [godotengine/godot#$pr](https://github.com/godotengine/godot/pull/$pr) ([#$issue]($URL/issues/$issue))")
done < <(git show "$SHA:truely/upstream-prs.tsv" 2>/dev/null || true)
if [ "${#carried[@]}" -gt 0 ]; then
	printf 'Godot pull requests included before Godot merges them:\n\n'
	printf '%s\n' "${carried[@]}"
	printf '\n'
fi

cat <<EOF
## Install

- **Editors:** \`*_win64.zip\`, \`*_linux.x86_64.zip\`, \`*_macos.universal.zip\`, and the same with \`mono_\` for C#. Unzip and keep the folder together.
- **Self-contained:** every editor folder contains a \`._sc_\` file. Settings, caches and export templates stay in that folder, so this build and the official Godot of the same version never overwrite each other's templates.
- **Export templates:** \`*_export_templates.tpz\` and \`*_mono_export_templates.tpz\` (Windows, Linux, macOS, Web with and without threads). In the editor: Editor → Manage Export Templates → Install from File.
- **macOS:** the builds are not signed or notarized. First start: right-click Godot.app → Open. For the self-contained mode, run \`xattr -dr com.apple.quarantine <folder>\` once, otherwise macOS starts the app from a random path and does not find \`._sc_\`.
- **Mono (C#):** needs the .NET SDK 10 or newer. The editor brings its own Godot NuGet packages (\`GodotSharp/Tools/nupkgs\`). The Web export has no C# support.

EOF
if [ -n "$PREV" ]; then
	printf '**Full diff:** [%s...%s](%s/compare/%s...%s)\n' "$PREV" "$TAG" "$URL" "$PREV" "$TAG"
else
	printf '**Full history:** [%s](%s/commits/%s)\n' "$TAG" "$URL" "$TAG"
fi
