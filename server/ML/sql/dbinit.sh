#!/bin/bash

set -euxo pipefail

function usage() {
    # XXX: keep the 2 blank lines for better error formatting
    cat <<-EOF


Usage: $0 DB NPOOLS NHEROES SIDE

Example:
    # DB=stats.db NPOOLS=2 NHEROES=4 SIDE=0 (a.k.a. red / attacker):
    $0 stats.db 2 4 0
EOF

    exit 1
}

DB=${1:?"$(usage)"}
NHEROES="${2:?"$(usage)"}"
NPOOLS="${3:?"$(usage)"}"
SIDE="${4:?"$(usage)"}"

function _sqlite3() {
    sqlite3 -batch -noheader -csv "$DB" "$@"
}

function abort() {
    echo "$1" >&2
    exit 1
}

function main() {
    sqldir="$(dirname ${BASH_SOURCE[0]})"

    [ -e "$DB" ] && abort "DB is invalid: already exists: $DB" || :
    [[ "$NPOOLS" =~ ^[0-9]+$ ]] || abort "NPOOLS is invalid: not a number: $NPOOLS"
    [[ "$NHEROES" =~ ^[0-9]+$ ]] || abort "NHEROES is invalid: not a number: $NHEROES"
    [[ "$SIDE" =~ ^[01]$ ]] || abort "NSIDE is invalid: must be 0 or 1, got: $NHEROES"

    sqlite3 "$DB" < "$sqldir/structure.sql"
    sed -e "s/--.*//" \
        -e "1,/\?/s/\?/$NPOOLS/" \
        -e "1,/\?/s/\?/$NHEROES/" \
        -e "1,/\?/s/\?/$SIDE/" \
        "$sqldir/seed.sql" | sqlite3 "$DB"

    echo "Done."
}

main "$@"
