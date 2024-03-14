# VCMI setup guide (Ubuntu)

This guide is for installing the modified VCMI which is intended be used as an RL environment for the [vcmi-gym](https://github.com/smanolloff/vcmi-gym) project.

The steps below were tested on a Ubuntu 22.04.

## Prerequisites

Checkout vcmi as a submodule of vcmi-gym as per **Step 1.** of the [vcmi-gym setup guide](https://github.com/smanolloff/vcmi-gym/blob/main/doc/setup_ubuntu.md) 

## Setup steps

> [!IMPORTANT]
> All commands in this tutorial must be executed from the VCMI root folder
> <br>(e.g. `/home/yourname/projects/vcmi-gym/vcmi_gym/envs/v0/vcmi`)

1. Compile VCMI

    1. Install precompiled dependencies:

        ```bash
        $ sudo apt-get install cmake g++ libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev \
            libsdl2-mixer-dev zlib1g-dev libavformat-dev libswscale-dev libboost-dev \
            libboost-filesystem-dev libboost-system-dev libboost-thread-dev libboost-program-options-dev \
            libboost-locale-dev qtbase5-dev libtbb-dev libluajit-5.1-dev qttools5-dev
        ```

    1. Build

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


    1. _(Optional)_ Build a debug binary
        
        Do this if you want to compile a binary which runs slower and allows debugging.

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

1. HOMM3 data files

    These files are not included in VCMI (or vcmi-gym). You will need an existing HOMM3 instllation to perform this step.
    
    Follow "Step 2: Installing Heroes III data files" from the [official VCMI docs](https://github.com/vcmi/vcmi/blob/1.3.2/docs/players/Installation_Linux.md#install-data-using-vcmibuilder-script).

    **tl; dr** copy the data files to `h3` (directory structure shown below), then run `$ ./vcmibuilder --data h3 --validate`.
    ```bash
    $ tree -dL 1 h3
    h3
    ├── Data
    ├── EULA
    ├── Games
    ├── Maps
    ├── Mp3
    ├── random_maps
    └── __redist

    $ ./vcmibuilder --data game/h3
    ```

1. User configs

    VCMI normally creates those files by itself, but this version is modified to prevent disk writes.

    ```bash
    $ ln -st "${XDG_CONFIG_HOME:-$HOME/.config}/vcmi" myclient/{settings,modSettings}.json
    ```

1. Test installation

    1. Start a new game on the specified map (with GUI)

        ```bash
        $ build/bin/myclient-gui --map ai/P1.vmap
        ```

    1. Benchmark performance (no GUI)

        This will start the game in headless mode (no GUI) and directly engage in a battle where the attacker is a simple RNG AI and the defender - the default VCMI AI.

        When the battle ends, it is immediately restarted and "played" again

        ```
        $ build/bin/myclient-headless --gymdir ~/Projects/vcmi-gym --map ai/generated/B001.vmap --loglevel-ai error --benchmark
        ```

        Performance will be measured and stats will be occasionally reported. The output looks something like this:

        ```
        Benchmark:
        * Map: ai/generated/B001.vmap
        * Attacker AI: MMAI_USER
        * Defender AI: StupidAI
        
        -  steps/s: 16     resets/s: 0.75
        -  steps/s: 17     resets/s: 0.74
        -  ...etc.
        ```


        #
        # NOTE: the same command can be used with the the "release" binary (if compiled) to compare the performance
        # (typically ~10 times faster)
        #
