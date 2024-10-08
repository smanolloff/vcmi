#!/usr/bin/env bash

if [ "$MMAI" = "1" ]; then
  . CI/fetch_libtorch.sh "v1.0" "macos-arm64"
fi

DEPS_FILENAME=dependencies-mac-arm
. CI/mac/before_install.sh
