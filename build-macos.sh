#!/bin/bash
# Builds the plugin and installs it into OBS's user plugins directory.
#
# The build must happen OUTSIDE this iCloud-synced project folder: files
# synced by iCloud Drive pick up extended attributes that make macOS
# codesign fail ("resource fork, Finder information, or similar detritus
# not allowed"). So this script configures/builds in a scratch directory
# under /tmp and only copies the finished .plugin bundle back here / into OBS.
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/rec-time-monitor-build-macos"
PLUGINS_DIR="$HOME/Library/Application Support/obs-studio/plugins"
DEVELOPER_ID_APPLICATION="Developer ID Application: MOIZ I BARTRA PRODUCCIONS SL (B5DRNCGUYN)"

cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Xcode \
  -DENABLE_FRONTEND_API=ON -DENABLE_QT=ON \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DPLUGIN_BUILD_NUMBER=1

cmake --build "$BUILD_DIR" --config RelWithDebInfo

rm -rf "$PLUGINS_DIR/rec-time-monitor.plugin"
cp -R "$BUILD_DIR/RelWithDebInfo/rec-time-monitor.plugin" "$PLUGINS_DIR/rec-time-monitor.plugin"
xattr -cr "$PLUGINS_DIR/rec-time-monitor.plugin"

if security find-identity -v -p codesigning | grep -qF "$DEVELOPER_ID_APPLICATION"; then
  codesign --force --options runtime --timestamp \
    --sign "$DEVELOPER_ID_APPLICATION" \
    "$PLUGINS_DIR/rec-time-monitor.plugin"
  echo "Signat amb: $DEVELOPER_ID_APPLICATION"
else
  echo "AVÍS: no s'ha trobat el certificat 'Developer ID Application' al clauer; plugin instal·lat sense signar." >&2
fi

echo "Installed: $PLUGINS_DIR/rec-time-monitor.plugin"
echo "Reinicia OBS (o Tools > Refresh browser sources no cal, cal reiniciar OBS) perquè carregui el plugin."
