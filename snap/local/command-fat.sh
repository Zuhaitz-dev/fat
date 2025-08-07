#!/bin/bash

# Define the root of the host's filesystem as seen from inside the snap
HOST_ROOT="/var/lib/snapd/hostfs"

# Set up the environment to find and run host-system commands
export PATH="$HOST_ROOT/usr/bin:$HOST_ROOT/bin:$PATH"
export LD_LIBRARY_PATH="$HOST_ROOT/usr/lib:$HOST_ROOT/lib:$HOST_ROOT/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"

# Execute the actual fat binary with all the arguments passed to the script.
exec "$SNAP/usr/local/bin/fat" "$@"
