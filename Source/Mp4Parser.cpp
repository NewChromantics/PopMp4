#include "Mp4Parser.hpp"
#include <iostream>

//	https://stackoverflow.com/a/61146679/355753
template <typename T>
void swapEndian(T& buffer)
{
	static_assert(std::is_pod<T>::value, "swapEndian support POD type only");
	char* startIndex = reinterpret_cast<char*>(&buffer);
	char* endIndex = startIndex + sizeof(buffer);
	std::reverse(startIndex, endIndex);
}


class DataReader_t
{
public:
	DataReader_t(uint64_t ExternalFilePosition,ReadBytesFunc_t ReadBytes) :
		mReadBytes				( ReadBytes ),
		mExternalFilePosition	( ExternalFilePosition )
	{
	}

	Atom_t					ReadNextAtom();
	uint32_t				Read32();
	uint64_t				Read64();
	std::vector<uint8_t>	ReadBytes(size_t Size);
	std::string				ReadString(int Length);

private:
	//	calls read, throws if data missing, moves along file pos
	void					Read(DataSpan_t& Buffer);
	
private:
	uint64_t		mFilePosition = 0;
	uint64_t		mExternalFilePosition = 0;
	ReadBytesFunc_t	mReadBytes;
};


Atom_t DataReader_t::ReadNextAtom()
{
	Atom_t Atom;
	Atom.FilePosition = mFilePosition + mExternalFilePosition;
	Atom.Size = Read32();
	Atom.Fourcc = ReadString(4);
	
	//	size of 1 means 64 bit size
	if ( Atom.Size == 1 )
	{
		Atom.Size64 = Read64();
	}
	
	if ( Atom.AtomSize() < 8 )
		throw std::runtime_error("Atom reported size as less than 8 bytes; not possible.");
		
	Atom.Data = ReadBytes( Atom.ContentSize() );
	
	return Atom;
}

uint32_t DataReader_t::Read32()
{
	uint32_t Value = 0;
	DataSpan_t Data( Value );
	Read( Data );
	swapEndian(Value);
	return Value;
}

uint64_t DataReader_t::Read64()
{
	uint64_t Value = 0;
	DataSpan_t Data( Value );
	Read( Data );
	swapEndian(Value);
	return Value;
}

std::vector<uint8_t> DataReader_t::ReadBytes(size_t Size)
{
	std::vector<uint8_t> Buffer;
	Buffer.resize(Size);
	DataSpan_t Data;
	Data.Buffer = Buffer.data();
	Data.BufferSize = Buffer.size();
	Read( Data );
	return Buffer;
}

std::string DataReader_t::ReadString(int Length)
{
	auto Bytes = ReadBytes(Length);
	char* Chars = reinterpret_cast<char*>(Bytes.data());
	return std::string( Chars, Length );
}

void DataReader_t::Read(DataSpan_t& Buffer)
{
	auto ReadPos = mFilePosition+mExternalFilePosition;
	if ( !mReadBytes( Buffer, ReadPos ) )
		throw TNeedMoreDataException();
	
	//	move file pos along
	mFilePosition += Buffer.BufferSize;
}



bool Mp4Parser_t::Read(ReadBytesFunc_t ReadBytes)
{
	try
	{
		DataReader_t Reader(mFilePosition,ReadBytes);
		//	get next atom
		auto Atom = Reader.ReadNextAtom();
	
		//	do specific atom decode
		std::cout << "Got atom " << Atom.Fourcc << std::endl;
		mFilePosition += Atom.AtomSize();
		return true;
	}
	catch(TNeedMoreDataException& e)
	{
		return false;
	}	
}
