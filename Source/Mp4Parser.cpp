#include "Mp4Parser.h"
#include <iostream>
#include <sstream>

namespace std
{
	static auto& Debug = std::cout;		//	debug to stout
	//std::stringstream Debug;	//	absorb debug
}

//	https://stackoverflow.com/a/61146679/355753
template <typename T>
void swapEndian(T& buffer)
{
	static_assert(std::is_pod<T>::value, "swapEndian support POD type only");
	char* startIndex = reinterpret_cast<char*>(&buffer);
	char* endIndex = startIndex + sizeof(buffer);
	std::reverse(startIndex, endIndex);
}




Atom_t DataReader_t::ReadNextAtom()
{
	Atom_t Atom;
	Atom.FilePosition = mFilePosition + mExternalFilePosition;
	Atom.Size = Read32();
	//Atom.Fourcc = ReadString(4);
	Atom.Fourcc = Read32Reversed();
	
	//	size of 1 means 64 bit size
	if ( Atom.Size == 1 )
	{
		Atom.Size64 = Read64();
	}
	
	if ( Atom.AtomSize() < 8 )
		throw std::runtime_error("Atom reported size as less than 8 bytes; not possible.");
	
	//	dont extract data, but we do want to step over it	
	//Atom.Data = ReadBytes( Atom.ContentSize() );
	mFilePosition += Atom.ContentSize();
	
	return Atom;
}

template<typename TYPE>
std::span<uint8_t> GetByteSpan(TYPE& Element)
{
	auto* Ptr = reinterpret_cast<uint8_t*>( &Element );
	return std::span( Ptr, sizeof(TYPE) );
}

uint8_t DataReader_t::Read8()
{
	uint8_t Value = 0;
	std::span Data( &Value, 1 );
	Read( Data );
	return Value;
}

uint16_t DataReader_t::Read16()
{
	uint16_t Value = 0;
	auto Data = GetByteSpan(Value);
	Read( Data );
	swapEndian(Value);
	return Value;
}

uint32_t DataReader_t::Read24()
{
	uint8_t abc[3];
	std::span Data( abc, std::size(abc) );
	Read( Data );
	uint32_t abc32 = 0;
	abc32 |= abc[0] << 16;
	abc32 |= abc[1] << 8;
	abc32 |= abc[2] << 0;
	return abc32;
}

uint32_t DataReader_t::Read32()
{
	uint32_t Value = 0;
	auto Data = GetByteSpan( Value );
	Read( Data );
	swapEndian(Value);
	return Value;
}

uint32_t DataReader_t::Read32Reversed()
{
	uint8_t abc[4];
	std::span Data( abc, std::size(abc) );
	Read( Data );
	uint32_t abc32 = 0;
	abc32 |= abc[0] << 24;
	abc32 |= abc[1] << 16;
	abc32 |= abc[2] << 8;
	abc32 |= abc[3] << 0;
	return abc32;
}

uint64_t DataReader_t::Read64()
{
	uint64_t Value = 0;
	auto Data = GetByteSpan( Value );
	Read( Data );
	swapEndian(Value);
	return Value;
}

std::vector<uint8_t> DataReader_t::ReadBytes(size_t Size)
{
	std::vector<uint8_t> Buffer;
	Buffer.resize(Size);
	std::span Data( Buffer );
	Read( Data );
	return Buffer;
}

std::string DataReader_t::ReadString(int Length)
{
	auto Bytes = ReadBytes(Length);
	char* Chars = reinterpret_cast<char*>(Bytes.data());
	return std::string( Chars, Length );
}

void DataReader_t::Read(std::span<uint8_t> Buffer)
{
	auto ReadPos = mFilePosition+mExternalFilePosition;
	if ( !ReadFileBytes( Buffer, ReadPos ) )
		throw TNeedMoreDataException();
	
	//	move file pos along
	mFilePosition += Buffer.size();
}

std::vector<Atom_t*> Atom_t::GetChildAtoms(uint32_t MatchFourcc)
{
	std::vector<Atom_t*> Children;
	for( int i=0;	i<mChildAtoms.size();	i++ )
	{
		auto& Child = mChildAtoms[i];
		if ( Child.Fourcc != MatchFourcc )
			continue;
		Children.push_back(&Child);
	}
	return Children;
}


Atom_t* Atom_t::GetChildAtom(uint32_t MatchFourcc)
{
	Atom_t* Match = nullptr;
	for( int i=0;	i<mChildAtoms.size();	i++ )
	{
		auto& Child = mChildAtoms[i];
		if ( Child.Fourcc != MatchFourcc )
			continue;
			
		if ( Match )
		{
			std::stringstream Error;
			Error << "Atom " << Fourcc << " found multiple child matches for " << MatchFourcc;
			throw std::runtime_error(Error.str());
		}
		Match = &Child;
	}
	return Match;
}

