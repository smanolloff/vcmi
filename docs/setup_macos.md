# VCMI setup guide (MacOS)

> [!WARNING]
> This guide is part of the
> [vcmi-gym setup guide](https://github.com/smanolloff/vcmi-gym/blob/main/doc/setup_macos.md)
> and should be followed only as part of the instructions there.

The setup guide below is tested on MacOS 14.0 Sonoma.

All commands in this tutorial must be executed from the VCMI root folder
<br>(e.g. `/home/yourname/vcmi-gym/vcmi`).

### Set up dependencies

```bash
$ brew install --cask cmake
$ pip3 install conan~=1.6
$ conan profile new default --detect
$ conan profile update settings.compiler.cppstd=17 default
```

NOTE: the command the below *might fail*, in which case follow the special
instruction afterwards:

```bash
$ conan install . \
    --install-folder=conan-generated \
    --no-imports \
    --build=missing \
    --profile:build=default \
    --profile:host=default \
    --require-override "qt/5.15.11@"
```

In case the above command fails with a qt-related error, apply
[this](https://codereview.qt-project.org/c/qt/qtbase/+/503172/1/mkspecs/features/toolchain.prf#295)
patch to `~/.conan/data/qt/5.15.11/_/_/source/qt5/qtbase/mkspecs/features/mac/toolchain.prf`
(exact qt version may vary), then repeat the `conan install` command.

To be able to load AI models in VCMI, libtorch is needed. VCMI should use the
same libtorch as the one used by vcmi-gym and installed by pip. Create this
symlink (python version may vary):

```bash
$ ln -s ../../../../../.venv/lib/python3.10/site-packages/torch AI/MMAI/BAI/model/libtorch
```

### Compile VCMI

Before proceeding, make sure you have `libtorch` properly set up.
* if you are building VCMI for gameplay only, set `MMAI_LIBTORCH_PATH` to a
path containing downloaded torch libraries downloaded
from [here](https://github.com/smanolloff/vcmi-libtorch-builds/releases).
* if you are building VCMI for training new models, set
`MMAI_LIBTORCH_PATH=/path/to/libtorch` where the path contains torch
libraries downloaded by `pip` as part of
[vcmi-gym](https://github.com/smanolloff/vcmi-gym)'s setup.

##### Examples

* Compilation options for regular gameplay support (running VCMI as a standalone program):

```bash
$ cmake -S . -B rel -Wno-dev \
    -D CMAKE_TOOLCHAIN_FILE=conan-generated/conan_toolchain.cmake \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=0 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_ML=0 \
    -D ENABLE_MMAI=1 \
    -D MMAI_LIBTORCH_PATH=/path/to/libtorch  # contains lib/ and include/

$ cmake --build rel/
```

* Compilation options for ML training support (loading VCMI lib from vcmi-gym):

```bash
$ cmake -S . -B rel -Wno-dev \
    -D CMAKE_TOOLCHAIN_FILE=conan-generated/conan_toolchain.cmake \
    -D CMAKE_BUILD_TYPE=Release \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=0 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_ML=1 \
    -D ENABLE_MMAI=1 \
    -D MMAI_LIBTORCH_PATH=/path/to/libtorch  # contains lib/ and include/

$ cmake --build rel/
```

_(Optional)_ For a debug build with IDE support and tests:

```bash
$ cmake -S . -B build -Wno-dev \
    -D CMAKE_TOOLCHAIN_FILE=conan-generated/conan_toolchain.cmake \
    -D CMAKE_BUILD_TYPE=Debug \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=1 \
    -D ENABLE_CCACHE=1 \
    -D ENABLE_NULLKILLER_AI=0 \
    -D ENABLE_LAUNCHER=0 \
    -D ENABLE_ML=1 \
    -D ENABLE_MMAI=1 \
    -D ENABLE_MMAI_TEST=1 \
    -D MMAI_LIBTORCH_PATH=/path/to/libtorch

$ cmake --build build/
```

### HOMM3 data files

Provide the directory with the preinstalled Heroes3 game to `vcmibuilder`:

```bash
$ ./vcmibuilder --data /path/to/heroes3
```

Note: the expected directory structure of that directory is as follows:

```bash
/path/to/heroes3
├── Data
├── EULA
├── Games
├── Maps
├── Mp3
├── random_maps
└── ...
```

### User configs

VCMI normally creates those files by itself, but this version is modified to
prevent disk writes at runtime and the "user data" dir is set to a local
"data" directory (instead of "~/Library/Application Support/vcmi").

If you need to modify game configs, you can directly edit the corresponding file
in `./data/config/`.

### Manual test

Start a new game on the specified map (with GUI):

```bash
$ rel/bin/mlclient-cli --map gym/A1.vmap
```

### Benchmark

```
$ rel/bin/mlclient-cli --headless --map gym/A1.vmap --loglevel-ai error --benchmark
```

This will run the game in headless mode and directly engage in a battle
where the attacker is a simple RNG AI and the defender - VCMI's built-in
scripted AI. When the battle ends, it will be immediately restarted and
"played" again.

Performance will be measured and stats will occasionally be reported until
the process is interrupted. The output looks something like this:

```
Benchmark:
* Map: gym/A1.vmap
* Attacker AI: MMAI_USER
* Defender AI: StupidAI

-  steps/s: 159    resets/s: 6.86  
-  steps/s: 155    resets/s: 6.70  
-  steps/s: 156    resets/s: 7.01  
-  steps/s: 152    resets/s: 6.62  
```

> [!TIP]
> If you also compiled the debug binary, you can benchmark it by running
> `build/bin/mlclient-cli` with the same arguments and
> observe the performance difference for yourself.

### Loading AI models

Launch the game and replace the default VCMI scripted AI with a pre-trained *real* AI:

```bash
rel/bin/mlclient-cli --map gym/A1.vmap --blue-ai MMAI_MODEL --blue-model /path/to/model.pt
```

where `/path/to/model.pt` is a pre-trained Torch JIT model. If you don't have
such a model, you will need to train it first 🤓 That's what the
[vcmi-gym](https://github.com/smanolloff/vcmi-gym)
project is for, so make sure to check it out.
