#!/bin/zsh

set -euo pipefail

readonly script_directory="${0:A:h}"
readonly repository_root="${script_directory:h:h}"
readonly smart_developer_directory="${SMART_XCODE_DEVELOPER_DIR:-/Applications/Xcode-27-beta-6.app/Contents/Developer}"
readonly smart_derived_data="${SMART_DERIVED_DATA_PATH:-/private/tmp/SmartMcBopomofoSafeBuildXcode27}"
readonly smart_product="${smart_derived_data}/Build/Products/Release/SmartMcBopomofoBeta.app"
readonly launch_services_tool="/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister"

if [[ "${smart_derived_data}" != /private/tmp/SmartMcBopomofo* ]]; then
    print -u2 "SMART_DERIVED_DATA_PATH must be a dedicated /private/tmp/SmartMcBopomofo path."
    exit 2
fi

unregister_temporary_product() {
    if [[ -d "${smart_product}" ]]; then
        # Xcode registers every built input-method app with Launch Services.
        # Remove only the temporary build product so System Settings continues
        # to show the single copy installed under ~/Library/Input Methods.
        "${launch_services_tool}" -u "${smart_product}" >/dev/null 2>&1 || true
    fi
}

trap unregister_temporary_product EXIT

cd "${repository_root}"
DEVELOPER_DIR="${smart_developer_directory}" xcodebuild \
    -project McBopomofo.xcodeproj \
    -scheme McBopomofo \
    -configuration Release \
    -derivedDataPath "${smart_derived_data}" \
    ARCHS="arm64 x86_64" \
    ONLY_ACTIVE_ARCH=NO \
    build

codesign --verify --deep --strict --verbose=2 "${smart_product}"
print "Built and verified: ${smart_product}"
