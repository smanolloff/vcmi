# VCMI setup guide (MacOS)

> [!WARNING]
> This guide is part of the
> [vcmi-gym setup guide](https://github.com/smanolloff/vcmi-gym/blob/main/doc/setup_macos.md)
> and should be followed only as part of the instructions there.

The setup guide below is tested on MacOS 14.0 Sonoma.

All commands in this tutorial must be executed from the VCMI root folder
<br>(e.g. `/home/yourname/vcmi-gym/vcmi_gym/envs/v0/vcmi`).

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
    --profile:host=default
```

In case the above command fails with a qt-related error, apply
[this](https://codereview.qt-project.org/c/qt/qtbase/+/503172/1/mkspecs/features/toolchain.prf#295)
patch to `~/.conan/data/qt/5.15.11/_/_/source/qt5/qtbase/mkspecs/features/mac/toolchain.prf`
(exact qt version may vary), then repeat the `conan install` command.

### Compile VCMI

```bash
$ cmake --fresh -S . -B build -Wno-dev \
        -D CMAKE_TOOLCHAIN_FILE=conan-generated/conan_toolchain.cmake \
        -D CMAKE_BUILD_TYPE=Debug \
        -D ENABLE_SINGLE_APP_BUILD=1 \
        -D ENABLE_CCACHE=1 \
        -D ENABLE_NULLKILLER_AI=0 \
        -D ENABLE_LAUNCHER=0 \
        -D ENABLE_MYCLIENT_BUILD=1 \
        -D ENABLE_DEV_BUILD=1 \
        -D CMAKE_EXPORT_COMPILE_COMMANDS=1

$ cmake --build rel/
```

_(Optional)_ Compile a debug binary:

```bash
$ cmake -S . -B build -Wno-dev \
    -D CMAKE_TOOLCHAIN_FILE=conan-generated/conan_toolchain.cmake \
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
prevent disk writes.
Instead, symbolic links must be manually created to files which contain
the appropriate settings for vcmi-gym:

```bash
$ ln -s "$PWD"/myclient/{settings,modSettings}.json "$HOME/Library/Application Support/vcmi/config"
```

### Manual test

Start a new game on the specified map (with GUI):

```bash
$ rel/bin/myclient-gui --map ai/P1.vmap
```

### Benchmark

```
$ rel/bin/myclient-headless --map ai/generated/B001.vmap --loglevel-ai error --benchmark
```

This will run the game in headless mode and directly engage in a battle
where the attacker is a simple RNG AI and the defender - VCMI's built-in
scripted AI. When the battle ends, it will be immediately restarted and
"played" again.

Performance will be measured and stats will occasionally be reported until
the process is interrupted. The output looks something like this:

```
Benchmark:
* Map: ai/generated/B001.vmap
* Attacker AI: MMAI_USER
* Defender AI: StupidAI

-  steps/s: 159    resets/s: 6.86  
-  steps/s: 155    resets/s: 6.70  
-  steps/s: 156    resets/s: 7.01  
-  steps/s: 152    resets/s: 6.62  
```

> [!TIP]
> If you also compiled the debug binary, you can benchmark it by running
> `build/bin/myclient-headless` with the same arguments and
> observe the performance difference for yourself.

### Loading AI models

Launch the game and replace the default VCMI scripted AI with a pre-trained *real* AI:

```bash
rel/bin/myclient-gui --map ai/P1.vmap --attacker-ai MMAI_MODEL --attacker-model /path/to/model.zip
```

where `/path/to/model` is your pre-trained AI model. If you don't have such a
model, you will need to train it first 🤓 That's what the
[vcmi-gym](https://github.com/smanolloff/vcmi-gym)
project is for, so make sure to check it out.
