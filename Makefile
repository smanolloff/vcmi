MAKEFLAGS += --always-make
SHELL := bash
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
	tag=$$(git -C dependencies tag --points-at HEAD); echo "Dependencies tag: $$tag"; \
	curl -LO https://github.com/vcmi/vcmi-dependencies/releases/download/$$(git -C dependencies tag --points-at HEAD)/dependencies-mac-arm.txz
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

vastai-build:
	cmake -S . -B rel -Wno-dev \
		-D CMAKE_BUILD_TYPE=Release \
		-D CMAKE_EXPORT_COMPILE_COMMANDS=0 \
		-D ENABLE_CCACHE=1 \
		-D ENABLE_NULLKILLER2_AI=0 \
		-D ENABLE_LAUNCHER=0 \
		-D ENABLE_VIDEO=0 \
		-D ENABLE_DISCORD=0 \
		-D ENABLE_TRANSLATIONS=0 \
		-D ENABLE_ML=1 \
		-D ENABLE_MMAI=1 \
		-D ONNXRUNTIME_ROOT=/opt/onnxruntime
	cmake --build rel -- -j$$(nproc)
