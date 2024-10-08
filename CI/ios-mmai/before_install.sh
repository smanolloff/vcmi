#!/usr/bin/env bash

set -eux

ARCHIVE_RELEASE=v0.0.0-test
ARCHIVE_NAME=vcmi-libtorch-ios-arm64.txz

ARCHIVE_URL="https://github.com/smanolloff/vcmi-libtorch-builds/releases/download/$ARCHIVE_RELEASE/$ARCHIVE_NAME"
curl -fL "$ARCHIVE_URL" | tar -xJ -C AI/MMAI

[ -d libtorch ] || { ls -lah; echo "failed extract libtorch"; exit 1; }

. CI/ios/before_install.sh
