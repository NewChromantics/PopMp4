#pragma once

#if defined(__cplusplus)
#define DLL_EXPORT extern "C"
#else
#define DLL_EXPORT
#endif

//	find a way to either automate this, or this file should be a symlink or something in the .package
#include "../PopMp4.xcframework/macos-arm64_x86_64/PopMp4.framework/Versions/A/Headers/PopMp4.h"
