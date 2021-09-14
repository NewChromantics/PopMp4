#pragma once

#include <stdint.h>
#include <functional>
#include <string>
#include <vector>

#define __noexcept _NOEXCEPT

class TNeedMoreDataException : public std::exception
{
public:
	virtual const char* what() const __noexcept { return "Invalid device name"; }
};



//	replace with std::span, but that's c++20
class DataSpan_t
{
public:
	DataSpan_t(){}
	
	//	hacky! support C arrays etc
	template<typename T>
	DataSpan_t(T& Value) :
		Buffer		( reinterpret_cast<uint8_t*>(&Value) ),
		BufferSize	( sizeof(T) )
	{
	}
	
	uint8_t*	Buffer = nullptr;
	size_t		BufferSize = 0;
};

typedef std::function<bool(DataSpan_t&,size_t)> ReadBytesFunc_t;
class Atom_t;

class DataReader_t
{
public:
	DataReader_t(uint64_t ExternalFilePosition,ReadBytesFunc_t ReadBytes) :
		mReadBytes				( ReadBytes ),
		mExternalFilePosition	( ExternalFilePosition )
	{
	}

	Atom_t					ReadNextAtom();
	uint8_t					Read8();
	uint32_t				Read24();
	uint32_t				Read32();
	uint64_t				Read64();
	std::vector<uint8_t>	ReadBytes(size_t Size);
	std::string				ReadString(int Length);

private:
	//	calls read, throws if data missing, moves along file pos
	void					Read(DataSpan_t& Buffer);
	
public:
	uint64_t		mFilePosition = 0;
	uint64_t		mExternalFilePosition = 0;
	ReadBytesFunc_t	mReadBytes;
};


class BufferReader_t : public DataReader_t
{
public:
	BufferReader_t(uint64_t ExternalFilePosition,std::vector<uint8_t> Contents);

	bool	ReadBytes(DataSpan_t&,size_t);
	int		BytesRemaining()	{	return mContents.size() - mFilePosition;	}
	
	//	need to make a copy atm
	std::vector<uint8_t>	mContents;
};

class Atom_t
{
public:
	std::string	Fourcc;
	uint32_t	Size = 0;
	uint64_t	Size64 = 0;
	uint64_t	FilePosition = 0;
	std::vector<Atom_t>	mChildAtoms;

	void		DecodeChildAtoms(ReadBytesFunc_t ReadBytes);
	
	//	fetch contents on demand
	std::vector<uint8_t>	GetContents(ReadBytesFunc_t ReadBytes);
	BufferReader_t			GetContentsReader(ReadBytesFunc_t ReadBytes);
	std::vector<Atom_t*>	GetChildAtoms(const std::string& MatchFourcc);
	Atom_t&					GetChildAtomRef(const std::string& MatchFourcc);	//	 expect & match one-instance of this child atom	
	Atom_t*					GetChildAtom(const std::string& MatchFourcc);	//	 expect & match one-instance of this child atom	

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
	uint64_t	DataPosition = 0;
	uint64_t	DataFilePosition = 0;
	uint64_t	DataSize = 0;
};

//	this is the actual mp4 decoder
//	based heavily on https://github.com/NewChromantics/PopEngineCommon/blob/master/Mp4.js
class Mp4Parser_t
{
public:
	bool						Read(ReadBytesFunc_t ReadBytes,std::function<void(const Sample_t&)> EnumNewSample);
	
	void						DecodeAtom_Moov(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void						DecodeAtom_Trak(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void						DecodeAtom_Media(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void						DecodeAtom_MediaInfo(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::vector<Sample_t>		DecodeAtom_SampleTable(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::vector<ChunkMeta_t>	DecodeAtom_ChunkMetas(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::vector<uint64_t>		DecodeAtom_ChunkOffsets(Atom_t* ChunkOffsets32Atom,Atom_t* ChunkOffsets64Atom,ReadBytesFunc_t ReadBytes);
	std::vector<uint64_t>		DecodeAtom_SampleSizes(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	std::vector<bool>			DecodeAtom_SampleKeyframes(Atom_t* pAtom,int SampleCount,ReadBytesFunc_t ReadBytes);
	std::vector<uint64_t>		DecodeAtom_SampleDurations(Atom_t* pAtom,int SampleCount,int Default,ReadBytesFunc_t ReadBytes);
	
	void						OnSamples(std::vector<Sample_t>& NewSamples);
	
public:
	uint64_t				mFilePosition = 0;
	
	std::vector<Sample_t>	mNewSamples;
	std::vector<Atom_t>		mDatAtoms;	//	hold onto mdat in case they appear before moof/moovs
};
