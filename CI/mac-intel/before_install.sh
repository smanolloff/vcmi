#!/usr/bin/env bash

if [ "$MMAI" = "1" ]; then
  . CI/fetch_libtorch.sh "v1.0" "macos-x64"
fi

DEPS_FILENAME=dependencies-mac-intel
. CI/mac/before_install.sh
