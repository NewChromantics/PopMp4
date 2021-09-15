#pragma once
#include <stdint.h>

//	CAPI for convinence
extern "C" int		PopMp4_CreateDecoder();
extern "C" void		PopMp4_DestroyDecoder(int Instance);
extern "C" bool		PopMp4_PushMp4Data(int Instance,const uint8_t* Data,uint32_t DataSize,bool EndOfFile); 
extern "C" bool		PopMp4_PopSample(int Instance,bool* EndOfFile,uint8_t* DataBuffer,uint32_t* DataBufferSize,uint64_t* PresentationTimeMs,uint64_t* DecodeTimeMs,uint16_t* Stream,bool* IsKeyframe,uint64_t* DurationMs);
