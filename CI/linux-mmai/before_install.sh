#!/bin/sh

set -eux

. CI/linux/before_install.sh

ARCHIVE_RELEASE=v0.0.0-test
ARCHIVE_NAME=vcmi-libtorch-linux-x64.txz

ARCHIVE_URL="https://github.com/smanolloff/vcmi-libtorch-builds/releases/download/$ARCHIVE_RELEASE/$ARCHIVE_NAME"
curl -fL "$ARCHIVE_URL" | tar -xz -

[ -d libtorch ] || { ls -lah; echo "failed extract libtorch"; exit 1; }
