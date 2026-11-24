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


# Properties the build reads from builtin_input_actions.tres, per class. Anything else is an error, so no
# data of the file is dropped without notice.
_BUILTIN_INPUT_PROPERTIES = {
    "InputAction": {"action", "events"},
    "InputEventKey": {
        "keycode",
        "command_or_control_autoremap",
        "alt_pressed",
        "shift_pressed",
        "ctrl_pressed",
        "meta_pressed",
    },
    "InputEventJoypadButton": {"device", "button_index"},
    "InputEventJoypadMotion": {"device", "axis", "axis_value"},
}

# InputEventWithModifiers property and KeyModifierMask value, in the order of the generated code.
_BUILTIN_INPUT_MODIFIERS = [
    ("command_or_control_autoremap", "CMD_OR_CTRL"),
    ("shift_pressed", "SHIFT"),
    ("alt_pressed", "ALT"),
    ("ctrl_pressed", "CTRL"),
    ("meta_pressed", "META"),
]


def _parse_tres_value(path, line_number, text):
    """Parses the property values used by builtin_input_actions.tres: bools, numbers, strings,
    string names, SubResource() and (typed) arrays of SubResource()."""
    import re

    text = text.strip()
    if text in ("true", "false"):
        return text == "true"
    match = re.fullmatch(r'&?"((?:[^"\\]|\\.)*)"', text)
    if match:
        return match.group(1).replace('\\"', '"').replace("\\\\", "\\")
    match = re.fullmatch(r'SubResource\(\s*"([^"]+)"\s*\)', text)
    if match:
        return ("SubResource", match.group(1))
    match = re.fullmatch(r"(?:Array\[\w+\]\()?\[(.*)\]\)?", text)
    if match:
        items = [item.strip() for item in match.group(1).split(",") if item.strip()]
        return [_parse_tres_value(path, line_number, item) for item in items]
    try:
        return int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        pass
    raise ValueError(f"{path}:{line_number}: unsupported value `{text}`.")


