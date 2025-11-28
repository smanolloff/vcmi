#!/usr/bin/env bash

set -x

RELEASE_TAG="2025-11-22"
FILENAME="$1.tgz"
DOWNLOAD_URL="https://github.com/smanolloff/vcmi-dependencies/releases/download/$RELEASE_TAG/$FILENAME"

downloadedFile="$RUNNER_TEMP/$FILENAME"
curl -Lo "$downloadedFile" "$DOWNLOAD_URL"
conan cache restore "$downloadedFile"
