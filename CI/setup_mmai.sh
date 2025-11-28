#!/bin/sh

set -x

flags=("-DENABLE_MMAI=ON")

echo "MMAI_FLAGS=${flags[@]}" >> "$GITHUB_ENV"

# Sadly, jq fails because of the JSON comments => use grep+awk
attacker=$(grep '"attacker"' Mods/MMAI/config/mmai-settings.json | awk -F '"' '{print $4}')
defender=$(grep '"defender"' Mods/MMAI/config/mmai-settings.json | awk -F '"' '{print $4}')
baseurl="https://github.com/smanolloff/vcmi-MMAI-mod/releases/download/v0.3"

cd Mods/MMAI/models
curl -LfO "$baseurl/$(basename $attacker)"
curl -LfO "$baseurl/$(basename $defender)"
