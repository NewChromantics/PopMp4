#pragma once
#include <stdint.h>

#if defined(EXPORT)

#elif defined(_MSC_VER)
//	windows
#define EXPORT __declspec(dllexport) extern "C"
#else
//	mac etc
#define EXPORT extern "C"
#endif

//	CAPI for convinence
EXPORT int		PopMp4_CreateDecoder();
EXPORT void		PopMp4_DestroyDecoder(int Instance);
EXPORT bool		PopMp4_PushMp4Data(int Instance,const uint8_t* Data,uint32_t DataSize,bool EndOfFile); 
EXPORT bool		PopMp4_PopSample(int Instance,bool* EndOfFile,uint8_t* DataBuffer,uint32_t* DataBufferSize,uint64_t* PresentationTimeMs,uint64_t* DecodeTimeMs,uint16_t* Stream,bool* IsKeyframe,uint64_t* DurationMs);