Atom_t& Atom_t::GetChildAtomRef(uint32_t MatchFourcc)
{
	Atom_t* Match = GetChildAtom(MatchFourcc);
	if ( !Match )
	{
		std::stringstream Error;
		Error << "Atom " << Fourcc << " missing expected child " << MatchFourcc;
		throw std::runtime_error(Error.str());
	}
	return *Match;
}

std::vector<uint8_t> Atom_t::GetContents(ReadBytesFunc_t ReadBytes)
{
	std::vector<uint8_t> Buffer;
	Buffer.resize(ContentSize());
	std::span Data( Buffer );

	auto ReadPos = ContentsFilePosition();
	if ( !ReadBytes( Data, ReadPos ) )
		throw TNeedMoreDataException();
	return Buffer;
}



BufferReader_t::BufferReader_t(uint64_t ExternalFilePosition,std::vector<uint8_t> Contents) :
	DataReader_t	( ExternalFilePosition ),
	mContents		( Contents )
{
}

bool BufferReader_t::ReadFileBytes(std::span<uint8_t> Buffer,size_t FilePosition)
{
	auto ContentsPosition = FilePosition - mExternalFilePosition;
	if ( ContentsPosition < 0 || ContentsPosition >= mContents.size() )
	{
		std::stringstream Error;
		Error << "Requested x" << Buffer.size() << " from File position (" << ContentsPosition << "=" << FilePosition << "-" << mExternalFilePosition << " out of contents range (" << mContents.size() << ")";
		throw std::runtime_error(Error.str());
	}
	
	for ( int i=0;	i<Buffer.size();	i++ )
	{
		Buffer[i] = mContents[ContentsPosition+i];
	}
	return true;
}



bool ExternalReader_t::ReadFileBytes(std::span<uint8_t> Buffer, size_t FilePosition)
{
	return mReadBytes(Buffer, FilePosition);
}

void Atom_t::DecodeChildAtoms(ReadBytesFunc_t ReadBytes)
{
	auto Reader = GetContentsReader(ReadBytes);
	//auto Contents = GetContents(ReadBytes);
	//auto ContentsFilePosition = this->ContentsFilePosition();
	//BufferReader_t Reader(ContentsFilePosition, Contents);


	while ( Reader.BytesRemaining() )
	{
		auto Atom = Reader.ReadNextAtom();
		mChildAtoms.push_back(Atom);
	}
}


BufferReader_t Atom_t::GetContentsReader(ReadBytesFunc_t ReadBytes)
{
	auto Contents = GetContents( ReadBytes );
	auto ContentsFilePosition = this->ContentsFilePosition();
	BufferReader_t Reader( ContentsFilePosition, Contents );
	return Reader;
}

bool Mp4::Parser_t::Read(ReadBytesFunc_t ReadBytes,std::function<void(Codec_t&)> EnumNewCodec,std::function<void(const Sample_t&)> EnumNewSample)
{
	try
	{
		ExternalReader_t Reader(mFilePosition,ReadBytes);
		//	get next atom
		auto Atom = Reader.ReadNextAtom();
	
		//	do specific atom decode
		if ( Atom.Fourcc == 'moov' )
		{
			auto OnFoundSamples = [this](std::vector<Sample_t>& Samples)
			{
				this->OnSamples(Samples);
			};
			auto OnFoundCodec = [this](std::shared_ptr<Codec_t> Codec)
			{
				this->OnCodecHeader(Codec);
			};
			DecodeAtom_Moov(Atom,ReadBytes,OnFoundSamples,OnFoundCodec);
		}
		
		std::Debug << "Got atom " << Atom.Fourcc << std::endl;
		mFilePosition += Atom.AtomSize();
		
		//	flush new codec as samples
		for ( int s=0;	s<mNewCodecs.size();	s++ )
		{
			auto& Codec = mNewCodecs[s];
			EnumNewCodec(*Codec);
		}
		mNewCodecs.resize(0);
		
		//	flush new samples
		for ( int s=0;	s<mNewSamples.size();	s++ )
		{
			auto& Sample = mNewSamples[s];
			EnumNewSample(Sample);
		}
		mNewSamples.resize(0);
		
		return true;
	}
	catch(TNeedMoreDataException&)
	{
		return false;
	}	
}

