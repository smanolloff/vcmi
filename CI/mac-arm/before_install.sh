#!/usr/bin/env bash

DEPS_FILENAME=dependencies-mac-arm
. CI/mac/before_install.sh

if [ "$MMAI" = "1" ]; then
  . CI/fetch_libtorch.sh "v1.0.0" "mac-arm64"
fi
