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


class Atom_t
{
public:
	std::string	Fourcc;
	uint32_t	Size = 0;
	uint64_t	Size64 = 0;
	uint64_t	FilePosition = 0;
	std::vector<Atom_t>	mChildAtoms;

	void		DecodeChildAtoms(ReadBytesFunc_t ReadBytes);
	void		DecodeChildAtoms(std::vector<uint8_t>& Contents);
	
	//	fetch contents on demand
	std::vector<uint8_t>	GetContents(ReadBytesFunc_t ReadBytes);
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

//	this is the actual mp4 decoder
//	based heavily on https://github.com/NewChromantics/PopEngineCommon/blob/master/Mp4.js
class Mp4Parser_t
{
public:
	bool				Read(ReadBytesFunc_t ReadBytes);
	
	void				DecodeAtom_Moov(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void				DecodeAtom_Trak(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void				DecodeAtom_Media(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void				DecodeAtom_MediaInfo(Atom_t& Atom,ReadBytesFunc_t ReadBytes);
	void				DecodeAtom_SampleTable(Atom_t& Atom,ReadBytesFunc_t ReadBytes);

public:
	uint64_t			mFilePosition = 0;
	
	std::vector<Atom_t>	mDatAtoms;	//	hold onto mdat in case they appear before moof/moovs
};
