"""Generates core/truely_version.gen.h from truely/version.txt (the version of the truely-godot-engine fork).

The fork keeps its own SemVer number in truely/version.txt instead of version.py, so syncing with
Godot never conflicts. The Godot version (version.py) stays the base version of the engine.
"""

import re
import sys

import methods
from misc.utility.color import print_error

TRUELY_VERSION_FILE = methods.base_folder / "truely" / "version.txt"


def get_truely_version_info() -> dict[str, int]:
    """Reads truely/version.txt (one line "MAJOR.MINOR.PATCH") and stops the build if it is invalid."""
    try:
        text = TRUELY_VERSION_FILE.read_text(encoding="utf-8").strip()
    except OSError as err:
        print_error(f"Cannot read the Truely version file {TRUELY_VERSION_FILE}: {err}")
        sys.exit(255)

    match = re.fullmatch(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", text)
    if match is None:
        print_error(
            f'Invalid Truely version "{text}" in {TRUELY_VERSION_FILE}, expected "MAJOR.MINOR.PATCH" (e.g. 0.1.0).'
        )
        sys.exit(255)

    return {
        "major": int(match.group(1)),
        "minor": int(match.group(2)),
        "patch": int(match.group(3)),
    }


def truely_version_builder(target, source, env):
    with methods.generated_wrapper(str(target[0])) as file:
        file.write(
            """\
#define TRUELY_VERSION_MAJOR {major}
#define TRUELY_VERSION_MINOR {minor}
#define TRUELY_VERSION_PATCH {patch}
""".format(**source[0].read())
        )
