# VCMI setup guide (Ubuntu)

> [!WARNING]
> This guide is part of the
> [vcmi-gym setup guide](https://github.com/smanolloff/vcmi-gym/blob/main/doc/setup_ubuntu.md)
> and should be followed only as part of the instructions there.

The setup guide below is tested on Ubuntu 22.04 with Python 3.10.12.

All commands in this tutorial must be executed from the VCMI root folder
<br>(e.g. `/home/yourname/vcmi-gym/vcmi_gym/envs/v0/vcmi`).

### Set up dependencies

```bash
$ sudo apt-get install cmake g++ libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
    libsdl2-mixer-dev zlib1g-dev libavformat-dev libswscale-dev libboost-dev \
    libboost-filesystem-dev libboost-system-dev libboost-thread-dev libboost-program-options-dev \
    libboost-locale-dev qtbase5-dev libtbb-dev libluajit-5.1-dev qttools5-dev pybind11-dev python3.10-dev
```

```
$ curl -o libtorch.zip https://download.pytorch.org/libtorch/cpu/libtorch-cxx11-abi-static-with-deps-2.1.2%2Bcpu.zip
$ unzip -d myclient libtorch.zip
$ rm libtorch.zip
```

### Compile VCMI

```bash
$ cmake -S . -B rel -Wno-dev \
    -D CMAKE_BUILD_TYPE=Release \
    -D ENABLE_SINGLE_APP_BUILD=1 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_MYCLIENT_BUILD=1 \
    -D ENABLE_DEV_BUILD=0 \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=0

$ cmake --build rel/
```

_(Optional)_ Compile a debug binary:

```bash
$ cmake -S . -B build -Wno-dev \
    -D CMAKE_BUILD_TYPE=Debug \
    -D ENABLE_SINGLE_APP_BUILD=1 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_MYCLIENT_BUILD=1 \
    -D ENABLE_DEV_BUILD=1 \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=1

$ cmake --build build/
```

### HOMM3 data files

Please refer to the [HOMM3 data files](./setup_macos.md#homm3-data-files)
instructions for MacOS.

### User configs

VCMI normally creates those files by itself, but this version is modified to
prevent disk writes.
Instead, symbolic links must be manually created to files which contain
the appropriate settings for vcmi-gym:

```bash
$ ln -s "$PWD"/myclient/{settings,modSettings}.json "${XDG_CONFIG_HOME:-$HOME/.config}/vcmi"
```

### Manual test

Please refer to the [Manual test](./setup_macos.md#manual-test)
instructions for MacOS.

### Benchmark

Please refer to the [Benchmark](./setup_macos.md#benchmark)
instructions for MacOS.

### Loading AI models

Please refer to the [Loading AI models](./setup_macos.md#loading-ai-models)
instructions for MacOS.