void Mp4::DecodeAtom_Moov(Atom_t& Atom,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&)> OnSamples,std::function<void(std::shared_ptr<Codec_t> Codec)> OnCodec)
{
	Atom.DecodeChildAtoms( ReadBytes );
	
	//	decode movie header mvhd as it has timing meta
	MovieHeader_t MovieHeader;
	auto MovieHeaderAtom = Atom.GetChildAtom('mvhd');
	if ( MovieHeaderAtom )
		MovieHeader = DecodeAtom_MovieHeader( *MovieHeaderAtom, ReadBytes);
		
	//	decode tracks
	auto Traks = Atom.GetChildAtoms('trak');
	for ( int t=0;	t<Traks.size();	t++ )
	{
		auto& Trak = *Traks[t];
		//	tracks in mp4s start at 1...
		//	also, work out if there's a proper place for track/stream numbers 
		auto TrackNumber = t;	
		DecodeAtom_Trak(Trak,TrackNumber,MovieHeader,ReadBytes,OnSamples,OnCodec);
	}
}

uint64_t GetDateTimeFromSecondsSinceMidnightJan1st1904(uint64_t Timestamp)
{
	return 1234;	//	todo!
}

MovieHeader_t Mp4::DecodeAtom_MovieHeader(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	auto Reader = Atom.GetContentsReader(ReadBytes);
	
	//	https://developer.apple.com/library/content/documentation/QuickTime/QTFF/art/qt_l_095.gif
	auto Version = Reader.Read8();
	auto Flags = Reader.Read24();
		
	//	hololens had what looked like 64 bit timestamps...
	//	this is my working reference :)
	//	https://github.com/macmade/MP4Parse/blob/master/source/MP4.MVHD.cpp#L50
	uint64_t CreationTime=0,ModificationTime=0,Duration=0;	//	long
	uint64_t TimeScale = 0;
	if ( Version == 0)
	{
		CreationTime = Reader.Read32();
		ModificationTime = Reader.Read32();
		TimeScale = Reader.Read32();
		Duration = Reader.Read32();
	}
	else if(Version == 1)
	{
		CreationTime = Reader.Read64();
		ModificationTime = Reader.Read64();
		TimeScale = Reader.Read32();
		Duration = Reader.Read64();
	}
	else
	{
		std::stringstream Error;
		Error << "Expected Version 0 or 1 for MVHD (Version=" << Version << ". If neccessary can probably continue without timing info!";
		throw std::runtime_error(Error.str());
	}
		
	auto PreferredRate = Reader.Read32();
	//auto PreferredVolume = Reader.Read16();	//	8.8 fixed point volume
	auto PreferredVolumeHi = Reader.Read8();
	auto PreferredVolumeLo = Reader.Read8();
	auto Reserved = Reader.ReadBytes(10);

	auto Matrix_a = Reader.Read32();
	auto Matrix_b = Reader.Read32();
	auto Matrix_u = Reader.Read32();
	auto Matrix_c = Reader.Read32();
	auto Matrix_d = Reader.Read32();
	auto Matrix_v = Reader.Read32();
	auto Matrix_x = Reader.Read32();
	auto Matrix_y = Reader.Read32();
	auto Matrix_w = Reader.Read32();

	auto PreviewTime = Reader.Read32();
	auto PreviewDuration = Reader.Read32();
	auto PosterTime = Reader.Read32();
	auto SelectionTime = Reader.Read32();
	auto SelectionDuration = Reader.Read32();
	auto CurrentTime = Reader.Read32();
	auto NextTrackId = Reader.Read32();

	for ( int i=0;	i<Reserved.size();	i++ )
	{
		auto Zero = Reserved[i];
		if (Zero != 0)
			std::Debug << "Reserved value " << Zero << " is not zero" << std::endl;
	}

	//	actually a 3x3 matrix, but we make it 4x4 for unity
	//	gr: do we need to transpose this? docs don't say row or column major :/
	//	wierd element labels, right? spec uses them.
/*
		//	gr: matrixes arent simple
		//		https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFChap4/qtff4.html#//apple_ref/doc/uid/TP40000939-CH206-18737
		//	All values in the matrix are 32 - bit fixed-point numbers divided as 16.16, except for the { u, v, w}
		//	column, which contains 32 - bit fixed-point numbers divided as 2.30.Figure 5 - 1 and Figure 5 - 2 depict how QuickTime uses matrices to transform displayed objects.
		var a = Fixed1616ToFloat(Matrix_a);
		var b = Fixed1616ToFloat(Matrix_b);
		var u = Fixed230ToFloat(Matrix_u);
		var c = Fixed1616ToFloat(Matrix_c);
		var d = Fixed1616ToFloat(Matrix_d);
		var v = Fixed230ToFloat(Matrix_v);
		var x = Fixed1616ToFloat(Matrix_x);
		var y = Fixed1616ToFloat(Matrix_y);
		var w = Fixed230ToFloat(Matrix_w);
		var MtxRow0 = new Vector4(a, b, u, 0);
		var MtxRow1 = new Vector4(c, d, v, 0);
		var MtxRow2 = new Vector4(x, y, w, 0);
		var MtxRow3 = new Vector4(0, 0, 0, 1);
*/
	MovieHeader_t Header;
	//var Header = new TMovieHeader();
	Header.TimeScaleUnitsPerSecond = TimeScale; //	timescale is time units per second
	//Header.VideoTransform = new Matrix4x4(MtxRow0, MtxRow1, MtxRow2, MtxRow3);

	//	gr: this is supposed to be seconds
	//	scale in CR sample is 1000(1 sec)
	//	so we convert back to ms
	Header.DurationMs = Header.TimeUnitsToMs( Duration );

	Header.CreationTimeMs = GetDateTimeFromSecondsSinceMidnightJan1st1904(CreationTime);
	Header.ModificationTimeMs = GetDateTimeFromSecondsSinceMidnightJan1st1904(ModificationTime);
	Header.CreationTimeMs = GetDateTimeFromSecondsSinceMidnightJan1st1904(CreationTime);
	Header.PreviewDurationMs = Header.TimeUnitsToMs( PreviewDuration );
	return Header;
}

