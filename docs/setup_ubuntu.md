# VCMI setup guide (Ubuntu)

> [!WARNING]
> This guide is part of the
> [vcmi-gym setup guide](https://github.com/smanolloff/vcmi-gym/blob/main/doc/setup_ubuntu.md)
> and should be followed only as part of the instructions there.

The setup guide below is tested on Ubuntu 22.04 with Python 3.10.12.

It also works on Debian 12.5 with python3.11-dev installed instead (make sure
to replace 3.10 with 3.11 when following the steps below).

All commands in this tutorial must be executed from the VCMI root folder
<br>(e.g. `/home/yourname/vcmi-gym/vcmi`).

### Set up dependencies

```bash
$ sudo apt update
$ sudo apt install cmake g++ libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
    libsdl2-mixer-dev zlib1g-dev libavformat-dev libswscale-dev libboost-dev \
    libboost-filesystem-dev libboost-system-dev libboost-thread-dev libboost-program-options-dev \
    libboost-locale-dev qtbase5-dev libtbb-dev libluajit-5.1-dev qttools5-dev \
    libsqlite3-dev pybind11-dev python3.10-dev ccache
```

### Compile VCMI

Linux binaries use CXX11 ABI. As such, they are incompatible with non-CXX11
ABI binaries. Unfortunately, the `libtorch` shipped with Python's `torch`
package is non-CXX11 by default and cannot be linked to VCMI.

There are two workarounds to this issue:

1. Compile VCMI with `-D ENABLE_LIBTORCH=1` *after* installing the CPU-only
cxx11 ABI torch package (see `requirements.txt` in vcmi-gym). Recommended
if you want to use VCMI for playing against pre-trained AI models, OR for
training new AI models on CPU only.
1. Compile VCMI with `-D ENABLE_LIBTORCH=0`. Recommended if you want to train
new AI models on CPU or GPU. VCMI itself will not be able to load pre-trained
models.

```bash
$ cmake -S . -B rel -Wno-dev \
    -D CMAKE_BUILD_TYPE=Release \
    -D ENABLE_SINGLE_APP_BUILD=1 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_GYMCLIENT=1 \
    -D ENABLE_DEV_BUILD=0 \
    -D ENABLE_LIBTORCH=1 \
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
      -D ENABLE_GYMCLIENT=1 \
      -D ENABLE_DEV_BUILD=1 \
      -D ENABLE_LIBTORCH=1 \
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
$ mkdir "${XDG_CONFIG_HOME:-$HOME/.config}/vcmi"
$ ln -s "$PWD"/gymclient/{settings,modSettings,persistentStorage}.json "${XDG_CONFIG_HOME:-$HOME/.config}/vcmi"
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
