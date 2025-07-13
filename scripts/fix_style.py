#!/usr/bin/env python3

import os
import subprocess
import sys
import argparse

os.chdir(os.path.dirname(__file__) + "/..")

def recursive_file_list(path):
	result = []
	for dirpath, _, filenames in os.walk(path):
		result += [os.path.join(dirpath, filename) for filename in filenames]
	return result

IGNORE_FILES = [
]
IGNORE_DIRS = [
	"src/game/generated",
	"src/rust-bridge"
]
def filter_ignored(filenames):
	result = []
	for filename in filenames:
		real_filename = os.path.realpath(filename)
		if real_filename not in [os.path.realpath(ignore_file) for ignore_file in IGNORE_FILES] \
			and not any(real_filename.startswith(os.path.realpath(subdir) + os.path.sep) for subdir in IGNORE_DIRS):
			result.append(filename)

	return result

def filter_cpp(filenames):
	return [filename for filename in filenames
		if any(filename.endswith(ext) for ext in ".c .cpp .h".split())]

def find_clang_format(version):
	# Common paths for clang-format on different systems
	common_paths = [
		"clang-format",
		f"clang-format-{version}",
		f"/opt/clang-format-static/clang-format-{version}",
		# Windows Visual Studio paths
		r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin\clang-format.exe",
		r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\bin\clang-format.exe",
		r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\bin\clang-format.exe",
		r"C:\Program Files\LLVM\bin\clang-format.exe",
	]
	
	# First try to find the exact version
	for binary in common_paths:
		try:
			out = subprocess.check_output([binary, "--version"])
			version_output = out.decode("utf-8")
			if f"clang-format version {version}." in version_output:
				return binary
		except (FileNotFoundError, subprocess.CalledProcessError):
			continue
	
	# If exact version not found, try to find any working clang-format
	print(f"Warning: clang-format version {version} not found, trying any available version...")
	for binary in common_paths:
		try:
			out = subprocess.check_output([binary, "--version"])
			version_output = out.decode("utf-8")
			print(f"Found: {version_output.strip()}")
			return binary
		except (FileNotFoundError, subprocess.CalledProcessError):
			continue
	
	print(f"Found no clang-format (looking for version {version})")
	sys.exit(-1)

clang_format_bin = find_clang_format(10)

def reformat(filenames):
	for filename in filenames:
		with open(filename, 'r+b') as f:
			try:
				f.seek(-1, os.SEEK_END)
				if f.read(1) != b'\n':
					f.write(b'\n')
			except OSError:
				f.seek(0)
	subprocess.check_call([clang_format_bin, "-i"] + filenames)

def warn(filenames):
	clang = subprocess.call([clang_format_bin, "-Werror", "--dry-run"] + filenames)
	newline = 0
	for filename in filenames:
		with open(filename, 'rb') as f:
			try:
				f.seek(-1, os.SEEK_END)
				if f.read(1) != b'\n':
					print(filename + ": error: missing newline at EOF", file=sys.stderr)
					newline = 1
			except OSError:
				f.seek(0)
	return clang or newline

def main():
	p = argparse.ArgumentParser(description="Check and fix style of changed files")
	p.add_argument("-n", "--dry-run", action="store_true", help="Don't fix, only warn")
	args = p.parse_args()
	filenames = filter_ignored(filter_cpp(recursive_file_list("src")))
	if not args.dry_run:
		reformat(filenames)
	else:
		sys.exit(warn(filenames))

if __name__ == "__main__":
	main()