void Mp4::DecodeAtom_Trak(Atom_t& Atom,int TrackNumber,MovieHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&)> OnSamples,std::function<void(std::shared_ptr<Codec_t> Codec)> OnCodec)
{
	Atom.DecodeChildAtoms( ReadBytes );

	auto Mdias = Atom.GetChildAtoms('mdia');
	for ( int t=0;	t<Mdias.size();	t++ )
	{
		auto& Mdia = *Mdias[t];
		DecodeAtom_Media(Mdia,TrackNumber,MovieHeader,ReadBytes,OnSamples,OnCodec);
	}
}

void Mp4::DecodeAtom_Media(Atom_t& Atom,int TrackNumber,MovieHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&)> OnSamples,std::function<void(std::shared_ptr<Codec_t> Codec)> OnCodec)
{
	Atom.DecodeChildAtoms( ReadBytes );
	
	MediaHeader_t MediaHeader( MovieHeader );
	auto Mdhd = Atom.GetChildAtom('mdhd');
	if ( Mdhd )
	{
		MediaHeader = DecodeAtom_MediaHeader( *Mdhd, MovieHeader, ReadBytes );
	}
	MediaHeader.TrackNumber = TrackNumber;
	
	DecodeAtom_MediaInfo( Atom.GetChildAtomRef('minf'), MediaHeader, ReadBytes, OnSamples, OnCodec );
}

MediaHeader_t Mp4::DecodeAtom_MediaHeader(Atom_t& Atom,MovieHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes)
{
	auto Reader = Atom.GetContentsReader(ReadBytes);
	
	auto Version = Reader.Read8();
	auto Flags = Reader.Read24();
	auto CreationTime = Reader.Read32();
	auto ModificationTime = Reader.Read32();
	auto TimeScale = Reader.Read32();
	auto Duration = Reader.Read32();
	auto Language = Reader.Read16();
	auto Quality = Reader.Read16();

	MediaHeader_t Header(MovieHeader);
	Header.TimeScaleUnitsPerSecond = TimeScale;
	//Header.Duration = new TimeSpan(0,0, (int)(Duration * Header.TimeScale));
	Header.CreationTimeMs = GetDateTimeFromSecondsSinceMidnightJan1st1904(CreationTime);
	Header.ModificationTimeMs = GetDateTimeFromSecondsSinceMidnightJan1st1904(ModificationTime);
	Header.LanguageId = Language;
	Header.Quality = Quality / static_cast<float>(1 << 16);
	Header.DurationMs = Header.TimeUnitsToMs( Duration );
	return Header;
}

void Mp4::DecodeAtom_MediaInfo(Atom_t& Atom,MediaHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes,std::function<void(std::vector<Sample_t>&)> OnSamples,std::function<void(std::shared_ptr<Codec_t> Codec)> OnCodec)
{
	Atom.DecodeChildAtoms( ReadBytes );
	//	js is
	//	samples = DecodeAtom_SampleTable
	auto Samples = DecodeAtom_SampleTable( Atom.GetChildAtomRef('stbl'), MovieHeader, ReadBytes, OnCodec );
	
	OnSamples(Samples);
}

void Mp4::Parser_t::OnCodecHeader(std::shared_ptr<Codec_t> Codec)
{
	mNewCodecs.push_back(Codec);
}

void Mp4::Parser_t::OnSamples(std::vector<Sample_t>& NewSamples)
{
	mNewSamples.insert( mNewSamples.end(), std::begin(NewSamples), std::end(NewSamples) );
}


