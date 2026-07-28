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
LOCAL_SRC_DIR="/tmp/rec-time-monitor-source-macos"
BUILD_DIR="/tmp/rec-time-monitor-build-macos"
PLUGINS_DIR="$HOME/Library/Application Support/obs-studio/plugins"
DEVELOPER_ID_APPLICATION="Developer ID Application: MOIZ I BARTRA PRODUCCIONS SL (B5DRNCGUYN)"

# CMake's OBS bootstrap stores and builds its nested dependencies under the
# source directory. Pointing only BUILD_DIR at /tmp is therefore not enough
# when the checkout lives in iCloud Drive: Xcode can still hang while reading
# dehydrated files from SRC_DIR/.deps. Keep a reusable local source snapshot;
# its own .deps survives between builds, while rsync refreshes project files.
mkdir -p "$LOCAL_SRC_DIR"
rsync -a --delete \
  --exclude '.git/' \
  --exclude '.deps/' \
  --exclude 'build_*/' \
  --exclude 'dist/' \
  "$SRC_DIR/" "$LOCAL_SRC_DIR/"

# A build tree configured before the local snapshot was introduced points at
# the iCloud source path and CMake refuses to reuse it with a different -S.
if [[ -f "$BUILD_DIR/CMakeCache.txt" ]] &&
  ! grep -qF "CMAKE_HOME_DIRECTORY:INTERNAL=$LOCAL_SRC_DIR" "$BUILD_DIR/CMakeCache.txt"; then
  cmake -E remove_directory "$BUILD_DIR"
fi

cmake -S "$LOCAL_SRC_DIR" -B "$BUILD_DIR" -G Xcode \
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
