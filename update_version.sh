#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# Get the new version and codename from their respective files
NEW_VERSION_SIMPLE=$(cat .version/VERSION_FILE)
NEW_CODENAME=$(cat .version/CODENAME_FILE)

NEW_VERSION_FULL="${NEW_VERSION_SIMPLE} - (${NEW_CODENAME})"

echo "Updating project version to: $NEW_VERSION_FULL"

# Get the old version from the Makefile to use in sed replacement
OLD_VERSION=$(grep -oE 'VERSION := "v[0-9]+\.[0-9]+\.[0-9]+-beta"' Makefile | awk '{print $NF}')
echo "Detected old version: $OLD_VERSION"

# Update the Makefile
# The new Makefile will read the version directly from VERSION_FILE and CODENAME_FILE
sed -i "s/VERSION := .*/VERSION := \"\$(shell cat .version\/VERSION_FILE) - (\$(shell cat .version\/CODENAME_FILE))\"/g" Makefile

# Update the README.md
sed -i "s/v0\.1\.0-beta/${NEW_VERSION_SIMPLE}/g" README.md
sed -i "s/v0\.1\.0-beta - (Betelgeuse)/${NEW_VERSION_FULL}/g" README.md

# Update the man page (fat.1)
sed -i "s/v0\.1\.0-beta/${NEW_VERSION_SIMPLE}/g" man/fat.1

# Update the snapcraft.yaml file
# This one requires special handling as it uses a different version format ('0.1.0')
SNAP_OLD_VERSION=$(grep 'version:' snap/snapcraft.yaml | awk '{print $NF}' | tr -d "'")
SNAP_NEW_VERSION=$(echo "$NEW_VERSION_SIMPLE" | sed 's/v//g' | sed 's/-beta//g')
sed -i "s/version: .*/version: '${SNAP_NEW_VERSION}'/g" snap/snapcraft.yaml

echo "Version update complete!"