std::vector<ChunkMeta_t> Mp4::DecodeAtom_ChunkMetas(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	std::vector<ChunkMeta_t> Metas;
	
	auto Reader = Atom.GetContentsReader(ReadBytes);
	auto Flags = Reader.Read24();
	auto Version = Reader.Read8();
	auto EntryCount = Reader.Read32();
	
	auto MetaSize = 3 * 4;	//	3x32 bit
	/*
	const Offset = Reader.FilePosition;
	for ( let i=Offset;	i<Atom.Data.length;	i+=MetaSize )
	{
		const ChunkData = Atom.Data.slice( i, i+MetaSize );
		const Meta = new ChunkMeta_t(ChunkData);
		Metas.Add(Meta);
	}
	*/
	for ( int e=0;	e<EntryCount;	e++ )
	{
		ChunkMeta_t ChunkMeta;
		ChunkMeta.FirstChunk = Reader.Read32();
		ChunkMeta.SamplesPerChunk = Reader.Read32();
		ChunkMeta.SampleDescriptionId = Reader.Read32();
		Metas.push_back(ChunkMeta);
	};
	if (Metas.size() != EntryCount)
		throw std::runtime_error("Chunk metas didn't match expected count");//`Expected ${EntryCount} chunk metas, got ${Metas.length}`;
	return Metas;
}

std::vector<uint64_t> Mp4::DecodeAtom_ChunkOffsets(Atom_t* ChunkOffsets32Atom,Atom_t* ChunkOffsets64Atom,ReadBytesFunc_t ReadBytes)
{
	Atom_t* pAtom = nullptr;
	int OffsetSize = 0;
	if ( ChunkOffsets32Atom )
	{
		OffsetSize = 32 / 8;
		pAtom = ChunkOffsets32Atom;
	}
	else if ( ChunkOffsets64Atom )
	{
		OffsetSize = 64 / 8;
		pAtom = ChunkOffsets64Atom;
	}
	else
	{
		throw std::runtime_error("Missing offset atom");
	}
	
	auto& Atom = *pAtom;
	auto Reader = Atom.GetContentsReader(ReadBytes);
	auto Version = Reader.Read8();
	auto Flags = Reader.Read24();
	//var Version = AtomData[8];
	//var Flags = Get24(AtomData[9], AtomData[10], AtomData[11]);
	auto EntryCount = Reader.Read32();
		
	std::vector<uint64_t> Offsets;
	for ( int e=0;	e<EntryCount;	e++ )
	{
		uint64_t Offset = 0;
		if ( OffsetSize == 32/8 )
			Offset = Reader.Read32();
		if ( OffsetSize == 64/8 )
			Offset = Reader.Read64();
		Offsets.push_back(Offset);
	}

	return Offsets;
}

std::vector<uint64_t> Mp4::DecodeAtom_SampleSizes(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	auto Reader = Atom.GetContentsReader(ReadBytes);
	auto Version = Reader.Read8();
	auto Flags = Reader.Read24();
	auto SampleSize = Reader.Read32();
	auto EntryCount = Reader.Read32();
		
	std::vector<uint64_t> Sizes;
		
	//	if size specified, they're all this size
	if (SampleSize != 0)
	{
		for ( int i=0;	i<EntryCount;	i++)
			Sizes.push_back(SampleSize);
		return Sizes;
	}
		
	//	each entry in the table is the size of a sample (and one chunk can have many samples)
	//const SampleSizeStart = 20;	//	Reader.FilePosition
	auto SampleSizeStart = Reader.mFilePosition;
	if ( Reader.mFilePosition != SampleSizeStart )
		throw std::runtime_error("Offset calculation has gone wrong");
			
	//	gr: docs don't say size, but this seems accurate...
	//		but also sometimes doesnt SEEM to match the size in the header?
	SampleSize = (Atom.ContentSize() - SampleSizeStart) / EntryCount;
	//for ( let i = SampleSizeStart; i < AtomData.Length; i += SampleSize)
	for ( int e=0;	e<EntryCount;	e++ )
	{
		if (SampleSize == 3)
		{
			auto Size = Reader.Read24();
			//var Size = Get24(AtomData[i + 0], AtomData[i + 1], AtomData[i + 2]);
			Sizes.push_back(Size);
		}
		else if (SampleSize == 4)
		{
			auto Size = Reader.Read32();
			//var Size = Get32(AtomData[i + 0], AtomData[i + 1], AtomData[i + 2], AtomData[i + 3]);
			Sizes.push_back(Size);
		}
		else
			throw std::runtime_error(std::string("Unhandled sample size ")+std::to_string(SampleSize));
	}
	
	return Sizes;
}

