#include "PopMp4.hpp"
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include "Mp4Parser.hpp"
#include <iostream>

namespace PopMp4
{
	class TSample;
	class TDecoder;
	
	std::map<int,std::shared_ptr<TDecoder>>	Decoders;
	int		LastInstanceIdent = 1000;
	
	TDecoder&	GetDecoder(int Instance);
}

class PopMp4::TSample
{
public:
	uint64_t				mTimestamp = 0;
	uint16_t				mStream = 0;
	std::vector<uint8_t>	mData;
};

class PopMp4::TDecoder
{
public:
	TDecoder();
	~TDecoder();

	void						PushData(const uint8_t* Data,size_t DataSize,bool EndOfFile);	//	std::span would be better, c++20
	std::shared_ptr<TSample>	PopSample();
	bool						HasEndOfFile()	{	return mHadEndOfFile && mPendingData.empty();	};	//	no more data to process
	
private:
	void						Thread();
	void						WakeThread();

	//	this function is basically the only mp4-specific interface,
	//	if we wanted to support more containers, do it here
	bool						DecodeNext();	//	returns true if we decoded something, false if we need more data
	
public:
	std::mutex					mPendingDataLock;
	std::vector<uint8_t>		mPendingData;
	size_t						mPendingDataFilePosition = 0;	//	pendingdata[0] is at this position in the file
	bool						mHadEndOfFile = false;
	Mp4Parser_t					mParser;					//	the actual data decoder

	std::mutex					mSamplesLock;
	std::vector<std::shared_ptr<TSample>>	mSamples;
	
	bool						mRunThread = true;
	std::thread					mDecodeThread;
};


PopMp4::TDecoder::TDecoder()
{
	auto Thread = [this](void*)
	{
		this->Thread();
	};
	mDecodeThread = std::thread( Thread, this );
}

PopMp4::TDecoder::~TDecoder()
{
	//	wait for thread to finish
	mRunThread = false;
	WakeThread();
	if (mDecodeThread.joinable())
		mDecodeThread.join();
}

void PopMp4::TDecoder::WakeThread()
{
	//	notify conditional
}

void PopMp4::TDecoder::Thread()
{
	while ( mRunThread )
	{
		bool NeedMoreData = !DecodeNext();
		
		//	no more data to process, and none coming
		if ( HasEndOfFile() )
			break;

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
	WakeThread();
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
	};
	
	//	try and read next atom
	return mParser.Read( ReadBytes );
}


PopMp4::TDecoder& PopMp4::GetDecoder(int Instance)
{
	auto pDecoder = Decoders.at(Instance);	//	throws if missing
	return *pDecoder;
}


extern "C" int PopMp4_CreateDecoder()
{
	auto Instance = PopMp4::LastInstanceIdent++;
	std::shared_ptr<PopMp4::TDecoder> Decoder( new PopMp4::TDecoder );
	
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

extern "C" bool		PopMp4_PopSample(int Instance,uint64_t* Timestamp,uint16_t* Stream,uint8_t* DataBuffer,uint32_t* DataBufferSize,bool* EndOfFile)
{
	if ( !DataBuffer )
		return false;
		
	auto& Decoder = PopMp4::GetDecoder(Instance);
	auto pNextSample = Decoder.PopSample();
	if ( !pNextSample )
	{
		*EndOfFile = Decoder.HasEndOfFile();	
		return false;
	}
	
	*EndOfFile = false;
		
	auto& NextSample = *pNextSample;
	
	*Stream = NextSample.mStream;
	*Timestamp = NextSample.mTimestamp;
	
	auto RealSize = NextSample.mData.size();
	auto CopySize = std::min<uint64_t>( *DataBufferSize, RealSize );
	memcpy( DataBuffer, NextSample.mData.data(), CopySize );
	*DataBufferSize = RealSize;
	
	return true;
}

