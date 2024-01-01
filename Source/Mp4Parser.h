#pragma once

#include <stdint.h>
#include <functional>
#include <string>
#include <vector>
#include <map>
#include <cmath>	//	floorf
#include <memory>
#include <span>


std::string	GetFourccString(uint32_t Fourcc,bool Reversed);

class TNeedMoreDataException : public std::exception
{
public:
	const char* what() const noexcept override { return "NeedMoreData"; }
};


//	ReadBytes( FillThisBufferSize, FilePosition )
typedef std::function<bool(std::span<uint8_t>,size_t)> ReadBytesFunc_t;
class Atom_t;

class DataReader_t
{
public:
	DataReader_t(uint64_t ExternalFilePosition) :
		mExternalFilePosition	( ExternalFilePosition )
	{
	}
	virtual ~DataReader_t() {};

	Atom_t					ReadNextAtom();
	uint8_t					Read8();
	uint16_t				Read16();
	uint32_t				Read24();
	uint32_t				Read32();
	uint32_t				Read32Reversed();
	uint64_t				Read64();
	std::vector<uint8_t>	ReadBytes(size_t Size);
	std::string				ReadString(int Length);

private:
	void					Read(std::span<uint8_t> Buffer);

protected:
	virtual bool			ReadFileBytes(std::span<uint8_t> Buffer, size_t FilePositon) = 0;

public:
	uint64_t		mFilePosition = 0;
	uint64_t		mExternalFilePosition = 0;
};


class ExternalReader_t : public DataReader_t
{
public:
	ExternalReader_t(uint64_t ExternalFilePosition, ReadBytesFunc_t ReadBytes) :
		mReadBytes		(ReadBytes),
		DataReader_t	(ExternalFilePosition)
	{
	}

	virtual bool		ReadFileBytes(std::span<uint8_t> Buffer,size_t FilePositon) override;

private:
	ReadBytesFunc_t		mReadBytes;
};

class BufferReader_t : public DataReader_t
{
public:
	BufferReader_t(uint64_t ExternalFilePosition, std::vector<uint8_t> Contents);

	virtual bool			ReadFileBytes(std::span<uint8_t>,size_t FilePositon) override;

	int						BytesRemaining()	{	return mContents.size() - mFilePosition;	}

	//	need to make a copy atm
	std::vector<uint8_t>	mContents;
};


//	make BufferReader_t use this
class ViewReader_t : public DataReader_t
{
public:
	ViewReader_t(uint64_t ExternalFilePosition,std::span<uint8_t> Contents);

	virtual bool			ReadFileBytes(std::span<uint8_t>,size_t FilePositon) override;

	int						BytesRemaining()	{	return mContents.size() - mFilePosition;	}
	
	std::span<uint8_t>		mContents;
};

class TBitReader
{
public:
	TBitReader(std::function<uint8_t(size_t)> GetNthByte) :
		mGetByte	( GetNthByte )
	{
	}
	
	bool						Read(int& Data,int BitCount);
	bool						Read(uint64_t& Data,int BitCount);
	bool						Read(uint8_t& Data,int BitCount);
	int							Read(int BitCount)					{	int Data;	return Read( Data, BitCount) ? Data : -1;	}
	size_t						BitPosition() const					{	return mBitPos;	}
	size_t						BytesRead() const
	{
		auto RoundUpBits = (8 - (mBitPos % 8)) % 8;
		return (mBitPos+RoundUpBits) / 8;
	}

	template<int BYTES,typename STORAGE>
	bool						ReadBytes(STORAGE& Data,int BitCount);

private:
	std::function<uint8_t(size_t)>	mGetByte;
	unsigned int				mBitPos = 0;	//	current bit-to-read/write-pos (the tail)
};



class Atom_t
{
public:
	uint32_t	Fourcc = 0;
	uint32_t	Size = 0;
	uint64_t	Size64 = 0;
	uint64_t	FilePosition = 0;
	std::vector<Atom_t>	mChildAtoms;

	void					DecodeChildAtoms(ReadBytesFunc_t ReadBytes);
	
	//	fetch contents on demand
	std::vector<uint8_t>	GetContents(ReadBytesFunc_t ReadBytes);
	BufferReader_t			GetContentsReader(ReadBytesFunc_t ReadBytes);
	std::vector<Atom_t*>	GetChildAtoms(uint32_t MatchFourcc);
	Atom_t&					GetChildAtomRef(uint32_t MatchFourcc);	//	 expect & match one-instance of this child atom
	Atom_t*					GetChildAtom(uint32_t MatchFourcc);	//	 expect & match one-instance of this child atom

	std::string	GetFourccString()	{	return ::GetFourccString( Fourcc, true );	}
	uint64_t	ContentsFilePosition()	{	return FilePosition + HeaderSize();	}
	uint64_t	AtomSize()		{	return (Size==1) ? Size64 : Size;	}
	uint64_t	ContentSize()	{	return AtomSize() - HeaderSize();	}
	uint64_t	HeaderSize()	
	{
		uint64_t HeaderSize = 32/8;	//	.Size
		HeaderSize += 4;	//	.Fourcc
		if ( Size == 1 )
			HeaderSize += 64/8;	//	.Size64
		return HeaderSize;
	}
	
};


class ChunkMeta_t
{
public:
	uint32_t	FirstChunk = 0;
	uint32_t	SamplesPerChunk = 0;
	uint32_t	SampleDescriptionId = 0;
};

class Sample_t
{
public:
	uint64_t	PresentationTimeMs = 0;
	uint64_t	DecodeTimeMs = 0;
	uint64_t	DurationMs = 0;
	bool		IsKeyframe = false;
	uint64_t	DataSize = 0;
	uint32_t	DataPrefixSize = 0;	//	amount of bytes which is a prefix (eg. length of nalu packet)

