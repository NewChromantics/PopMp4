#pragma once
/*
 
 objective-c api for swift to get to access to low level c++ stuff
 
*/
#include "PopMp4Framework.h"

#import <Foundation/Foundation.h>
#import <Accelerate/Accelerate.h>
#import <simd/simd.h>



//	data generation for testing float->simd3<float> swift conversion
//DLL_EXPORT FrameResource* __nonnull CRStreamer_GetTrianglePositions();

DLL_EXPORT NSString* __nonnull PopMp4_GetVersion();
//DLL_EXPORT NSString* __nonnull SegmentManager_WaitForStreamMetaChanged_Objc(int Instance);
//DLL_EXPORT FrameResource* __nonnull SegmentManager_WaitForResourceDownload_Objc(int Instance);
