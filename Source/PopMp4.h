#pragma once
#include <stdint.h>

//	CAPI for convinence
extern "C" int		PopMp4_CreateDecoder();
extern "C" void		PopMp4_DestroyDecoder(int Instance);
extern "C" bool		PopMp4_PushMp4Data(int Instance,const uint8_t* Data,uint32_t DataSize,bool EndOfFile); 
extern "C" bool		PopMp4_PopSample(int Instance,uint64_t* Timestamp,uint16_t* Stream,uint8_t* DataBuffer,uint32_t* DataBufferSize,bool* EndOfFile);