std::vector<bool> Mp4::DecodeAtom_SampleKeyframes(Atom_t* pAtom,int SampleCount,ReadBytesFunc_t ReadBytes)
{
	//	keyframe index map
	std::vector<bool> Keyframes;
	//	init array
	{
		auto Default = pAtom ? false : true;
		for ( int i=0;	i<SampleCount;	i++ )
			Keyframes.push_back(Default);
	}
	if ( !pAtom )
		return Keyframes;
	
	auto& Atom = *pAtom;
	auto Reader = Atom.GetContentsReader(ReadBytes);
	auto Version = Reader.Read8();
	auto Flags = Reader.Read24();
	auto EntryCount = Reader.Read32();
	
	if ( EntryCount == 0 )
		return Keyframes;
		
	//	gr: docs don't say size, but this seems accurate...
	auto IndexSize = (Atom.ContentSize() - Reader.mFilePosition) / EntryCount;
	for ( int e=0;	e<EntryCount;	e++ )
	{
		int SampleIndex;
		if ( IndexSize == 3 )
		{
			SampleIndex = Reader.Read24();
		}
		else if ( IndexSize == 4 )
		{
			SampleIndex = Reader.Read32();
		}
		else
			throw std::runtime_error( std::string("Unhandled index size ") + std::to_string(IndexSize) );
		
		//	gr: indexes start at 1
		SampleIndex--;
		Keyframes[SampleIndex] = true;
	}
	return Keyframes;
}


std::vector<uint64_t> Mp4::DecodeAtom_SampleDurations(Atom_t* pAtom,int SampleCount,int Default,ReadBytesFunc_t ReadBytes)
{
	std::vector<uint64_t> Durations;
	if ( !pAtom )
	{
		if ( Default == -1 )
			throw std::runtime_error("No atom and no default to get sample durations from");
		
		for ( int e=0;	e<SampleCount;	e++ )
			Durations.push_back(Default);
		return Durations;
	}
	
	auto& Atom = *pAtom;
	auto Reader = Atom.GetContentsReader(ReadBytes);
	auto Version = Reader.Read8();
	auto Flags = Reader.Read24();
	auto EntryCount = Reader.Read32();
	
	while ( Reader.BytesRemaining() )
	{
		auto SubSampleCount = Reader.Read32();
		auto SubSampleDuration = Reader.Read32();
		
		for ( int s=0;	s<SubSampleCount;	s++ )
			Durations.push_back(SubSampleDuration);
	}
	
	if ( Durations.size() != EntryCount )
	{
		//	gr: for some reason, EntryCount is often 1, but there are more samples
		//	throw `Durations extracted doesn't match entry count`
	}
	
	if ( Durations.size() != SampleCount )
	{
		std::stringstream Error;
		Error << "Durations Extracted(" << Durations.size() << ") don't match sample count(" << SampleCount << ") EntryCount=" << EntryCount;
		throw std::runtime_error(Error.str());
	}
		
	return Durations;

}

std::shared_ptr<Codec_t> Mp4::DecodeAtom_SampleDescription(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	auto Reader = Atom.GetContentsReader(ReadBytes);
	
	//	 https://www.cimarronsystems.com/wp-content/uploads/2017/04/Elements-of-the-H.264-VideoAAC-Audio-MP4-Movie-v2_0.pdf

	auto Version = Reader.Read8();
	auto Flags = Reader.Read24();
	auto EntryCount = Reader.Read32();

	//	each entry is essentially an atom
	//	https://github.com/NewChromantics/PopCodecs/blob/master/PopMpeg4.cs#L1022
	//	should match EntryCount
	while ( Reader.BytesRemaining() )
	{
		auto ChildAtom = Reader.ReadNextAtom();
		Atom.mChildAtoms.push_back(ChildAtom);
	}

	
	//	h264 has
	//	avc1
	//		avcC <-- sps etc
	//		pasp <-- pixel aspect
	auto* H264Atom = Atom.GetChildAtom('avc1');
	if ( H264Atom )
		return DecodeAtom_Avc1(*H264Atom,ReadBytes);
	
	//	should only be one child I think
	std::stringstream Error;
	Error << "Don't know how to parse sample codec[s]; ";
	for ( int c=0;	c<Atom.mChildAtoms.size();	c++ )
		Error << Atom.mChildAtoms[c].Fourcc << ", ";
	throw std::runtime_error(Error.str());
}

CodecAvc1_t::CodecAvc1_t(DataReader_t& DataReader)
{
	auto Version = DataReader.Read8();
	mProfile = DataReader.Read8();
	mCompatbility = DataReader.Read8();
	mLevel = DataReader.Read8();
	
	auto ReservedAndSizeLength = DataReader.Read8();	//	6bits==0 3==lengthminusone
	auto Reserved1 =  ReservedAndSizeLength & 0b11111100;
	mLengthMinusOne = ReservedAndSizeLength & 0b00000011;

	auto ReservedAndSpsCount = DataReader.Read8();
	auto SpsCount =  ReservedAndSpsCount & 0b00011111;
	auto Reserved2 = ReservedAndSpsCount & 0b11100000;
	
	for ( int i=0;	i<SpsCount;	i++ )
	{
		auto SpsLength = DataReader.Read16();
		Sps_t Sps;
		Sps.Data = DataReader.ReadBytes(SpsLength);
		mSps.push_back(Sps);
	}
	
	auto PpsCount = DataReader.Read8();
	for ( int i=0;	i<PpsCount;	i++ )
	{
		auto PpsLength = DataReader.Read16();
		Pps_t Pps;
		Pps.Data = DataReader.ReadBytes(PpsLength);
		mPps.push_back(Pps);
	}
}

