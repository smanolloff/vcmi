#!/bin/sh

set -eux

. CI/linux/before_install.sh

FILE_NAME=vcmi-libtorch-linux-x64.txz
DOWNLOAD_URL="https://github.com/smanolloff/vcmi-libtorch-builds/releases/download/v0.0.0-test/$FILE_NAME"
curl -fL libtorch.txz "$DOWNLOAD_URL" | tar -xz -

[ -d libtorch ] || { ls -lah; echo "failed extract libtorch"; exit 1; }
