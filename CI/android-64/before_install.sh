#!/usr/bin/env bash

DEPS_FILENAME=dependencies-android-64
. CI/android/before_install.sh

if [ "$MMAI" = "1" ]; then
  . CI/fetch_libtorch.sh "v1.0.0" "android-arm64"
fi
