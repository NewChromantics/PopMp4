#pragma once
/*
 
 objective-c api for swift to get to access to low level c++ stuff
 
*/
#import <Foundation/Foundation.h>
#import <Accelerate/Accelerate.h>
#import <simd/simd.h>

//	linkage from swift needs to not have extern"C" and does no mangling.
//	objective-c mangles the name so this needs to be extern"C"
#if !defined(DLL_EXPORT)
#define DLL_EXPORT
#endif

DLL_EXPORT NSString*__nonnull PopMp4_GetVersion();
DLL_EXPORT int PopMp4_AllocDecoder(NSString* Filename);
DLL_EXPORT void PopMp4_FreeDecoder(int Instance);
DLL_EXPORT NSString*__nonnull PopMp4_GetDecodeStateJson(int Instance);

/*
 DLL_EXPORT int		PopMp4_GetVersionThousand();
 
 //	returns PopMp4Decoder_NullInstance if we failed to create a decoder
 DLL_EXPORT int		PopMp4_CreateDecoder();
 DLL_EXPORT int		PopMp4_CreateDecoderWithOptions(const char* OptionsJson,char* ErrorBuffer,int ErrorBufferSize);
 DLL_EXPORT void		PopMp4_DestroyDecoder(int Instance);
 DLL_EXPORT bool		PopMp4_PushMp4Data(int Instance,const uint8_t* Data,uint32_t DataSize,bool EndOfFile);
 
 DLL_EXPORT bool		PopMp4_PopSample(int Instance,bool* EndOfFile,uint8_t* DataBuffer,uint32_t* DataBufferSize,uint64_t* PresentationTimeMs,uint64_t* DecodeTimeMs,uint16_t* Stream,bool* IsKeyframe,uint64_t* DurationMs);
 
 //	fills json with the state of the decoder
 //	.Error		Processing/allocaton error
 //	.Atoms		Tree of the atoms so-far parsed in the file
 //	.EndOfFile	File has finished parsing
 DLL_EXPORT void		PopMp4_GetDecoderState(int Instance,char* JsonBuffer,int JsonBufferSize);
 
 */
