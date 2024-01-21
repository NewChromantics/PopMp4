#include "PopMp4.h"
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include <thread>
#include "Mp4Decoder.hpp"
#include <iostream>
#include "Mp4Parser.hpp" //ExternalReader_t
#include <sstream>


//	fopen_s is a ms specific "safe" func, so provide an alternative
#if !defined(TARGET_WINDOWS)
int fopen_s(FILE **f, const char *name, const char *mode)
{
	//assert(f);
	*f = fopen(name, mode);
	//	Can't be sure about 1-to-1 mapping of errno and MS' errno_t
	if (!*f)
		return errno;
	return 0;
}
#endif

/*


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

void DataSourceBuffer_t::PushData(std::span<uint8_t> Data)
{
	std::scoped_lock Lock( mDataLock );
	std::copy( Data.begin(), Data.end(), std::back_inserter(mData) );
}
	
void DataSourceBuffer_t::PushEndOfFile()
{
	std::scoped_lock Lock( mDataLock );
	mHadEof = true;
}

void DataSourceBuffer_t::LockData(size_t FilePosition,size_t Size,std::function<void(std::span<uint8_t>)> OnPeekData)
{
	std::scoped_lock Lock( mDataLock );
	
	//	if out of bounds and may still have data, throw "not enough data yet" exception
	//	if we've had EOF (and all the data we'll ever get), this is an out of bounds read
	if ( FilePosition + Size > mData.size() )
	{
		if ( mHadEof )
			throw std::runtime_error("MP4 read out of bounds");

		throw TNeedMoreDataException();
	}
	
	auto TheData = std::span(mData);
	TheData = TheData.subspan( FilePosition, Size );
	OnPeekData( TheData );
}


void DataSourceBuffer_t::LockData(std::function<void(std::span<uint8_t>,bool HadEof)> OnPeekData)
{
	std::scoped_lock Lock( mDataLock );
	OnPeekData( mData, mHadEof );
}

bool DataSourceBuffer_t::HadEof()
{
	std::scoped_lock Lock(mDataLock);	
	return mHadEof;
}




DataSourceFile_t::DataSourceFile_t(std::string_view Filename) :
	mFilename	( Filename )
{
	//	gr: hack for now
	if ( mFilename.starts_with("file://") )
		mFilename = mFilename.substr( std::strlen("file://") );
	
	auto ReadThread = [this]()
	{
		try
		{
			ReadFileThread();
		}
		catch(std::exception& e)
		{
			this->OnError(e.what());
		}
		//this->PushEndOfFile()
	};
	mReadFileThread = std::thread( ReadThread );
}

DataSourceFile_t::~DataSourceFile_t()
{
	OnError("Aborted");
	if ( mReadFileThread.joinable() )
		mReadFileThread.join();
	mReadFileThread = {};
}

void DataSourceFile_t::OnError(std::string_view Error)
{
	std::lock_guard Lock(mDataLock);
	mError += Error;
}

void DataSourceFile_t::ThrowIfError()
{
	std::lock_guard Lock(mDataLock);
	if ( mError.empty() )
		return;
	
	throw std::runtime_error(mError);
}

void DataSourceFile_t::ReadFileThread()
{
	auto& Filename = mFilename;
	
	FILE* File = nullptr;
	auto Error = fopen_s(&File,Filename.c_str(), "rb");
	if ( !File )
		throw std::runtime_error( std::string("Failed to open ") + Filename );
	
	std::vector<uint8_t> Buffer(1*1024*1024);
	fseek(File, 0, SEEK_SET);
	while (!feof(File))
	{
		ThrowIfError();
		auto BytesRead = fread( Buffer.data(), 1, Buffer.size(), File );
		auto DataRead = std::span( Buffer.data(), BytesRead );
		mFileReadBufferSource.PushData( DataRead );
	}
	fclose(File);
	mFileReadBufferSource.PushEndOfFile();
}

void DataSourceFile_t::PushData(std::span<uint8_t> Data)
{
	throw std::runtime_error("Cannot push data into a DataSourceFile_t");
}
	
void DataSourceFile_t::PushEndOfFile()
{
	throw std::runtime_error("Cannot push data into a DataSourceFile_t");
}

void DataSourceFile_t::LockData(size_t FilePosition,size_t Size,std::function<void(std::span<uint8_t>)> OnPeekData)
{
	ThrowIfError();
	return mFileReadBufferSource.LockData( FilePosition, Size, OnPeekData );
}


void DataSourceFile_t::LockData(std::function<void(std::span<uint8_t>,bool HadEof)> OnPeekData)
{
	ThrowIfError();
	return mFileReadBufferSource.LockData( OnPeekData );
}

bool DataSourceFile_t::HadEof()
{
	ThrowIfError();
	return mFileReadBufferSource.HadEof();
}



PopMp4::Decoder_t::Decoder_t(PopJson::ViewBase_t& Options)
{
	if ( Options.HasKey("Filename") )
	{
		auto Filename = Options["Filename"].GetString();
		mInputSource.reset( new DataSourceFile_t(Filename) );
	}
	else
	{
		mInputSource.reset( new DataSourceBuffer_t );
	}
	
	auto Thread = [this](void*)
	{
		try
		{
			this->DecoderThread();
		}
		catch(std::exception& e)
		{
			OnError(e);
		}
		mDecoderThreadFinished = true;
	};
	mDecodeThread = std::thread( Thread, this );
}


PopMp4::Decoder_t::~Decoder_t()
{
	//	wait for thread to finish
	mRunThread = false;
	WakeDecoderThread();
	if (mDecodeThread.joinable())
		mDecodeThread.join();
}


void PopMp4::Decoder_t::WakeDecoderThread()
{
	//	notify conditional
}


void PopMp4::Decoder_t::DecoderThread()
{
	while ( mRunThread )
	{
		try
		{
			if ( !DecodeIteration() )
				return;
		}
		catch(TNeedMoreDataException& NeedMoreData)
		{
			//	replace this with wait conditional
			std::this_thread::sleep_for( std::chrono::milliseconds(100) );
		}
	}
}


void PopMp4::Decoder_t::ValidateAtom(Atom_t& Atom)
{
	//	throw if non ascii fourcc
	auto IsAscii = [](uint8_t v)
	{
		return isprint(v);
		//if ( v >= 'a' && v <= 'z' )	return true;
		//if ( v >= 'A' && v <= 'Z' )	return true;
		//if ( v >= 'a' && v <= 'z' )	return true;
	};
	
	//	gr: find some cases where this is wrong!
	auto Fourcc = Atom.Fourcc;
	uint8_t a = (Fourcc>>0) & 0xff;
	uint8_t b = (Fourcc>>8) & 0xff;
	uint8_t c = (Fourcc>>16) & 0xff;
	uint8_t d = (Fourcc>>24) & 0xff;
	std::array<uint8_t,4> abcd = { a,b,c,d };
	bool Valid = true;
	for ( auto x : abcd )
		Valid = Valid && IsAscii(x);
	if ( !Valid )
	{
		std::stringstream Error;
		Error << "Detected invalid fourcc in mp4 atom; " << Atom.GetFourccString() << std::endl;
		throw std::runtime_error(Error.str());
	}
	
	//	throw if size is greater than the file size
	auto AtomDataEnd = Atom.ContentsFilePosition() + Atom.ContentSize();
	if ( this->mInputSource->HadEof() )
	{
		//	todo: probe this with input source
		/*
		//	gr: maybe have a tolerance here for general file or programmer mistakes?
		//		we'r just trying to trap non-mp4 files
		auto Tolerance = 1024;
		if ( AtomDataEnd > mMp4Data.size()+Tolerance )
		{
			std::stringstream Error;
			Error << "Detected mp4 atom (" << Atom.GetFourccString() << ") content size too big for file " << Atom.ContentSize() << "/" << mMp4Data.size();
			throw std::runtime_error(Error.str());
		}
		 */
	}
	
	//	throw if size is massive (just an arbritry limit)
	auto Limit = 1024 * 1024 * 1024 * 1;	//	1gb
	if ( Atom.ContentSize() > Limit )
	{
		std::stringstream Error;
		Error << "Detected mp4 atom (" << Atom.GetFourccString() << ") content size too big (arbritry size limit) " << Atom.ContentSize() << "/" << Limit;
		throw std::runtime_error(Error.str());
	}
}

