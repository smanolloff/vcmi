#!/usr/bin/env bash

if [ "$MMAI" = "1" ]; then
  . CI/fetch_libtorch.sh "v1.0.0" "ios-arm64"
fi

echo DEVELOPER_DIR=/Applications/Xcode_14.2.app >> $GITHUB_ENV

. CI/install_conan_dependencies.sh "dependencies-ios"
