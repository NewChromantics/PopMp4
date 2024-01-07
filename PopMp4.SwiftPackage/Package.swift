// swift-tools-version: 5.8
// The swift-tools-version declares the minimum version of Swift required to build this package.


import PackageDescription



let package = Package(
	name: "PopMp4Swift",
	
	platforms: [
		.iOS(.v15),
		.macOS(.v10_15)
	],
	

	products: [
		.library(
			name: "PopMp4Swift",
			targets: [
				"PopMp4Swift"
			]),
	],
	targets: [

		.target(
			name: "PopMp4Swift",
			/* include all targets where .h contents need to be accessible to swift */
			dependencies: ["PopMp4Objc","PopMp4Framework"],
			path: "./PopMp4Swift"
		),
		
		.binaryTarget(
					name: "PopMp4Framework",
					path: "PopMp4.xcframework"
				),
		
		.target(
			name: "PopMp4Objc",
			dependencies: [],
			path: "./PopMp4Objc",
			//publicHeadersPath: ".",	//	not using include/ seems to have some errors resolving symbols? (this may before my extern c's)
			cxxSettings: [
				.headerSearchPath("../PopMp4Api/"),	//	this allows headers in same place as .cpp
			]
		)
		,
/*
		.testTarget(
			name: "PopMp4Tests",
			dependencies: ["PopMp4"]
		),
 */
	]
)
