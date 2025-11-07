#!/bin/sh

MMAI_DEPS=$1

set -x
dst=AI/MMAI/third-party
mkdir -p $dst
platform=$2

flags=("-DENABLE_MMAI=ON")

case "$MMAI_DEPS" in
*executorch*)
    ARCHIVE_URL="https://github.com/smanolloff/vcmi-executorch-builds/releases/download/$MMAI_DEPS"
    flags+=("-DMMAI_EXECUTORCH_PATH=$dst/executorch")
    ;;
*libtorch*)
    ARCHIVE_URL="https://github.com/smanolloff/vcmi-libtorch-builds/releases/download/$MMAI_DEPS"
    flags+=("-DMMAI_LIBTORCH_PATH=$dst/libtorch")
    ;;
*)
    echo "Unknown value for mmai_deps: '$MMAI_DEPS'";
    exit 1;
    ;;
esac

curl -o download.zip -fL "$ARCHIVE_URL"
openssl dgst -sha256 download.zip
unzip -q download.zip -d $dst
[ -d $dst/$libtype ] || { ls -lah $dst; echo "failed to extract $libtype"; exit 1; }

echo "MMAI_FLAGS=${flags[@]}" >> "$GITHUB_ENV"

#
# Download models
#

# Sadly, jq fails because of the JSON comments => use grep+awk
attacker=$(grep '"attacker"' Mods/MMAI/config/mmai-settings.json | awk -F '"' '{print $4}')
defender=$(grep '"defender"' Mods/MMAI/config/mmai-settings.json | awk -F '"' '{print $4}')
baseurl="https://github.com/smanolloff/vcmi-MMAI-mod/releases/download/v0.2"

cd Mods/MMAI/models
curl -LfO "$baseurl/$(basename $attacker)"
curl -LfO "$baseurl/$(basename $defender)"
