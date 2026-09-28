#!/bin/sh
set -eu

printf 'AILEE build hash: %s\n' "$(cat /opt/ailee/BUILD_HASH)"
exec "$@"
