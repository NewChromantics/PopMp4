#include "PopMp4.h"
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include "Mp4Parser.h"
#include <iostream>

namespace PopMp4
{
	class TSample;
	class TDecoder;
	class TDecoderParams;
	
	std::map<int,std::shared_ptr<TDecoder>>	Decoders;
	int		LastInstanceIdent = 1000;
	
	TDecoder&	GetDecoder(int Instance);
}

class PopMp4::TSample : public Sample_t
{
public:
	TSample(const Sample_t& Sample) :
		Sample_t	( Sample )
	{
	}
	uint16_t				mStream = 0;
	std::vector<uint8_t>	mData;
};


class PopMp4::TDecoderParams
{
public:
	bool	mConvertH264ToAnnexB = true;	//	data in mp4s will be prefixed with length (8,16 or 32bit). Enable this to convert sample data to Nalu-prefixed annexb 0 0 0 1 (no length)
};

class PopMp4::TDecoder
{
public:
	TDecoder(TDecoderParams Params);
	~TDecoder();

	void						PushData(const uint8_t* Data,size_t DataSize,bool EndOfFile);	//	std::span would be better, c++20
	std::shared_ptr<TSample>	PopSample();
	bool						HasDecoderThreadFinished()	{	return mDecoderThreadFinished;	}
	
private:
	void						DecoderThread();
	void						WakeDecoderThread();

	//	this function is basically the only mp4-specific interface,
	//	if we wanted to support more containers, do it here
	bool						DecodeNext();	//	returns true if we decoded something, false if we need more data
	
	void						OnNewSample(const Sample_t& Sample);
	bool						ReadBytes(DataSpan_t& Buffer,size_t FilePosition);

private:
	std::mutex					mPendingDataLock;
	std::vector<uint8_t>		mPendingData;
	size_t						mPendingDataFilePosition = 0;	//	pendingdata[0] is at this position in the file
	bool						mHadEndOfFile = false;
	Mp4Parser_t					mParser;					//	the actual data decoder

	TDecoderParams				mParams;

	std::mutex					mSamplesLock;
	std::vector<std::shared_ptr<TSample>>	mSamples;
	
	bool						mRunThread = true;
	std::thread					mDecodeThread;
	bool						mDecoderThreadFinished = false;	//	will parse no more data
};


PopMp4::TDecoder::TDecoder(TDecoderParams Params) :
	mParams	(Params)
{
	auto Thread = [this](void*)
	{
		this->DecoderThread();
		mDecoderThreadFinished = true;
	};
	mDecodeThread = std::thread( Thread, this );
}

PopMp4::TDecoder::~TDecoder()
{
	//	wait for thread to finish
	mRunThread = false;
	WakeDecoderThread();
	if (mDecodeThread.joinable())
		mDecodeThread.join();
}

void PopMp4::TDecoder::WakeDecoderThread()
{
	//	notify conditional
}


void PopMp4::TDecoder::DecoderThread()
{
	while ( mRunThread )
	{
		bool NeedMoreData = !DecodeNext();
		
		//	we've had our end of file...
		if ( mHadEndOfFile )
		{
			//	and the parser has read all our data, so no more to decode
			auto PendingDataEnd = mPendingDataFilePosition + mPendingData.size();
			if ( mParser.mFilePosition >= PendingDataEnd )
				break;
		}

		if ( NeedMoreData )
		{
			//	replace this with wait conditional
			std::this_thread::sleep_for( std::chrono::milliseconds(100) );
		}
	}
}


void PopMp4::TDecoder::PushData(const uint8_t* Data,size_t DataSize,bool EndOfFile)
{
	{
		std::lock_guard<std::mutex> Lock( mPendingDataLock );
		
		if ( EndOfFile )
			mHadEndOfFile = EndOfFile;
		
		if ( DataSize )
			mPendingData.insert( mPendingData.end(), Data, Data+DataSize );
	}
	WakeDecoderThread();
}

std::shared_ptr<PopMp4::TSample> PopMp4::TDecoder::PopSample()
{
	std::lock_guard<std::mutex> Lock( mSamplesLock );
	if ( mSamples.size() == 0 )
		return nullptr;
	
	auto Popped = mSamples.front();
	mSamples.erase( mSamples.begin() );
	return Popped;
}

bool PopMp4::TDecoder::DecodeNext()
{
	auto ReadBytes = [&](DataSpan_t& Buffer,size_t FilePosition)
	{
		return this->ReadBytes( Buffer, FilePosition );
	};
	
	auto OnNewSample = [&](const Sample_t& Sample)
	{
		this->OnNewSample(Sample);
	};

	//	try and read next atom
	return mParser.Read( ReadBytes, OnNewSample );
}

void PopMp4::TDecoder::OnNewSample(const Sample_t& Sample)
{
	if ( Sample.DataFilePosition == 0 )
		throw std::runtime_error("Sample hasn't resolved file position of data");

	std::shared_ptr<TSample> pSample( new TSample(Sample) );
	//	grab data
	pSample->mData.resize( Sample.DataSize );
	DataSpan_t Buffer( pSample->mData );
	if ( !ReadBytes( Buffer, Sample.DataFilePosition ) )
		throw std::runtime_error("Sample's data [position] hasn't been streamed in yet");
	
	std::lock_guard<std::mutex> Lock(mSamplesLock);
	mSamples.push_back(pSample);
}

bool PopMp4::TDecoder::ReadBytes(DataSpan_t& Buffer,size_t FilePosition)
{
	std::lock_guard<std::mutex> Lock( mPendingDataLock );
	
	//	requesting data we've dropped!
	if ( FilePosition < mPendingDataFilePosition )
		throw std::runtime_error("Requesting data we've dropped");
	
	//	requesting data we don't have [yet]
	auto PendingDataEnd = mPendingDataFilePosition + mPendingData.size();
	if ( FilePosition + Buffer.BufferSize > PendingDataEnd )
	{
		std::cout << "Requesting data out of range" << std::endl;
		return false;
	}
	
	//	copy
	for ( int i=0;	i<Buffer.BufferSize;	i++ )
	{
		int p = FilePosition - mPendingDataFilePosition + i;
		Buffer.Buffer[i] = mPendingData[p];
	}
	return true;	
}
	
PopMp4::TDecoder& PopMp4::GetDecoder(int Instance)
{
	auto pDecoder = Decoders.at(Instance);	//	throws if missing
	return *pDecoder;
}


extern "C" int PopMp4_CreateDecoder()
{
	auto Instance = PopMp4::LastInstanceIdent++;
	
	PopMp4::TDecoderParams Params;
	std::shared_ptr<PopMp4::TDecoder> Decoder( new PopMp4::TDecoder(Params) );
	
	//	todo: make sure key isnt used
	PopMp4::Decoders[Instance] = Decoder;
	return Instance;
}

extern "C" void		PopMp4_DestroyDecoder(int Instance)
{
	PopMp4::Decoders.erase( Instance );
}

extern "C" bool		PopMp4_PushMp4Data(int Instance,const uint8_t* Data,uint32_t DataSize,bool EndOfFile)
{
	auto& Decoder = PopMp4::GetDecoder(Instance);
	Decoder.PushData( Data, DataSize, EndOfFile );
	return true;
}


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

