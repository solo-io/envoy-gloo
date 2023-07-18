#!/bin/sh
set -eu

# CHECK_ON_MINOR_UPDATE we dont mimick upstream that close as we have smaller scope 
# of what we support but we should still check on an upgrade
# note that in gloo we overwrite this anyways

if "${DISABLE_CORE_DUMPS:-false}" ; then
  ulimit -c 0
fi

exec /usr/local/bin/envoyinit "$@"

