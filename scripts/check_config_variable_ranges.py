#!/usr/bin/env python3
"""Validate config variable ranges and help text lengths statically.

The MACRO_CONFIG_* macros in src/engine/shared/config.cpp assert at startup that
the minimum is below the maximum, that the default lies within the range, and
that the generated help text is not truncated. Those assertions only fire in
debug builds, one variable at a time. This script reports every violation at
once so they can be fixed before running the client.
"""

import os
import re
import sys

os.chdir(os.path.dirname(__file__) + "/..")

CONFIG_FILES = [
	"src/engine/shared/config_variables.h",
	"src/engine/shared/config_variables_entity.h",
]

# sizeof(aHelp) - UTF8_BYTE_LENGTH - 1, matching the dbg_assert in config.cpp.
HELP_LIMIT = 512 - 4 - 1

# Bounds that are named constants rather than literals.
CONSTANTS = {
	"SERVER_MAX_CLIENTS": 64,
	"SERVERINFO_LEVEL_MIN": 0,
	"SERVERINFO_LEVEL_MAX": 2,
	"CountryCode::DEFAULT": -1,
	"CountryCode::MINIMUM": -999,
	"CountryCode::MAXIMUM": 999,
}


def split_arguments(text):
	"""Split macro arguments on commas outside of parens and string literals."""
	arguments = []
	current = []
	depth = 0
	in_string = False
	escaped = False
	for char in text:
		if in_string:
			current.append(char)
			if escaped:
				escaped = False
			elif char == "\\":
				escaped = True
			elif char == '"':
				in_string = False
			continue
		if char == '"':
			in_string = True
			current.append(char)
		elif char in "([":
			depth += 1
			current.append(char)
		elif char in ")]":
			depth -= 1
			current.append(char)
		elif char == "," and depth == 0:
			arguments.append("".join(current).strip())
			current = []
		else:
			current.append(char)
	arguments.append("".join(current).strip())
	return arguments


def parse_int(token):
	"""Return the integer value of a bound, or None if it cannot be resolved."""
	if token in CONSTANTS:
		return CONSTANTS[token]
	try:
		return int(token, 0)
	except ValueError:
		return None


def parse_string(token):
	"""Return the contents of a string literal, or None if it is not one."""
	if len(token) >= 2 and token.startswith('"') and token.endswith('"'):
		return token[1:-1]
	return None


def parse_config_variables(filename):
	"""Yield (kind, line number, arguments) for each MACRO_CONFIG_* invocation."""
	with open(filename, "r", encoding="utf-8") as file:
		for number, line in enumerate(file, start=1):
			# The trailing group allows an end-of-line comment after the macro.
			match = re.match(r"^MACRO_CONFIG_(INT|COL|STR)\((.*)\)\s*(?://.*)?$", line)
			if match:
				yield match.group(1), number, split_arguments(match.group(2))


def check_int(script_name, arguments, report):
	default = parse_int(arguments[2])
	minimum = parse_int(arguments[3])
	maximum = parse_int(arguments[4])
	if default is None or minimum is None or maximum is None:
		report(f"'{script_name}' has bounds that could not be resolved, add them to CONSTANTS.")
		return
	if not (minimum == 0 or maximum == 0 or minimum < maximum):
		report(f"'{script_name}': minimum ({minimum}) must be less than maximum ({maximum}).")
	if not ((minimum == 0 or default >= minimum) and (maximum == 0 or default <= maximum)):
		report(f"'{script_name}': default ({default}) must be in range of minimum ({minimum}) and maximum ({maximum}).")
	description = parse_string(arguments[6])
	if description is None:
		return
	if minimum == 0 and maximum == 0:
		help_text = f"{description} (default: {default})"
	elif maximum == 0:
		help_text = f"{description} (default: {default}, min: {minimum})"
	else:
		help_text = f"{description} (default: {default}, min: {minimum}, max: {maximum})"
	check_help_text(script_name, help_text, report)


def check_str(script_name, arguments, report):
	length = parse_int(arguments[2])
	default = parse_string(arguments[3])
	description = parse_string(arguments[5])
	if length is None or default is None or description is None:
		return
	check_help_text(script_name, f'{description} (default: "{default}", max length: {length - 1})', report)


def check_help_text(script_name, help_text, report):
	size = len(help_text.encode("utf-8"))
	if size >= HELP_LIMIT:
		report(f"'{script_name}': help text is {size} bytes, which exceeds the limit of {HELP_LIMIT} and would be truncated.")


def main():
	errors = []
	total = 0
	for filename in CONFIG_FILES:
		if not os.path.exists(filename):
			continue
		for kind, number, arguments in parse_config_variables(filename):
			total += 1
			script_name = arguments[1]

			def report(message, filename=filename, number=number):
				errors.append(f"{filename}:{number}: {message}")

			if kind == "INT" and len(arguments) >= 7:
				check_int(script_name, arguments, report)
			elif kind == "STR" and len(arguments) >= 6:
				check_str(script_name, arguments, report)

	for error in errors:
		print(error)
	if errors:
		print(f"Error: {len(errors)} config variable(s) would fail the assertions in config.cpp.")
		return 1
	print(f"Success: All {total} config variables have valid ranges and help texts.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
