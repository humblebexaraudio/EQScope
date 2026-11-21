#!/bin/bash
# Builds a signed EQScope .ipa (the app with the AUv3 inside) for iPhone and iPad, iOS / iPadOS 13 or later.
#
#   TEAM_ID=ABCDE12345 ./installer/ios/build_ios.sh                  # development build for your own devices
#   TEAM_ID=ABCDE12345 METHOD=app-store ./installer/ios/build_ios.sh # for TestFlight / the App Store
#
# TEAM_ID is your Apple Developer team ID (developer.apple.com -> Membership, or Xcode -> Settings -> Accounts).
# Your Apple account must be signed in to Xcode. A free account can only make development builds,
# which expire after 7 days. The .ipa is written to dist/ios/.
set -euo pipefail
cd "$(dirname "$0")/../.."

TEAM_ID="${TEAM_ID:?Set TEAM_ID to your Apple Developer team ID, e.g. TEAM_ID=ABCDE12345}"
METHOD="${METHOD:-development}"

cmake -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_XCODE_GENERATE_SCHEME=ON \
      -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID" \
      -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_STYLE=Automatic

xcodebuild -project build-ios/EQScope.xcodeproj -scheme EQScope_Standalone -configuration Release \
           -destination "generic/platform=iOS" -archivePath build-ios/EQScope.xcarchive \
           -allowProvisioningUpdates archive

sed -e "s/@METHOD@/$METHOD/" -e "s/@TEAM_ID@/$TEAM_ID/" installer/ios/ExportOptions.plist > build-ios/ExportOptions.plist
xcodebuild -exportArchive -archivePath build-ios/EQScope.xcarchive -exportPath dist/ios \
           -exportOptionsPlist build-ios/ExportOptions.plist -allowProvisioningUpdates

echo "Created dist/ios/ (method: $METHOD)"
