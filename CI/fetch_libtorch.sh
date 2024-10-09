#!/bin/sh

release=$1
platform=$2
ARCHIVE_URL="https://github.com/smanolloff/vcmi-libtorch-builds/releases/download/$release/vcmi-libtorch-$platform.txz"

curl -fL "$ARCHIVE_URL" | tar -xJ -C AI/MMAI

[ -d AI/MMAI/libtorch ] || { ls -lah AI/MMAI; echo "failed extract libtorch"; exit 1; }