	//	data position is either file-relative (moov+mdat files, mdat may be before or after)
	//	or relative to the next-mdat (probably fragmented)
	//	file position will never be zero, so we can assume if it's 0, it's not been resolved yet
	//	fragmented files can resolve mdat when they can
	uint64_t	DataFilePosition = 0;
	uint64_t	MdatPosition = 0;
	int			MdatIndex = -1;
};

class MovieHeader_t	//	from mvhd atom
{
public:
	uint64_t	TimeScaleUnitsPerSecond = 1000;	//	units per second
	uint64_t	DurationMs = 0;
	uint64_t	CreationTimeMs = 0;
	uint64_t	ModificationTimeMs = 0;
	uint64_t	PreviewDurationMs = 0;
	
	uint64_t	TimeUnitsToMs(uint64_t TimeUnit)
	{
		float Secs = TimeUnit / static_cast<float>(TimeScaleUnitsPerSecond);
		float Ms = Secs * 1000.f;
		return static_cast<uint64_t>( floorf(Ms) );
	}
};

class Sps_t
{
public:
	std::vector<uint8_t>	Data;
};

class Pps_t
{
public:
	std::vector<uint8_t>	Data;
};

class Codec_t
{
public:
	virtual ~Codec_t()	{};
	
	virtual std::string	GetName();
	virtual uint32_t	GetSampleDataPrefixSize()	{	return 0;	}
	
	uint32_t		mFourcc = 0;
};

class CodecAvc1_t : public Codec_t
{
public:
	static constexpr uint32_t	Fourcc = 'avc1';
public:
	CodecAvc1_t(DataReader_t& DataReader);
	
	virtual uint32_t	GetSampleDataPrefixSize() override	{	return mLengthMinusOne+1;	}

	uint8_t				mProfile = 0;
	uint8_t				mCompatbility = 0;
	uint8_t				mLevel = 0;
	uint8_t				mLengthMinusOne = 0;
	std::vector<Sps_t>	mSps;
	std::vector<Pps_t>	mPps;
};

class MediaHeader_t
{
public:
	MediaHeader_t(MovieHeader_t MovieHeader) :
		MovieHeader	( MovieHeader )
	{
	}
	
	//	media can have it's own timescale
	uint64_t		TimeScaleUnitsPerSecond = 1000;	//	units per second
	uint64_t		CreationTimeMs = 0;
	uint64_t		ModificationTimeMs = 0;
	uint64_t		LanguageId = 0;
	float			Quality = 1.f;
	uint64_t		DurationMs = 0;
	
	//	gr: do we need to include the movie header?
	MovieHeader_t	MovieHeader;
	std::shared_ptr<Codec_t>	Codec;
	int				TrackNumber = 0;
	
	uint64_t	TimeUnitsToMs(uint64_t TimeUnit)
	{
		float Secs = TimeUnit / static_cast<float>(TimeScaleUnitsPerSecond);
		float Ms = Secs * 1000.f;
		return static_cast<uint64_t>( floorf(Ms) );
	}
};

//	this is the actual mp4 decoder
//	based heavily on https://github.com/NewChromantics/PopEngineCommon/blob/master/Mp4.js
namespace Mp4
{
	void						DecodeAtom_Moov(Atom_t& Atom,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&,MediaHeader_t&)> OnSamples);
	MovieHeader_t				DecodeAtom_MovieHeader(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void						DecodeAtom_Trak(Atom_t& Atom,int TrackNumber,MovieHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&,MediaHeader_t&)> OnSamples);
	void						DecodeAtom_Media(Atom_t& Atom,int TrackNumber,MovieHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&,MediaHeader_t&)> OnSamples);
	MediaHeader_t				DecodeAtom_MediaHeader(Atom_t& Atom,MovieHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes);
	void						DecodeAtom_MediaInfo(Atom_t& Atom,MediaHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&,MediaHeader_t&)> OnSamples);
	std::shared_ptr<Codec_t>	DecodeAtom_SampleDescription(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::shared_ptr<Codec_t>	DecodeAtom_Avc1(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::vector<Sample_t>		DecodeAtom_SampleTable(Atom_t& Atom,MediaHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes);
	std::vector<ChunkMeta_t>	DecodeAtom_ChunkMetas(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::vector<uint64_t>		DecodeAtom_ChunkOffsets(Atom_t* ChunkOffsets32Atom,Atom_t* ChunkOffsets64Atom,ReadBytesFunc_t ReadBytes);
	std::vector<uint64_t>		DecodeAtom_SampleSizes(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::vector<bool>			DecodeAtom_SampleKeyframes(Atom_t* pAtom,int SampleCount,ReadBytesFunc_t ReadBytes);
	std::vector<uint64_t>		DecodeAtom_SampleDurations(Atom_t* pAtom,int SampleCount,int Default,ReadBytesFunc_t ReadBytes);

	class Parser_t;
}

class Mp4::Parser_t
{
public:
	bool						Read(ReadBytesFunc_t ReadBytes,std::function<void(Codec_t&)> EnumNewCodec,std::function<void(const Sample_t&)> EnumNewSample);

	void						OnCodecHeader(std::shared_ptr<Codec_t> Codec);
	void						OnSamples(std::vector<Sample_t>& NewSamples);
	
public:
	uint64_t				mFilePosition = 0;
	
	std::map<int,std::shared_ptr<Codec_t>>	mTrackCodecs;
	std::vector<std::shared_ptr<Codec_t>>	mNewCodecs;
	std::vector<Sample_t>		mNewSamples;
	std::vector<Atom_t>			mDatAtoms;	//	hold onto mdat in case they appear before moof/moovs
};