def _parse_builtin_input_actions_tres(path):
    """Reads builtin_input_actions.tres, an InputActions resource in Godot's text resource format.
    Returns a list of (action name, events); an event is a dict with "type" and its values."""
    import re

    header = None
    sections = []  # (tag, attributes, properties)
    with open(path, "r", encoding="utf-8") as f:
        for line_number, raw_line in enumerate(f, 1):
            line = raw_line.strip()
            if not line or line.startswith(";"):
                continue
            match = re.fullmatch(r"\[(gd_resource|ext_resource|sub_resource|resource)\b(.*)\]", line)
            if match:
                attributes = dict(re.findall(r'(\w+)=("[^"]*"|\S+)', match.group(2)))
                attributes = {key: value.strip('"') for key, value in attributes.items()}
                if match.group(1) == "gd_resource":
                    header = attributes
                elif match.group(1) == "ext_resource":
                    raise ValueError(f"{path}:{line_number}: external resources are not supported.")
                else:
                    sections.append((match.group(1), attributes, {}))
                continue
            match = re.fullmatch(r"([\w/]+)\s*=\s*(.+)", line)
            if not match or not sections:
                raise ValueError(f"{path}:{line_number}: can't read `{line}`.")
            sections[-1][2][match.group(1)] = (line_number, _parse_tres_value(path, line_number, match.group(2)))

    if header is None or header.get("type") != "InputActions":
        raise ValueError(f'{path}: expected [gd_resource type="InputActions" ...].')

    sub_resources = {}
    resource = None
    for tag, attributes, properties in sections:
        if tag == "resource":
            resource = properties
            continue
        resource_type = attributes.get("type")
        if resource_type not in _BUILTIN_INPUT_PROPERTIES:
            raise ValueError(f'{path}: sub-resource "{attributes.get("id")}" has the unsupported type {resource_type}.')
        for key, (line_number, _value) in properties.items():
            if key not in _BUILTIN_INPUT_PROPERTIES[resource_type]:
                raise ValueError(f"{path}:{line_number}: {resource_type}.{key} is not supported here.")
        sub_resources[attributes["id"]] = (resource_type, {key: value for key, (_line, value) in properties.items()})
    if resource is None or "actions" not in resource:
        raise ValueError(f"{path}: the [resource] section has no `actions`.")

    def get_sub_resource(reference, expected_types):
        if not (isinstance(reference, tuple) and reference[0] == "SubResource" and reference[1] in sub_resources):
            raise ValueError(f"{path}: {reference} is not a sub-resource of this file.")
        resource_type, properties = sub_resources[reference[1]]
        if resource_type not in expected_types:
            raise ValueError(f'{path}: sub-resource "{reference[1]}" is a {resource_type}, expected {expected_types}.')
        return resource_type, properties

    result = []
    for reference in resource["actions"][1]:
        _type, action = get_sub_resource(reference, ("InputAction",))
        name = action.get("action")
        if not name:
            raise ValueError(f'{path}: InputAction "{reference[1]}" has no `action` name.')
        events = []
        for event_reference in action.get("events", []):
            event_type, properties = get_sub_resource(
                event_reference, ("InputEventKey", "InputEventJoypadButton", "InputEventJoypadMotion")
            )
            if event_type != "InputEventKey" and properties.get("device", 0) != -1:
                raise ValueError(f'{path}: the joypad event "{event_reference[1]}" of "{name}" needs device = -1.')
            if event_type == "InputEventKey":
                if properties.get("keycode", 0) == 0:
                    raise ValueError(f'{path}: the key "{event_reference[1]}" of "{name}" has no keycode.')
                modifiers = [mask for prop, mask in _BUILTIN_INPUT_MODIFIERS if properties.get(prop, False)]
                events.append({"type": "key", "keycode": int(properties["keycode"]), "modifiers": modifiers})
            elif event_type == "InputEventJoypadButton":
                # The saver leaves out default values (button 0, axis 0).
                events.append({"type": "joypad_button", "button": int(properties.get("button_index", 0))})
            else:
                events.append({
                    "type": "joypad_motion",
                    "axis": int(properties.get("axis", 0)),
                    "axis_value": float(properties.get("axis_value", 0.0)),
                })
        result.append((name, events))
    return result


def make_builtin_input_actions(target, source, env):
    """Generates the table of Godot's built-in input actions (InputMap::get_builtins()) from
    builtin_input_actions.tres, an InputActions resource in Godot's text resource format. The file has
    no comments, so it can be opened in the editor's Inspector and saved again unchanged; the build
    reads its InputAction sub-resources and their `events` (InputEventKey, InputEventJoypadButton,
    InputEventJoypadMotion). It is not loaded at runtime and not exported.
    An action name with a feature suffix (`ui_close_dialog.macos`) replaces the action of the plain
    name when OS.has_feature() is true for the tag, like the `input/ui_*.<tag>` project settings."""
    events = []
    actions = []
    for name, action_events in _parse_builtin_input_actions_tres(str(source[0])):
        first = len(events)
        for event in action_events:
            if event["type"] == "key":
                key = " | ".join(
                    [f"Key(0x{event['keycode']:x})"] + ["KeyModifierMask::" + m for m in event["modifiers"]]
                )
                events.append(
                    f"{{ BuiltinInputActions::EVENT_KEY, {key}, JoyButton::INVALID, JoyAxis::INVALID, 0.0f }}"
                )
            elif event["type"] == "joypad_button":
                events.append(
                    f"{{ BuiltinInputActions::EVENT_JOYPAD_BUTTON, Key::NONE, JoyButton({event['button']}), JoyAxis::INVALID, 0.0f }}"
                )
            else:
                events.append(
                    f"{{ BuiltinInputActions::EVENT_JOYPAD_MOTION, Key::NONE, JoyButton::INVALID, JoyAxis({event['axis']}), {event['axis_value']!r}f }}"
                )
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