std::shared_ptr<Codec_t> Mp4::DecodeAtom_Avc1(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	auto Reader = Atom.GetContentsReader(ReadBytes);
	//	https://github.com/NewChromantics/PopCodecs/blob/master/PopMpeg4.cs#L1024
	//	stsd isn't very well documented
	//	https://www.cimarronsystems.com/wp-content/uploads/2017/04/Elements-of-the-H.264-VideoAAC-Audio-MP4-Movie-v2_0.pdf
	//auto SampleDescriptionSize = Reader.Read32();
	//auto CodecFourcc = Reader.ReadString(4);
	auto Reserved = Reader.ReadBytes(6);
	auto DataReferenceIndex = Reader.Read16();

	auto Predefines = Reader.ReadBytes(16);
	auto MediaWidth = Reader.Read16();
	auto MediaHeight = Reader.Read16();
	auto HorzResolution = Reader.Read16();
	auto HorzResolutionLow = Reader.Read16();
	auto VertResolution = Reader.Read16();
	auto VertResolutionLow = Reader.Read16();
	auto Reserved1 = Reader.Read8();
	auto FrameCount = Reader.Read8();
	auto ColourDepth = Reader.Read8();

	auto MoreData = Reader.ReadBytes(39);

	//	remaining data is in atoms again, expecting typically
	//	avcC (with actual codec data) and pasp (picture aspect)
	while ( Reader.BytesRemaining() )
	{
		auto ChildAtom = Reader.ReadNextAtom();
		Atom.mChildAtoms.push_back(ChildAtom);
	}

	//	explicitly decode avcc codec data (sps, pps in avcc format)
	auto& Avc1Atom = Atom.GetChildAtomRef('avcC');
	auto AvccData = Avc1Atom.GetContentsReader(ReadBytes);
	std::shared_ptr<CodecAvc1_t> pAvc1( new CodecAvc1_t(AvccData) );
	pAvc1->mFourcc = Avc1Atom.Fourcc;
	return pAvc1;
}
	
