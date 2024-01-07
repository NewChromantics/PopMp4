#pragma once

#include <stdbool.h>
#include <stdint.h>

//	constant for invalid instance numbers, to avoid use of magic-number 0 around code bases
enum { PopMp4Decoder_NullInstance=0 };

//	DLL_EXPORT defined by compiler settings
//	typically extern"C"

//	version thousand is (Major*1000*1000) + (Minor*1000) + (Patch*1)
//	1.2.3	= 100200300
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
