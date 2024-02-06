#define DLL_EXPORT extern"C"
//#define DLL_EXPORT
#include "../PopMp4.xcframework/macos-arm64_x86_64/PopMp4.framework/Versions/A/Headers/PopMp4.h"

//#include "PopMp4.h"
#import "include/PopMp4Api.h"
#import <Foundation/Foundation.h>
#include <array>
#include <iostream>


@implementation Mp4DecoderWrapper
{
	int instance;
}

- (id)init
{
	self = [super init];
	instance = PopMp4Decoder_NullInstance;
	return self;
}

- (void)allocateWithFilename:(NSString*)Filename error:(NSError**)throwError __attribute__((swift_error(nonnull_error)))
{
	*throwError = nil;
	try
	{
		@try
		{
			instance = PopMp4_AllocDecoder(Filename);
		}
		@catch (NSException* exception)
		{
			//*throwError = [NSError errorWithDomain:exception.reason code:0 userInfo:nil];
			throw std::runtime_error(exception.reason.UTF8String);
		}
	}
	catch (std::exception& e)
	{
		//*throwError = [NSError errorWithDomain:@"PopMp4 allocate" code:0 userInfo:nil];
		NSString* error = [NSString stringWithUTF8String:e.what()];
		*throwError = [NSError errorWithDomain:error code:0 userInfo:nil];
		//*throwError = GetError(exception);
	}
}


- (void)allocate:(NSError**)throwError __attribute__((swift_error(nonnull_error)))
{
	*throwError = nil;
	try
	{
		@try
		{
			instance = PopMp4_AllocDecoder(nil);
		}
		@catch (NSException* exception)
		{
			//*throwError = [NSError errorWithDomain:exception.reason code:0 userInfo:nil];
			throw std::runtime_error(exception.reason.UTF8String);
		}
	}
	catch (std::exception& e)
	{
		//*throwError = [NSError errorWithDomain:@"PopMp4 allocate" code:0 userInfo:nil];
		NSString* error = [NSString stringWithUTF8String:e.what()];
		*throwError = [NSError errorWithDomain:error code:0 userInfo:nil];
		//*throwError = GetError(exception);
	}
}

- (void)free
{
	PopMp4_FreeDecoder(instance);
	//mInstance = PopMp4Decoder_NullInstance;
}

- (NSString*__nonnull)getDecoderStateJson:(NSError**)throwError __attribute__((swift_error(nonnull_error)))
{
	*throwError = nil;
	try
	{
		@try
		{
			return PopMp4_GetDecodeStateJson(instance);
		}
		@catch (NSException* exception)
		{
			//*throwError = [NSError errorWithDomain:exception.reason code:0 userInfo:nil];
			throw std::runtime_error(exception.reason.UTF8String);
		}
	}
	catch (std::exception& e)
	{
		NSString* error = [NSString stringWithUTF8String:e.what()];
		*throwError = [NSError errorWithDomain:error code:0 userInfo:nil];
	}
}


- (void)pushData:(NSData*__nonnull)data
{
	//uint8_t* DataAddress = reinterpret_cast<uint8_t*>(data.bytes);
	uint8_t* DataAddress = (uint8_t*)data.bytes;
	bool EndOfFile = false;
	PopMp4_PushMp4Data( instance, DataAddress, data.length, EndOfFile);
}

- (void)pushEndOfFile
{
	bool EndOfFile = true;
	PopMp4_PushMp4Data( instance, nullptr, 0, EndOfFile);
}

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
	std::vector<char> ErrorBuffer(100*1024);

	NSMutableDictionary* Options =
	@{
	//	@"Filename" : Filename,
	};
	if ( Filename != nullptr )
		Options[@"Filename"] = Filename;
	
	NSData* OptionsJsonData = [NSJSONSerialization dataWithJSONObject:Options options:NSJSONWritingPrettyPrinted error:nil];
	NSString* OptionsJsonString = [[NSString alloc] initWithData:OptionsJsonData encoding:NSUTF8StringEncoding];
	const char* OptionsJsonStringC = [OptionsJsonString UTF8String];

	auto Instance = ::PopMp4_CreateDecoderWithOptions( OptionsJsonStringC, ErrorBuffer.data(), ErrorBuffer.size() );

	//auto Error = [NSString stringWithUTF8String: ErrorBuffer.data()];
	auto Error = std::string( ErrorBuffer.data() );

	if ( !Error.empty() )
	//if ( Error.length > 0 )
		//@throw([NSException exceptionWithName:@"Error allocating MP4 decoder" reason:Error userInfo:nil]);
		throw std::runtime_error(Error);
	
	if ( Instance == PopMp4Decoder_NullInstance )
		//@throw([NSException exceptionWithName:@"Error allocating MP4 decoder" reason:@"null returned" userInfo:nil]);
		throw std::runtime_error("Failed to allocate PopMp4 instance");
	
	return Instance;
}

DLL_EXPORT void PopMp4_FreeDecoder(int Instance)
{
	::PopMp4_FreeDecoder(Instance);
}

//	todo: can return a obj-c struct for swift to use directly instead of re-parsing in swift
DLL_EXPORT NSString*__nonnull PopMp4_GetDecodeStateJson(int Instance)
{
	std::vector<char> JsonBuffer(50*1024*1024);
	PopMp4_GetDecoderState( Instance, JsonBuffer.data(), JsonBuffer.size() );
	
	auto Length = std::strlen(JsonBuffer.data());
	if ( Length > 512*1024 )
	{
		auto LengthKb = Length / 1024;
		std::cerr << "Warning; PopMp4_GetDecoderState json is " << LengthKb << "kb" << std::endl;
	}
	auto Json = [NSString stringWithUTF8String: JsonBuffer.data()];
	//auto JsonData = [NSData dataWithBytes:JsonBuffer.data() length:JsonBuffer.size()];
	auto JsonData = [NSData dataWithBytes:JsonBuffer.data() length:Length];

	NSError* JsonParseError = nil;
	auto Dictionary = [NSJSONSerialization JSONObjectWithData:JsonData options:NSJSONReadingMutableContainers error:&JsonParseError];
	
	//return Dictionary;
	return Json;
}