std::vector<Sample_t> Mp4::DecodeAtom_SampleTable(Atom_t& Atom,MediaHeader_t& MovieHeader,ReadBytesFunc_t ReadBytes,std::function<void(std::shared_ptr<Codec_t> Codec)> OnCodec)
{
	Atom.DecodeChildAtoms( ReadBytes );
	
	//	get avc1 etc codec
	{
		auto SampleDescriptionAtom = Atom.GetChildAtomRef('stsd');
		MovieHeader.Codec = DecodeAtom_SampleDescription(SampleDescriptionAtom,ReadBytes);
		
		//	should error if we have no codec?
		if ( MovieHeader.Codec )
		{
			MovieHeader.Codec->mTrackNumber = MovieHeader.TrackNumber;
			OnCodec( MovieHeader.Codec );
		}
	}
	
	auto ChunkOffsets32Atom = Atom.GetChildAtom('stco');
	auto ChunkOffsets64Atom = Atom.GetChildAtom('co64');
	auto SampleSizesAtom = Atom.GetChildAtom('stsz');
	auto SampleToChunkAtom = Atom.GetChildAtom('stsc');
	auto SyncSamplesAtom = Atom.GetChildAtom('stss');
	auto SampleDecodeDurationsAtom = Atom.GetChildAtom('stts');
	auto SamplePresentationTimeOffsetsAtom = Atom.GetChildAtom('ctts');
	
	//	work out samples from atoms!
	if (SampleSizesAtom == nullptr)
		throw std::runtime_error("Track missing sample sizes atom");
	if (ChunkOffsets32Atom == nullptr && ChunkOffsets64Atom == nullptr)
		throw std::runtime_error("Track missing chunk offset atom");
	if (SampleToChunkAtom == nullptr)
		throw std::runtime_error("Track missing sample-to-chunk table atom");
	if (SampleDecodeDurationsAtom == nullptr)
		throw std::runtime_error("Track missing time-to-sample table atom");
	
	auto PackedChunkMetas = DecodeAtom_ChunkMetas(*SampleToChunkAtom,ReadBytes);
	auto ChunkOffsets = DecodeAtom_ChunkOffsets( ChunkOffsets32Atom, ChunkOffsets64Atom, ReadBytes );
	auto SampleSizes = DecodeAtom_SampleSizes( *SampleSizesAtom, ReadBytes );
	auto SampleKeyframes = DecodeAtom_SampleKeyframes(SyncSamplesAtom, SampleSizes.size(), ReadBytes );
	auto SampleDurations = DecodeAtom_SampleDurations( SampleDecodeDurationsAtom, SampleSizes.size(), -1, ReadBytes );
	auto SamplePresentationTimeOffsets = DecodeAtom_SampleDurations(SamplePresentationTimeOffsetsAtom, SampleSizes.size(), 0, ReadBytes );
	
	//	durations start at zero (proper time must come from somewhere else!) and just count up over durations
	std::vector<uint64_t> SampleDecodeTimes;//new int[SampleSizes.Count];
	for ( int i=0;	i<SampleSizes.size();	i++ )
	{
		auto LastDuration = (i == 0) ? 0 : SampleDurations[i - 1];
		auto LastTime = (i == 0) ? 0 : SampleDecodeTimes[i - 1];
		auto DecodeTime = LastTime + LastDuration;
		SampleDecodeTimes.push_back( DecodeTime );
	}

	//	pad (fill in gaps) the metas to fit offset information
	//	https://sites.google.com/site/james2013notes/home/mp4-file-format
	std::vector<ChunkMeta_t> ChunkMetas;
	for ( int i=0;	i<PackedChunkMetas.size();	i++ )
	{
		auto ChunkMeta = PackedChunkMetas[i];
		//	first begins at 1. despite being an index...
		auto FirstChunk = ChunkMeta.FirstChunk - 1;
		//	pad previous up to here
		while ( ChunkMetas.size() < FirstChunk )
			ChunkMetas.push_back(ChunkMetas[ChunkMetas.size() - 1]);

		ChunkMetas.push_back(ChunkMeta);
	}
	//	and pad the end
	while (ChunkMetas.size() < ChunkOffsets.size())
		ChunkMetas.push_back(ChunkMetas[ChunkMetas.size() - 1]);

	/*
	//	we're now expecting this to be here
	var MdatStartPosition = MdatAtom.HasValue ? MdatAtom.Value.AtomDataFilePosition : (long?)null;
*/
	uint64_t* MdatStartPosition = nullptr;
	/*
		//	superfolous data
		var Chunks = new List<TSample>();
		long? MdatEnd = (MdatAtom.HasValue) ? (MdatAtom.Value.DataSize) : (long?)null;
		for (int i = 0; i < ChunkOffsets.Count; i++)
		{
			var ThisChunkOffset = ChunkOffsets[i];
			//	chunks are serial, so length is up to next
			//	gr: mdatend might need to be +1
			long? NextChunkOffset = (i >= ChunkOffsets.Count - 1) ? MdatEnd : ChunkOffsets[i + 1];
			long ChunkLength = (NextChunkOffset.HasValue) ? (NextChunkOffset.Value - ThisChunkOffset) : 0;

			var Chunk = new TSample();
			Chunk.DataPosition = ThisChunkOffset;
			Chunk.DataSize = ChunkLength;
			Chunks.Add(Chunk);
		}
		*/
	std::vector<Sample_t> Samples;
	auto SampleDataPrefixSize = MovieHeader.Codec ? MovieHeader.Codec->GetSampleDataPrefixSize() : 0;

	int SampleIndex = 0;
	for ( int i=0;	i<ChunkMetas.size();	i++)
	{
		auto SampleMeta = ChunkMetas[i];
		auto ChunkIndex = i;
		auto ChunkFileOffset = ChunkOffsets[ChunkIndex];

		for ( int s=0;	s<SampleMeta.SamplesPerChunk;	s++ )
		{
			Sample_t Sample;
			Sample.DataPrefixSize = SampleDataPrefixSize;

			if ( MdatStartPosition != nullptr )
				Sample.MdatPosition = ChunkFileOffset - (*MdatStartPosition);
			else
				Sample.DataFilePosition = ChunkFileOffset;

			Sample.DataSize = SampleSizes[SampleIndex];
			Sample.IsKeyframe = SampleKeyframes[SampleIndex];
			Sample.DecodeTimeMs = MovieHeader.TimeUnitsToMs( SampleDecodeTimes[SampleIndex] );
			Sample.DurationMs = MovieHeader.TimeUnitsToMs( SampleDurations[SampleIndex] );
			Sample.PresentationTimeMs = MovieHeader.TimeUnitsToMs( SampleDecodeTimes[SampleIndex] + SamplePresentationTimeOffsets[SampleIndex] );
			Samples.push_back(Sample);

			ChunkFileOffset += Sample.DataSize;
			SampleIndex++;
		}
	}

	if (SampleIndex != SampleSizes.size())
	{
		std::stringstream Error;
		Error << "Enumerated " << SampleIndex << " samples, expected " << SampleSizes.size();
		throw std::runtime_error(Error.str());
	}

	return Samples;
}


