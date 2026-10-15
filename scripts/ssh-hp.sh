#!/bin/sh
D=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DISPLAY=:0 SSH_ASKPASS="$D/ask.sh" setsid ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=20 symbiot@172.16.42.1 "$@" < /dev/null 2>/dev/null