json11::Json::object GetAtomMeta(Atom_t& Atom)
{
	json11::Json::object AtomJson;
	AtomJson["Fourcc"] = Atom.GetFourccString();
	AtomJson["AtomSizeBytes"] = (int)Atom.AtomSize();
	AtomJson["HeaderSizeBytes"] = (int)Atom.HeaderSize();
	AtomJson["ContentSizeBytes"] = (int)Atom.ContentSize();
	AtomJson["ContentsFilePosition"] = (int)Atom.ContentsFilePosition();

	json11::Json::array AtomChildrenJson;
	for ( auto& Child : Atom.mChildAtoms )
	{
		auto ChildJson = GetAtomMeta( Child );
		AtomChildrenJson.push_back( ChildJson );
	}
	
	if ( !AtomChildrenJson.empty() )
	{
		AtomJson["Children"] = AtomChildrenJson;
	}

	return AtomJson;
}


bool PopMp4::Decoder_t::DecodeIteration()
{
	//	would be good to have a functor for ExternalReader which locks the data rather than needing to copy
	auto ReadFileBytes = [this](std::span<uint8_t> FillBuffer,size_t FilePosition)
	{
		auto OnLock = [&](std::span<uint8_t> FileData)
		{
			std::copy( FileData.begin(), FileData.end(), FillBuffer.begin() );
		};
		mInputSource->LockData( FilePosition, FillBuffer.size(), OnLock );
		return true;
	};
	
	
	//	if we've had EOF and read all the bytes, we can finish
	bool IsFinished = false;
	auto OnLockedAllData = [&](std::span<uint8_t> AllData,bool HadEof)
	{
		IsFinished = mMp4BytesParsed == AllData.size_bytes() && HadEof;
	};
	mInputSource->LockData(OnLockedAllData);
	if ( IsFinished )
		return false;
	
	ExternalReader_t Reader(mMp4BytesParsed,ReadFileBytes);

	//	gr: due to race conditions, it's possible that we've reached EOF after the check above
	//		but inside ReadNextAtom, so catch a bad read here and if it turns out we're "now" at
	//		the end of the file, dont report an error
	Atom_t Atom;
	try
	{
		Atom = Reader.ReadNextAtom();
		ValidateAtom(Atom);
		
		//	should be able to read children here
		try
		{
			Atom.DecodeChildAtoms( ReadFileBytes );
		}
		catch(TNeedMoreDataException& e)
		{
			//	this will loop around
			throw;
		}
		catch(std::exception& e)
		{
			//	maybe child isn't full of child atoms
			std::cerr << "Failed to get children in atom " << Atom.GetFourccString() << "; " << e.what() << std::endl;
		}
		
		//	this will read the contents, if it fails because we need more data, we won't move along the bytes read yet
		//ProcessAtom( Atom, ReadFileBytes );
		std::scoped_lock Lock(mDataLock);
		mExtractedMp4RootAtoms.push_back(Atom.Fourcc);
	}
	catch(TNeedMoreDataException& e)
	{
		return true;
	}
	catch(std::exception& e)
	{
		throw;
		/*
		 //	we've had our end of file...
		 if ( mHadEndOfFile )
		 {
		 //	and the parser has read all our data, so no more to decode
		 auto PendingDataEnd = mPendingDataFilePosition + mPendingData.size();
		 if ( mParser.mFilePosition >= PendingDataEnd )
		 return false;
		 }
		 */
	}
	
	//	save meta
	{
		auto AtomJson = GetAtomMeta( Atom );
		mAtomTree.push_back(AtomJson);
	}
	
	//	move onto next atom (this skips data, even if we've not downloaded it yet)
	mMp4BytesParsed += Atom.AtomSize();
		
	return true;
}

