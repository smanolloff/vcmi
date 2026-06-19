MAKEFLAGS += --always-make
SHELL := bash
.ONESHELL:
.SHELLFLAGS := -euxo pipefail -c

help:
	@echo "Tasks:"
	@grep -E '^[a-zA-Z][a-zA-Z0-9_.-]*:.*?' $(MAKEFILE_LIST) \
		| awk -F':' '{printf "  \033[36m%-20s\033[0m\n", $$1}' \
		| uniq

_require-cmake:
	which cmake || pip install cmake

dependencies: _require-cmake
dependencies:
	curl -LO https://github.com/vcmi/vcmi-dependencies/releases/download/2026-06-18/dependencies-mac-arm.txz
	conan cache restore dependencies-mac-arm.txz
	conan install . \
		--output-folder=conan-generated-debug \
		--build=missing \
		--profile=dependencies/conan_profiles/macos-arm \
		--profile=dependencies/conan_profiles/base/apple-system \
		-s "&:build_type=Debug"
	conan install . \
		--output-folder=conan-generated \
		--build=missing \
		--profile=dependencies/conan_profiles/macos-arm \
		--profile=dependencies/conan_profiles/base/apple-system \
		-s "&:build_type=Release"

