#!/bin/sh

set -eux

. CI/linux/before_install.sh

FILE_NAME=vcmi-libtorch-v2.4.1-ios-arm64-v0.0.0-test.txz
DOWNLOAD_URL="https://github.com/smanolloff/vcmi-libtorch-builds/releases/download/v1.0/$FILENAME"
curl -fLo libtorch.txz.zip "$DOWNLOAD_URL"
unzip -p libtorch.txz.zip | tar -xz -

[ -d libtorch ] || { ls -lah; echo "failed extract libtorch"; exit 1; }