void PopMp4::Decoder_t::OnError(std::string_view Error)
{
	std::scoped_lock Lock(mDataLock);
	mError = Error;
}


void PopMp4::Decoder_t::PushData(std::span<uint8_t> Data)
{
	auto InputSource = mInputSource;
	InputSource->PushData(Data);
	WakeDecoderThread();
}

void PopMp4::Decoder_t::PushEndOfFile()
{
	auto InputSource = mInputSource;
	InputSource->PushEndOfFile();
	WakeDecoderThread();
}

json11::Json::object PopMp4::Decoder_t::GetState()
{
	json11::Json::object Meta;
	
	std::scoped_lock Lock(mDataLock);
	if ( !mError.empty() )
	{
		Meta["Error"] = mError;
	}
	
	Meta["IsFinished"] = mDecoderThreadFinished;

	json11::Json::array RootAtoms;
	for ( auto ExtractedMp4RootAtom : mExtractedMp4RootAtoms )
	{
		RootAtoms.push_back( GetFourccString(ExtractedMp4RootAtom,true) );
	}
	//Meta["RootAtoms"].PushBack( mExtractedMp4RootAtoms, [&](const uint32_t& Fourcc){	return GetFourccString(Fourcc,true);	} );
	Meta["RootAtoms"] = RootAtoms;

	Meta["Mp4BytesParsed"] = (int)mMp4BytesParsed;
	Meta["AtomTree"] = mAtomTree;

	return Meta;
}
