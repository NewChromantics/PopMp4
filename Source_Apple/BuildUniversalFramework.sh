# exit(1) if any subcommands fail
set -e


PROJECT_NAME=PopMp4

# build temporary dir
BUILD_ARCHIVE_DIR="./Build"
#	use BUILT_PRODUCTS_DIR for PopAction_Apple which gets first stdout output
BUILT_PRODUCTS_DIR="./PopMp4.SwiftPackage"
BUILD_UNIVERSAL_DIR=${BUILT_PRODUCTS_DIR}

BUILDPATH_IOS="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_Ios"
BUILDPATH_IOSSIM="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_IosSimulator"
BUILDPATH_MACOS="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_Macos"
BUILDPATH_TVOS="${BUILD_ARCHIVE_DIR}/${PROJECT_NAME}_Tvos"
SCHEME_IOS="${PROJECT_NAME}_Framework"
SCHEME_IOSSIM="${PROJECT_NAME}_Framework"
SCHEME_MACOS="${PROJECT_NAME}_Framework"
SCHEME_TVOS="${PROJECT_NAME}_Framework"

PRODUCT_NAME_UNIVERSAL="${PROJECT_NAME}.xcframework"
PRODUCT_PATH="${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL}"

CONFIGURATION="Release"
DESTINATION_IOS="generic/platform=iOS"
DESTINATION_IOSSIM="generic/platform=iOS Simulator"
DESTINATION_MACOS="generic/platform=macos"
DESTINATION_TVOS="generic/platform=tvos"

# build archived frameworks
# see https://github.com/NewChromantics/PopAction_BuildApple/blob/master/index.js for some battle tested CLI options
echo "Building sub-framework archives..."
# gr: make sure destinations are in quotes
#xcodebuild archive -scheme ${SCHEME_IOS} -archivePath $BUILDPATH_IOS SKIP_INSTALL=NO -sdk iphoneos -configuration ${CONFIGURATION} -destination ${DESTINATION_IOS}
xcodebuild archive -scheme ${SCHEME_IOS}    -archivePath $BUILDPATH_IOS SKIP_INSTALL=NO -configuration ${CONFIGURATION} -destination ${DESTINATION_IOS}
xcodebuild archive -scheme ${SCHEME_IOSSIM} -archivePath $BUILDPATH_IOSSIM SKIP_INSTALL=NO -configuration ${CONFIGURATION} -destination "${DESTINATION_IOSSIM}"
xcodebuild archive -scheme ${SCHEME_MACOS}  -archivePath $BUILDPATH_MACOS SKIP_INSTALL=NO -configuration ${CONFIGURATION} -destination ${DESTINATION_MACOS}
#xcodebuild archive -scheme ${SCHEME_TVOS}   -archivePath $BUILDPATH_IOS SKIP_INSTALL=NO -configuration ${CONFIGURATION} -destination ${DESTINATION_TVOS}


if [ -d "${PRODUCT_PATH}" ] ; then
	echo "Cleaning (deleting) old xcframework... (${PRODUCT_PATH})"
	rm -rf ${PRODUCT_PATH}
fi

# bundle together
echo "Building xcframework... (${PRODUCT_PATH})"
Frameworks=""
Frameworks+=" -framework ${BUILDPATH_IOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}.framework"
Frameworks+=" -framework ${BUILDPATH_IOSSIM}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}.framework"
Frameworks+=" -framework ${BUILDPATH_MACOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}.framework"
#Frameworks+=" -framework ${BUILDPATH_TVOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}.framework"

#xcodebuild -create-xcframework -framework ${BUILDPATH_IOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -framework ${BUILDPATH_SIM}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -framework ${BUILDPATH_OSX}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Osx.framework -output ./build/${FULL_PRODUCT_NAME}
#xcodebuild -create-xcframework -framework ${BUILDPATH_IOS}.xcarchive/Products/Library/Frameworks/${PROJECT_NAME}_Ios.framework -output ${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL}
xcodebuild -create-xcframework ${Frameworks} -output ${BUILD_UNIVERSAL_DIR}/${PRODUCT_NAME_UNIVERSAL}

echo "xcframework ${PRODUCT_NAME_UNIVERSAL} built successfully"

# output meta for https://github.com/NewChromantics/PopAction_BuildApple
# which matches a regular scheme output
echo "FULL_PRODUCT_NAME=${PRODUCT_NAME_UNIVERSAL}"
echo "BUILT_PRODUCTS_DIR=${BUILD_UNIVERSAL_DIR}"

# output meta for github
# gr: I think my apple action will pull this out? if we use FULL_PRODUCT_NAME?
#echo PRODUCT_NAME=${PRODUCT_NAME_UNIVERSAL} >> GITHUB_OUTPUT
#echo PRODUCT_PATH=${BUILD_UNIVERSAL_DIR} >> GITHUB_OUTPUT
