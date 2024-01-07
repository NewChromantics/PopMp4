#include "PopMp4.h"
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include "Mp4Decoder.hpp"
#include <iostream>

/*
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
	TSample(){};
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
	void						OnNewCodec(Codec_t& Codec);
	bool						ReadBytes(std::span<uint8_t>& Buffer,size_t FilePosition);

private:
	std::mutex					mPendingDataLock;
	std::vector<uint8_t>		mPendingData;
	size_t						mPendingDataFilePosition = 0;	//	pendingdata[0] is at this position in the file
	bool						mHadEndOfFile = false;
	Mp4::Parser_t				mParser;					//	the actual data decoder

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
	auto ReadBytes = [&](std::span<uint8_t>& Buffer,size_t FilePosition)
	{
		return this->ReadBytes( Buffer, FilePosition );
	};
	
	auto OnNewCodec = [&](Codec_t& Codec)
	{
		this->OnNewCodec(Codec);
	};
	
	auto OnNewSample = [&](const Sample_t& Sample)
	{
		this->OnNewSample(Sample);
	};

	//	try and read next atom
	return mParser.Read( ReadBytes, OnNewCodec, OnNewSample );
}

void PopMp4::TDecoder::OnNewCodec(Codec_t& Codec)
{
	//	only interested in h264 atm
	if ( Codec.mFourcc != 'avcC' )
		return;
	
	//	no RTTI in UE. static cast is... okay, but not the safest...
	auto& Avc1 = static_cast<CodecAvc1_t&>(Codec);

	//	turn sps & pps into samples
	for ( int i=0;	i<Avc1.mSps.size();	i++ )
	{
		auto& Sps = Avc1.mSps[i].Data;
		std::shared_ptr<TSample> pSample( new TSample() );
		pSample->IsKeyframe = true;
		pSample->mData = Sps;
		pSample->mStream = Codec.mTrackNumber;
		
		if ( mParams.mConvertH264ToAnnexB )
		{
			//	insert nalu header
			//	do we need sps content flag?
			//	https://github.com/NewChromantics/PopCodecs/blob/master/PopH264.cs#L263
			uint8_t Nalu[] = {0,0,0,1};
			pSample->mData.insert( pSample->mData.begin(), &Nalu[0], &Nalu[std::size(Nalu)]);
		}
		
		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
	}
	
	for ( int i=0;	i<Avc1.mPps.size();	i++ )
	{
		auto& Pps = Avc1.mPps[i].Data;
		std::shared_ptr<TSample> pSample( new TSample() );
		pSample->IsKeyframe = true;
		pSample->mData = Pps;
		pSample->mStream = Codec.mTrackNumber;
		
		if ( mParams.mConvertH264ToAnnexB )
		{
			//	insert nalu header
			//	do we need sps content flag?
			//	https://github.com/NewChromantics/PopCodecs/blob/master/PopH264.cs#L263
			uint8_t Nalu[] = {0,0,0,1};
			pSample->mData.insert( pSample->mData.begin(), &Nalu[0], &Nalu[std::size(Nalu)]);
		}
		
		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
	}
}

//	https://stackoverflow.com/questions/24884827/possible-locations-for-sequence-picture-parameter-sets-for-h-264-stream/24890903#24890903
std::string GetH264PacketType(uint8_t NaluType)
{
	switch(NaluType)
	{
		case 0:	return "Unspecified";
		case 1:return "Coded slice of a non-IDR picture";
		case 2:return "Coded slice data partition A";
		case 3:return "Coded slice data partition B";
		case 4:return "Coded slice data partition C";
		case 5:return "Coded slice of an IDR picture";
		case 6:return "Supplemental enhancement information (SEI)";
		case 7:return "Sequence parameter set";
		case 8:return "Picture parameter set";
		case 9:return "Access unit delimiter";
		case 10:return "End of sequence";
		case 11:return "End of stream";
		case 12:return "Filler data";
		case 13:return "Sequence parameter set extension";
		case 14:return "Prefix NAL unit";
		case 15:return "Subset sequence parameter set";
		case 16:return "Depth parameter set";
		case 19:return "Coded slice of an auxiliary coded picture without partitioning";
		case 20:return "Coded slice extension";
		case 21:return "Coded slice extension for depth view components";
	default:break;
	}
	return "Unexpected/invalid nalu type";
}

void PopMp4::TDecoder::OnNewSample(const Sample_t& Sample)
{
	if ( Sample.DataFilePosition == 0 )
		throw std::runtime_error("Sample hasn't resolved file position of data");

	//	grab data
	std::vector<uint8_t> SampleData;
	SampleData.resize( Sample.DataSize );
	DataSpan_t Buffer( SampleData );
	if ( !ReadBytes( Buffer, Sample.DataFilePosition ) )
		throw std::runtime_error("Sample's data [position] hasn't been streamed in yet");
	
	
	if ( !mParams.mConvertH264ToAnnexB )
	{
		std::shared_ptr<TSample> pSample( new TSample(Sample) );
		pSample->mData = SampleData;
		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
		return;
	}

	//	convert to annexb/nalu packets
	//	todo: emulation bit
	//	walk through data, one sample may contain many packets
	if ( Sample.DataPrefixSize != 4 )
		throw std::runtime_error("Todo: handle non 32bit packet prefixes");

	BufferReader_t Reader(0,SampleData);
	while ( Reader.BytesRemaining() )
	{
		//	extract size
		auto Size = Reader.Read32();

		//	extract the following data
		std::shared_ptr<TSample> pSample( new TSample(Sample) );
		pSample->mData = Reader.ReadBytes(Size);
		
		//	debug h264 packet type
		auto H264PacketType = pSample->mData[0] & 0b00011111;
		std::cout << "H264 packet; " << GetH264PacketType(H264PacketType) << std::endl;
		
		//	insert nalu header
		uint8_t Nalu[] = {0,0,0,1};
		pSample->mData.insert( pSample->mData.begin(), &Nalu[0], &Nalu[std::size(Nalu)]);

		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
	}
	
	
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



class PopMp4::TSample : public Sample_t
{
public:
	TSample(const Sample_t& Sample) :
		Sample_t	( Sample )
	{
	}
	TSample(){};
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
	void						OnNewCodec(Codec_t& Codec);
	bool						ReadBytes(std::span<uint8_t>& Buffer,size_t FilePosition);

private:
	std::mutex					mPendingDataLock;
	std::vector<uint8_t>		mPendingData;
	size_t						mPendingDataFilePosition = 0;	//	pendingdata[0] is at this position in the file
	bool						mHadEndOfFile = false;
	Mp4::Parser_t				mParser;					//	the actual data decoder

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
	auto ReadBytes = [&](std::span<uint8_t>& Buffer,size_t FilePosition)
	{
		return this->ReadBytes( Buffer, FilePosition );
	};
	
	auto OnNewCodec = [&](Codec_t& Codec)
	{
		this->OnNewCodec(Codec);
	};
	
	auto OnNewSample = [&](const Sample_t& Sample)
	{
		this->OnNewSample(Sample);
	};

	//	try and read next atom
	return mParser.Read( ReadBytes, OnNewCodec, OnNewSample );
}

void PopMp4::TDecoder::OnNewCodec(Codec_t& Codec)
{
	//	only interested in h264 atm
	if ( Codec.mFourcc != 'avcC' )
		return;
	
	//	no RTTI in UE. static cast is... okay, but not the safest...
	auto& Avc1 = static_cast<CodecAvc1_t&>(Codec);

	//	turn sps & pps into samples
	for ( int i=0;	i<Avc1.mSps.size();	i++ )
	{
		auto& Sps = Avc1.mSps[i].Data;
		std::shared_ptr<TSample> pSample( new TSample() );
		pSample->IsKeyframe = true;
		pSample->mData = Sps;
		pSample->mStream = Codec.mTrackNumber;
		
		if ( mParams.mConvertH264ToAnnexB )
		{
			//	insert nalu header
			//	do we need sps content flag?
			//	https://github.com/NewChromantics/PopCodecs/blob/master/PopH264.cs#L263
			uint8_t Nalu[] = {0,0,0,1};
			pSample->mData.insert( pSample->mData.begin(), &Nalu[0], &Nalu[std::size(Nalu)]);
		}
		
		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
	}
	
	for ( int i=0;	i<Avc1.mPps.size();	i++ )
	{
		auto& Pps = Avc1.mPps[i].Data;
		std::shared_ptr<TSample> pSample( new TSample() );
		pSample->IsKeyframe = true;
		pSample->mData = Pps;
		pSample->mStream = Codec.mTrackNumber;
		
		if ( mParams.mConvertH264ToAnnexB )
		{
			//	insert nalu header
			//	do we need sps content flag?
			//	https://github.com/NewChromantics/PopCodecs/blob/master/PopH264.cs#L263
			uint8_t Nalu[] = {0,0,0,1};
			pSample->mData.insert( pSample->mData.begin(), &Nalu[0], &Nalu[std::size(Nalu)]);
		}
		
		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
	}
}

//	https://stackoverflow.com/questions/24884827/possible-locations-for-sequence-picture-parameter-sets-for-h-264-stream/24890903#24890903
std::string GetH264PacketType(uint8_t NaluType)
{
	switch(NaluType)
	{
		case 0:	return "Unspecified";
		case 1:return "Coded slice of a non-IDR picture";
		case 2:return "Coded slice data partition A";
		case 3:return "Coded slice data partition B";
		case 4:return "Coded slice data partition C";
		case 5:return "Coded slice of an IDR picture";
		case 6:return "Supplemental enhancement information (SEI)";
		case 7:return "Sequence parameter set";
		case 8:return "Picture parameter set";
		case 9:return "Access unit delimiter";
		case 10:return "End of sequence";
		case 11:return "End of stream";
		case 12:return "Filler data";
		case 13:return "Sequence parameter set extension";
		case 14:return "Prefix NAL unit";
		case 15:return "Subset sequence parameter set";
		case 16:return "Depth parameter set";
		case 19:return "Coded slice of an auxiliary coded picture without partitioning";
		case 20:return "Coded slice extension";
		case 21:return "Coded slice extension for depth view components";
	default:break;
	}
	return "Unexpected/invalid nalu type";
}

void PopMp4::TDecoder::OnNewSample(const Sample_t& Sample)
{
	if ( Sample.DataFilePosition == 0 )
		throw std::runtime_error("Sample hasn't resolved file position of data");

	//	grab data
	std::vector<uint8_t> SampleData;
	SampleData.resize( Sample.DataSize );
	DataSpan_t Buffer( SampleData );
	if ( !ReadBytes( Buffer, Sample.DataFilePosition ) )
		throw std::runtime_error("Sample's data [position] hasn't been streamed in yet");
	
	
	if ( !mParams.mConvertH264ToAnnexB )
	{
		std::shared_ptr<TSample> pSample( new TSample(Sample) );
		pSample->mData = SampleData;
		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
		return;
	}

	//	convert to annexb/nalu packets
	//	todo: emulation bit
	//	walk through data, one sample may contain many packets
	if ( Sample.DataPrefixSize != 4 )
		throw std::runtime_error("Todo: handle non 32bit packet prefixes");

	BufferReader_t Reader(0,SampleData);
	while ( Reader.BytesRemaining() )
	{
		//	extract size
		auto Size = Reader.Read32();

		//	extract the following data
		std::shared_ptr<TSample> pSample( new TSample(Sample) );
		pSample->mData = Reader.ReadBytes(Size);
		
		//	debug h264 packet type
		auto H264PacketType = pSample->mData[0] & 0b00011111;
		std::cout << "H264 packet; " << GetH264PacketType(H264PacketType) << std::endl;
		
		//	insert nalu header
		uint8_t Nalu[] = {0,0,0,1};
		pSample->mData.insert( pSample->mData.begin(), &Nalu[0], &Nalu[std::size(Nalu)]);

		std::lock_guard<std::mutex> Lock(mSamplesLock);
		mSamples.push_back(pSample);
	}
	
	
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
	
*/


void PopMp4::Decoder_t::PushData(std::span<uint8_t> Data)
{
	throw std::runtime_error("Todo");
}
