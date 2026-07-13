#!/bin/bash
# Notarizes the signed .pkg built by build-macos-installer.sh, then staples
# the notarization ticket to it so Gatekeeper trusts it fully offline (no
# "unidentified developer" warning at all on the installing machine).
#
# Requires a one-time setup that only YOU should run (never paste an
# app-specific password into a chat/AI session): in Terminal,
#
#   xcrun notarytool store-credentials "rec-time-monitor-notary" \
#     --apple-id "your-apple-id@example.com" \
#     --team-id "B5DRNCGUYN"
#
# It will prompt for an app-specific password (generate one at
# https://appleid.apple.com/account/manage under "Sign-In and Security" >
# "App-Specific Passwords"). This stores the credential securely in the
# keychain under the profile name used below, so this script never needs to
# see the password itself.
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_PATH="$SRC_DIR/dist/REC-Time-Monitor-macOS.pkg"
KEYCHAIN_PROFILE="rec-time-monitor-notary"

if [ ! -f "$PKG_PATH" ]; then
  echo "No trobat: $PKG_PATH — executa primer ./build-macos-installer.sh" >&2
  exit 1
fi

xcrun notarytool submit "$PKG_PATH" --keychain-profile "$KEYCHAIN_PROFILE" --wait

xcrun stapler staple "$PKG_PATH"

echo "Notaritzat i grapat: $PKG_PATH"
