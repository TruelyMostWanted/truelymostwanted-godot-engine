"""Functions used to generate source files during build time"""

from collections import OrderedDict

import methods


def make_default_controller_mappings(target, source, env):
    with methods.generated_wrapper(str(target[0])) as file:
        file.write("""\
#include "core/input/default_controller_mappings.h"

#include "core/typedefs.h"

""")

        PLATFORM_VARIABLES = {
            "Linux": "LINUXBSD",
            "Windows": "WINDOWS",
            "Mac OS X": "MACOS",
            "Android": "ANDROID",
            "iOS": "APPLE_EMBEDDED",
            "Web": "WEB",
        }

        # ensure mappings have a consistent order
        platform_mappings = OrderedDict()
        for src_path in map(str, source):
            with open(src_path, "r", encoding="utf-8") as f:
                mapping_file_lines = f.readlines()

            current_platform = None
            for line in mapping_file_lines:
                if not line:
                    continue
                line = line.strip()
                if len(line) == 0:
                    continue
                if line[0] == "#":
                    platform_or_header = line[1:].strip()
                    if platform_or_header not in PLATFORM_VARIABLES:
                        continue  # Header
                    current_platform = platform_or_header
                    if current_platform not in platform_mappings:
                        platform_mappings[current_platform] = {}
                elif current_platform:
                    line_parts = line.split(",")
                    guid = line_parts[0]
                    if guid in platform_mappings[current_platform]:
                        file.write(
                            f"// WARNING: DATABASE {src_path} OVERWROTE PRIOR MAPPING: {current_platform} {platform_mappings[current_platform][guid]}\n"
                        )
                    platform_mappings[current_platform][guid] = line

        file.write("const char *DefaultControllerMappings::mappings[] = {\n")
        for platform, mappings in platform_mappings.items():
            variable = PLATFORM_VARIABLES[platform]
            file.write(f"#ifdef {variable}_ENABLED\n")
            for mapping in mappings.values():
                file.write(f'\t"{mapping}",\n')
            file.write(f"#endif // {variable}_ENABLED\n")

        file.write("\tnullptr\n};\n")


def make_builtin_input_actions(target, source, env):
    """Generates the table of Godot's built-in input actions (InputMap::get_builtins()) from builtin_input_actions.json."""
    import json

    with open(str(source[0]), "r", encoding="utf-8") as f:
        data = json.load(f)

    events = []
    actions = []
    for action in data["actions"]:
        name = action["name"]
        if action.get("feature"):
            name += "." + action["feature"]
        first = len(events)
        for event in action["events"]:
            event_type = event["type"]
            if event_type == "key":
                key = " | ".join(
                    ["Key::" + event["keycode"]] + ["KeyModifierMask::" + m for m in event.get("modifiers", [])]
                )
                events.append(
                    f"{{ BuiltinInputActions::EVENT_KEY, {key}, JoyButton::INVALID, JoyAxis::INVALID, 0.0f }}"
                )
            elif event_type == "joypad_button":
                events.append(
                    f"{{ BuiltinInputActions::EVENT_JOYPAD_BUTTON, Key::NONE, JoyButton::{event['button']}, JoyAxis::INVALID, 0.0f }}"
                )
            elif event_type == "joypad_motion":
                value = float(event["axis_value"])
                events.append(
                    f"{{ BuiltinInputActions::EVENT_JOYPAD_MOTION, Key::NONE, JoyButton::INVALID, JoyAxis::{event['axis']}, {value!r}f }}"
                )
            else:
                raise ValueError(f'builtin_input_actions.json: unknown event type "{event_type}" in "{name}".')
        count = len(events) - first
        pointer = f"&_builtin_input_events[{first}]" if count > 0 else "nullptr"
        actions.append(f'{{ "{name}", {pointer}, {count} }}')

    with methods.generated_wrapper(str(target[0])) as file:
        file.write("""\
#include "core/input/builtin_input_actions.h"

""")
        file.write("static const BuiltinInputActions::Event _builtin_input_events[] = {\n")
        for event in events:
            file.write(f"\t{event},\n")
        file.write("};\n\n")
        file.write("const BuiltinInputActions::Action BuiltinInputActions::actions[] = {\n")
        for action in actions:
            file.write(f"\t{action},\n")
        file.write("};\n\n")
        file.write(f"const int BuiltinInputActions::action_count = {len(actions)};\n")
