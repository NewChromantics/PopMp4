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
	std::vector<uint8_t>	Data;
	
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
class Mp4Parser_t
{
public:
	bool		Read(ReadBytesFunc_t ReadBytes);
	
	uint64_t	mFilePosition = 0;
};
