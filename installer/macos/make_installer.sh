#!/bin/bash
# Builds dist/EQScope-<version>-macOS.pkg: a macOS installer for all users, for macOS 10.13 (High Sierra)
# or later on Intel and Apple Silicon Macs, with
#   - AU   -> /Library/Audio/Plug-Ins/Components/EQScope.component   (Logic, GarageBand, MainStage…)
#   - VST3 -> /Library/Audio/Plug-Ins/VST3/EQScope.vst3              (Ableton, Cubase, Reaper…)
#   - the standalone app and the user guide -> /Applications/Humble Bexar Audio/
# "Customize" in the installer lets people choose which parts to install. If EQ Probe (EQScope's
# name before 1.0) is installed, it is removed, because EQScope replaces it.
#
# Run after a Release build (cmake --build build --config Release). The user guide PDF is already in
# docs/; remake it with python3 docs/make_user_guide.py only if you changed it.
#
# Unsigned by default. To sign and notarize, set these first (needs a paid Apple Developer account):
#   APP_CERT="Developer ID Application: Your Name (TEAMID)"
#   INST_CERT="Developer ID Installer: Your Name (TEAMID)"
#   NOTARY_PROFILE="eqscope-notary"   # created once with: xcrun notarytool store-credentials eqscope-notary
#
# Written for the Bash 3.2 that ships with macOS: no arrays (an empty array is an "unbound variable"
# there under set -u), and every optional setting has a default.
set -euo pipefail
cd "$(dirname "$0")/../.."

VERSION="${VERSION:-1.1.0}"
BUILD_DIR="${BUILD_DIR:-build}"
APP_CERT="${APP_CERT:-}"
INST_CERT="${INST_CERT:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
MIN_MACOS="10.13"

ART="$BUILD_DIR/EQScope_artefacts/Release"
GUIDE="docs/EQScope User Guide.pdf"
RES="installer/macos/resources"
ID="com.humblebexar.eqscope"
OUT="dist/EQScope-$VERSION-macOS.pkg"
PLISTBUDDY="/usr/libexec/PlistBuddy"

for f in "$ART/AU/EQScope.component" "$ART/VST3/EQScope.vst3" "$ART/Standalone/EQScope.app" "$GUIDE"; do
    if [ ! -e "$f" ]; then
        echo "Missing: $f"
        echo "Build Release first:  cmake --build $BUILD_DIR --config Release"
        exit 1
    fi
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/au" "$WORK/vst3" "$WORK/app/Humble Bexar Audio" \
         "$WORK/scripts-au" "$WORK/scripts-vst3" "$WORK/scripts-app" "$WORK/pkgs" dist

ditto "$ART/AU/EQScope.component"   "$WORK/au/EQScope.component"
ditto "$ART/VST3/EQScope.vst3"      "$WORK/vst3/EQScope.vst3"
ditto "$ART/Standalone/EQScope.app" "$WORK/app/Humble Bexar Audio/EQScope.app"
cp "$GUIDE" "$WORK/app/Humble Bexar Audio/"

if [ -n "$APP_CERT" ]; then
    echo "Signing bundles with: $APP_CERT"
    for b in "$WORK/au/EQScope.component" "$WORK/vst3/EQScope.vst3" "$WORK/app/Humble Bexar Audio/EQScope.app"; do
        codesign --force --options runtime --timestamp --sign "$APP_CERT" "$b"
    done
fi

# Each part removes its EQ Probe predecessor (same plug-in ID) before installing.
cat > "$WORK/scripts-au/preinstall" <<'EOS'
#!/bin/sh
rm -rf "/Library/Audio/Plug-Ins/Components/EQ Probe.component"
exit 0
EOS
cat > "$WORK/scripts-au/postinstall" <<'EOS'
#!/bin/sh
# Make Logic notice the new AU straight away.
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
EOS
cat > "$WORK/scripts-vst3/preinstall" <<'EOS'
#!/bin/sh
rm -rf "/Library/Audio/Plug-Ins/VST3/EQ Probe.vst3"
exit 0
EOS
cat > "$WORK/scripts-app/preinstall" <<'EOS'
#!/bin/sh
rm -rf "/Applications/Humble Bexar Audio/EQ Probe.app" "/Applications/Humble Bexar Audio/EQ Probe User Guide.pdf"
exit 0
EOS
chmod +x "$WORK"/scripts-*/*

# Component packages. Bundles are marked non-relocatable so they always land in the folders above,
# even if an older copy exists elsewhere on the disk.
make_component() { # name root install-location scripts-folder
    name="$1"; root="$2"; location="$3"; scripts="$4"
    plist="$WORK/$name.plist"
    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    i=0
    while "$PLISTBUDDY" -c "Print :$i" "$plist" >/dev/null 2>&1; do
        # Some macOS versions leave the key out for some bundles: add it if it isn't there.
        "$PLISTBUDDY" -c "Set :$i:BundleIsRelocatable false" "$plist" >/dev/null 2>&1 \
            || "$PLISTBUDDY" -c "Add :$i:BundleIsRelocatable bool false" "$plist"
        i=$((i + 1))
    done
    pkgbuild --root "$root" --component-plist "$plist" --install-location "$location" --scripts "$scripts" \
             --identifier "$ID.$name" --version "$VERSION" --min-os-version "$MIN_MACOS" "$WORK/pkgs/$name.pkg"
}
make_component au   "$WORK/au"   "/Library/Audio/Plug-Ins/Components" "$WORK/scripts-au"
make_component vst3 "$WORK/vst3" "/Library/Audio/Plug-Ins/VST3"       "$WORK/scripts-vst3"
make_component app  "$WORK/app"  "/Applications"                      "$WORK/scripts-app"

sed -e "s/@VERSION@/$VERSION/g" -e "s/@MIN_MACOS@/$MIN_MACOS/g" installer/macos/distribution.xml > "$WORK/distribution.xml"

rm -f "$OUT"
if [ -n "$INST_CERT" ]; then
    productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" --resources "$RES" \
                 --sign "$INST_CERT" "$OUT"
else
    productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK/pkgs" --resources "$RES" "$OUT"
fi

if [ -n "$NOTARY_PROFILE" ]; then
    echo "Notarizing (this can take a few minutes)…"
    xcrun notarytool submit "$OUT" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$OUT"
fi

echo "Created $OUT"
