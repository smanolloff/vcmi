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
    libsqlite3-dev liblzma-dev python3.10-dev ccache
```

### Compile VCMI

##### Examples

* Compilation options for regular gameplay support (running VCMI as a standalone program):

```bash
$ cmake -S . -B rel -Wno-dev \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=0 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_ML=0 \
    -D ENABLE_MMAI=1

$ cmake --build rel/
```

* Compilation options for ML training support (loading VCMI lib from vcmi-gym):

```bash
$ cmake -S . -B rel -Wno-dev \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=0 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_ML=1 \
    -D ENABLE_MMAI=1

$ cmake --build rel/
```

_(Optional)_ For a debug build with IDE support and tests:

```bash
$ cmake -S . -B build -Wno-dev \
    -D CMAKE_BUILD_TYPE=Debug \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=1 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_ML=1 \
    -D ENABLE_MMAI=1 \
    -D ENABLE_MMAI_TEST=1

$ cmake --build build/
```

### HOMM3 data files

Please refer to the [HOMM3 data files](./setup_macos.md#homm3-data-files)
instructions for MacOS.

### User configs

VCMI normally creates those files by itself, but this version is modified to
prevent disk writes and the "user data" dir is set to a local
"data" directory (instead of "~/.config/vcmi").
To seed it with initial configs:

```bash
mkdir -p data/config
cp ML/configs/*.json data/config
```

If you need to modify game configs, you can directly edit the corresponding file
in `./data/config/`.

### Manual test

Plvease refer to the [Manual test](./setup_macos.md#manual-test)
instructions for MacOS.

### Benchmark

Please refer to the [Benchmark](./setup_macos.md#benchmark)
instructions for MacOS.

### Loading AI models

Please refer to the [Loading AI models](./setup_macos.md#loading-ai-models)
instructions for MacOS.
