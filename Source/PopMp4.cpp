#include "PopMp4.h"
#include "Mp4Decoder.hpp"
#include "InstanceManager.hpp"
#include <memory>
#include <span>
#include <iostream>


namespace PopMp4
{
	InstanceManager_t<Decoder_t>	DecoderInstances;
}


DLL_EXPORT int PopMp4_GetVersionThousand()
{
	auto VersionThousand = 0;
	VersionThousand += VERSION_MAJOR * 1000 * 1000;
	VersionThousand += VERSION_MINOR * 1000;
	VersionThousand += VERSION_PATCH;
	return VersionThousand;
}

static void StringCopyToBuffer(std::string_view Input,char* OutputBuffer,size_t OutputBufferSize)
{
	//	gr: throw?
	if ( !OutputBuffer || OutputBufferSize == 0 )
		return;
	
	auto CopyLength = std::min( OutputBufferSize, Input.length()+1 );	//	+1 as length doesnt include terminator on string_view
	for ( auto i=0;	i<Input.length();	i++ )
	{
		OutputBuffer[i] = Input[i];
	}
	
	OutputBuffer[CopyLength-1] = '\0';
}


DLL_EXPORT int PopMp4_CreateDecoderWithOptions(const char* OptionsJson,char* ErrorBuffer,int ErrorBufferSize)
{
	try
	{
		if ( !OptionsJson )
			OptionsJson = "{}";
		PopJson::Json_t Options( OptionsJson );
		auto Instance = PopMp4::DecoderInstances.Alloc( Options );
		return Instance;
	}
	catch(std::exception& e)
	{
		std::cerr << e.what() << std::endl;
		StringCopyToBuffer( e.what(), ErrorBuffer, ErrorBufferSize );
		return PopMp4Decoder_NullInstance;
	}
}


DLL_EXPORT int PopMp4_CreateDecoder()
{
	return PopMp4_CreateDecoderWithOptions( nullptr, nullptr, 0 );
}

DLL_EXPORT void PopMp4_DestroyDecoder(int Instance)
{
	try
	{
		PopMp4::DecoderInstances.Free(Instance);
	}
	catch(std::exception& e)
	{
		std::cerr << __FUNCTION__ << " exception; " << e.what() << std::endl;
	}
}

DLL_EXPORT bool PopMp4_PushMp4Data(int Instance,const uint8_t* Data,uint32_t DataSize,bool EndOfFile)
{
	try
	{
		auto Decoder = PopMp4::DecoderInstances.GetInstance(Instance);
		std::span DataSpan( const_cast<uint8_t*>(Data), DataSize );
		Decoder->PushData( DataSpan );
		if ( EndOfFile )
			Decoder->PushEndOfFile();
		return true;
	}
	catch(std::exception& e)
	{
		std::cerr << __FUNCTION__ << " exception:" << e.what() << std::endl;
		return false;
	}
}


DLL_EXPORT void PopMp4_GetDecoderState(int Instance,char* JsonBuffer,int JsonBufferSize)
{
	try
	{
		auto pInstance = PopMp4::DecoderInstances.GetInstance(Instance);
		auto Meta = pInstance->GetState();
		Meta["Instance"] = Instance;
		//auto Json = Meta.GetJsonString();
		auto Json = json11::Json(Meta).dump();
		StringCopyToBuffer( Json, JsonBuffer, JsonBufferSize );
	}
	catch(std::exception& e)
	{
		std::stringstream Json;
		Json << "{\"Error\":\"" << e.what() << "\"}";
		StringCopyToBuffer( Json.str(), JsonBuffer, JsonBufferSize );
	}
}


/*
extern "C" bool		PopMp4_PopSample(int Instance,bool* EndOfFile,uint8_t* DataBuffer,uint32_t* DataBufferSize,uint64_t* PresentationTimeMs,uint64_t* DecodeTimeMs,uint16_t* Stream,bool* IsKeyframe,uint64_t* DurationMs)
{
	if ( !DataBuffer )
		return false;
		
	auto& Decoder = PopMp4::GetDecoder(Instance);
	auto pNextSample = Decoder.PopSample();
	
	//	out of samples, decoder thread has ended. No more samples to come.
	if ( !pNextSample )
	{
		*EndOfFile = Decoder.HasDecoderThreadFinished();	
		return false;
	}
	
	*EndOfFile = false;
		
	auto& NextSample = *pNextSample;
	
	*Stream = NextSample.mStream;
	*PresentationTimeMs = NextSample.PresentationTimeMs;
	*DecodeTimeMs = NextSample.DecodeTimeMs;
	*IsKeyframe = NextSample.IsKeyframe;
	*DurationMs = NextSample.DurationMs;

	auto RealSize = NextSample.mData.size();
	auto CopySize = std::min<uint64_t>( *DataBufferSize, RealSize );
	memcpy( DataBuffer, NextSample.mData.data(), CopySize );
	*DataBufferSize = RealSize;
	
	return true;
}
*/
