#define DLL_EXPORT extern"C"
//#define DLL_EXPORT
#include "../PopMp4.xcframework/macos-arm64_x86_64/PopMp4.framework/Versions/A/Headers/PopMp4.h"

//#include "PopMp4.h"
#import "include/PopMp4Api.h"
#import <Foundation/Foundation.h>
#include <array>


@implementation FrameResource

@end

//	to be visible in swift, the declaration is in header.
//	but all headers for swift are in C (despite objc types??) and are not mangled
//	therefore with mm (c++) the name needs unmangling
DLL_EXPORT NSString* PopMp4_GetVersion()
{
	auto VersionThousand = PopMp4_GetVersionThousand();
	//auto VersionThousand = 0;
	auto Major = (VersionThousand/1000/1000) % 1000;
	auto Minor = (VersionThousand/1000) % 1000;
	auto Patch = (VersionThousand) % 1000;
	return [NSString stringWithFormat:@"%d.%d.%d", Major, Minor, Patch ];
}
/*

DLL_EXPORT NSString* __nonnull SegmentManager_WaitForStreamMetaChanged_Objc(int Instance)
{
	std::array<char,100*1024> JsonBuffer;
	if ( !SegmentManager_WaitForStreamMetaChanged( Instance, JsonBuffer.data(), JsonBuffer.size() ) )
	{
		auto* ErrorJson = "{\"Error\":\"WaitForStreamMetaChangedError\"}";
		return @(ErrorJson);
	}
	
	return @(JsonBuffer.data());
	auto Json = [NSString stringWithUTF8String:JsonBuffer.data()];
	return Json;
}

//	todo: need bytes!
DLL_EXPORT FrameResource* __nonnull SegmentManager_WaitForResourceDownload_Objc(int Instance)
{
	static std::vector<char> JsonBuffer(100*1024);
	static std::vector<uint8_t> DataBuffer(10*1024*1024);

	FrameResource* Resource = [FrameResource alloc];

	if ( !SegmentManager_WaitForResourceDownload( Instance, DataBuffer.data(), DataBuffer.size(), JsonBuffer.data(), JsonBuffer.size() ) )
	{
		auto* ErrorJson = "{\"Error\":\"SegmentManager_WaitForResourceDownload\"}";
		Resource.meta = @(ErrorJson);
		return Resource;
	}

	auto* Json = @(JsonBuffer.data());
	NSData* jsonData = [Json dataUsingEncoding:NSUTF8StringEncoding];
	NSError *jsonError;
	NSDictionary* Meta = [NSJSONSerialization JSONObjectWithData:jsonData options:NSJSONWritingPrettyPrinted error:&jsonError];

	int DataSize = [Meta[@"DataSize"] integerValue];
	bool Timeout = [Meta[@"Timeout"] boolValue];

	//auto Json = [NSString stringWithUTF8String:JsonBuffer.data()];
	//return Json;
	Resource.meta = @(JsonBuffer.data());
	if ( !Timeout )
		Resource.data = [NSData dataWithBytes:DataBuffer.data() length:DataSize];
	
	return Resource;
}
*/


DLL_EXPORT int PopMp4_AllocDecoder(NSString* Filename)
{
	return ::PopMp4_CreateDecoder();
}

DLL_EXPORT void PopMp4_FreeDecoder(int Instance)
{
	::PopMp4_FreeDecoder(Instance);
}

//	todo: return a proper struct for swift to use directly
DLL_EXPORT NSString*__nonnull PopMp4_GetDecodeStateJson(int Instance)
{
	std::vector<char> JsonBuffer(100*1024);
	PopMp4_GetDecoderState( Instance, JsonBuffer.data(), JsonBuffer.size() );
	
	auto Json = [NSString stringWithUTF8String: JsonBuffer.data()];
	auto JsonData = [NSData dataWithBytes:JsonBuffer.data() length:JsonBuffer.size()];
	
	NSError* JsonParseError = nil;
	auto Dictionary = [NSJSONSerialization JSONObjectWithData:JsonData options:NSJSONReadingMutableContainers error:&JsonParseError];
	
	//return Dictionary;
	return Json;
}



