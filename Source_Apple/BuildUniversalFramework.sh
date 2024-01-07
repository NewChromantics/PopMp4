# exit(1) if any subcommands fail
set -e


PROJECT_NAME=PopMp4

# build temporary dir
BUILD_ARCHIVE_DIR="./Build"
#	use BUILT_PRODUCTS_DIR for PopAction_Apple which gets first stdout output
BUILT_PRODUCTS_DIR="./PopMp4.SwiftPackage"
BUILD_UNIVERSAL_DIR=${BUILT_PRODUCTS_DIR}

BUILDPATH_IOS="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_Ios"
BUILDPATH_SIM="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_IosSimulator"
#BUILDPATH_MACOS="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_Osx"
BUILDPATH_MACOS="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_Macos"
BUILDPATH_TVOS="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_Tvos"
SCHEME_MACOS="${PROJECT_NAME}_Framework"

PRODUCT_NAME_UNIVERSAL="${PROJECT_NAME}.xcframework"
PRODUCT_PATH="${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL}"

CONFIGURATION="Release"
DESTINATION_IOS="generic/platform=iOS"
DESTINATION_MACOS="generic/platform=macos"

# build archived frameworks
# see https://github.com/NewChromantics/PopAction_BuildApple/blob/master/index.js for some battle tested CLI options
echo "Building sub-framework archives..."
#xcodebuild archive -scheme ${PROJECT_NAME}_Ios -archivePath $BUILDPATH_IOS SKIP_INSTALL=NO -sdk iphoneos -configuration ${CONFIGURATION} -destination ${DESTINATION_IOS}
#xcodebuild archive -scheme ${PROJECT_NAME}_Ios -archivePath $BUILDPATH_SIM SKIP_INSTALL=NO -sdk iphonesimulator
xcodebuild archive -scheme ${SCHEME_MACOS} -archivePath $BUILDPATH_MACOS SKIP_INSTALL=NO -configuration ${CONFIGURATION} -destination ${DESTINATION_MACOS}


if [ -d "${PRODUCT_PATH}" ] ; then
	echo "Cleaning (deleting) old xcframework... (${PRODUCT_PATH})"
	rm -rf ${PRODUCT_PATH}
fi

# bundle together
echo "Building xcframework... (${PRODUCT_PATH})"
#xcodebuild -create-xcframework -framework ${BUILDPATH_IOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -framework ${BUILDPATH_SIM}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -framework ${BUILDPATH_OSX}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Osx.framework -output ./build/${FULL_PRODUCT_NAME}
#xcodebuild -create-xcframework -framework ${BUILDPATH_IOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -output ${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL}
xcodebuild -create-xcframework -framework ${BUILDPATH_MACOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}.framework -output ${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL}

echo "xcframework ${PRODUCT_NAME_UNIVERSAL} built successfully"

# output meta for https://github.com/NewChromantics/PopAction_BuildApple
# which matches a regular scheme output
echo "FULL_PRODUCT_NAME=${PRODUCT_NAME_UNIVERSAL}"
echo "BUILT_PRODUCTS_DIR=${BUILD_UNIVERSAL_DIR}"

# output meta for github
# gr: I think my apple action will pull this out? if we use FULL_PRODUCT_NAME?
#echo PRODUCT_NAME=${PRODUCT_NAME_UNIVERSAL} >> GITHUB_OUTPUT
#echo PRODUCT_PATH=${BUILD_UNIVERSAL_DIR} >> GITHUB_OUTPUT
