#!/bin/bash
# Builds a double-clickable .pkg installer for macOS that drops the plugin
# into ~/Library/Application Support/obs-studio/plugins/ — no admin/root
# password needed, since it targets the "current user home" domain.
#
# If a "Developer ID Installer" certificate is present in the keychain, the
# resulting .pkg is signed with it (see DEVELOPER_ID_INSTALLER below). Signing
# alone still isn't enough for Gatekeeper to fully trust it on other Macs —
# it also needs to be notarized (run notarize-macos.sh afterwards) — but a
# signed-and-notarized .pkg installs without any Gatekeeper warning at all.
# If no such certificate is found, this falls back to an unsigned .pkg (right
# click > Open needed on the installing machine).
set -euo pipefail

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="/tmp/rec-time-monitor-build-macos"
PKG_STAGE="/tmp/rec-time-monitor-pkg-stage"
PLUGIN_BUNDLE="$BUILD_DIR/RelWithDebInfo/rec-time-monitor.plugin"
OUT_DIR="$SRC_DIR/dist"
VERSION="2.0.0"
DEVELOPER_ID_INSTALLER="Developer ID Installer: MOIZ I BARTRA PRODUCCIONS SL (B5DRNCGUYN)"

# Make sure we have a fresh, working plugin build first.
"$SRC_DIR/build-macos.sh"

rm -rf "$PKG_STAGE"
mkdir -p "$PKG_STAGE/root/Library/Application Support/obs-studio/plugins"
cp -R "$PLUGIN_BUNDLE" "$PKG_STAGE/root/Library/Application Support/obs-studio/plugins/rec-time-monitor.plugin"
xattr -cr "$PKG_STAGE/root/Library/Application Support/obs-studio/plugins/rec-time-monitor.plugin"

pkgbuild \
  --root "$PKG_STAGE/root" \
  --identifier cat.ccs.rec-time-monitor \
  --version "$VERSION" \
  --install-location "/" \
  "$PKG_STAGE/component.pkg"

mkdir -p "$OUT_DIR"
PRODUCTBUILD_ARGS=(
  --distribution "$SRC_DIR/installer/macos/distribution.xml"
  --package-path "$PKG_STAGE"
  --resources "$SRC_DIR/installer/macos"
)
if security find-identity -v | grep -qF "$DEVELOPER_ID_INSTALLER"; then
  PRODUCTBUILD_ARGS+=(--sign "$DEVELOPER_ID_INSTALLER")
  echo "Signant el .pkg amb: $DEVELOPER_ID_INSTALLER"
else
  echo "AVÍS: no s'ha trobat el certificat 'Developer ID Installer' al clauer; .pkg generat sense signar." >&2
fi

productbuild "${PRODUCTBUILD_ARGS[@]}" "$OUT_DIR/REC-Time-Monitor-macOS.pkg"

echo "Instal·lador creat: $OUT_DIR/REC-Time-Monitor-macOS.pkg"
echo "Per notaritzar-lo (necessari perquè Gatekeeper hi confiï sense avisos): ./notarize-macos.sh"
