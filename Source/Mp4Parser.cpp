#include "Mp4Parser.h"
#include <iostream>
#include <sstream>

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
	Atom.Fourcc = ReadString(4);
	
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

uint8_t DataReader_t::Read8()
{
	uint8_t Value = 0;
	DataSpan_t Data( Value );
	Read( Data );
	return Value;
}

uint32_t DataReader_t::Read24()
{
	uint8_t abc[3];
	DataSpan_t Data( abc );
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

std::vector<Atom_t*> Atom_t::GetChildAtoms(const std::string& MatchFourcc)
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


Atom_t* Atom_t::GetChildAtom(const std::string& MatchFourcc)
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

Atom_t& Atom_t::GetChildAtomRef(const std::string& MatchFourcc)
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
	DataSpan_t Data;
	Data.Buffer = Buffer.data();
	Data.BufferSize = Buffer.size();
	auto ReadPos = ContentsFilePosition();
	if ( !ReadBytes( Data, ReadPos ) )
		throw TNeedMoreDataException();
	return Buffer;
}



BufferReader_t::BufferReader_t(uint64_t ExternalFilePosition,std::vector<uint8_t> Contents) :
	DataReader_t	( ExternalFilePosition, std::bind(&BufferReader_t::ReadBytes, this, std::placeholders::_1, std::placeholders::_2) ),
	mContents		( Contents )
{
}

bool BufferReader_t::ReadBytes(DataSpan_t& Buffer,size_t FilePosition)
{
	auto ContentsPosition = FilePosition - mExternalFilePosition;
	if ( ContentsPosition < 0 )
		throw std::runtime_error("File position out of contents range");
	if ( ContentsPosition >= mContents.size() )
		throw std::runtime_error("File position out of contents range");
	for ( int i=0;	i<Buffer.BufferSize;	i++ )
	{
		Buffer.Buffer[i] = mContents[ContentsPosition+i];
	}
	return true;
}


void Atom_t::DecodeChildAtoms(ReadBytesFunc_t ReadBytes)
{
	//auto ContentsFilePosition = this->ContentsFilePosition();
	//BufferReader_t Reader( ContentsFilePosition, Contents );
	auto Reader = GetContentsReader(ReadBytes);
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

bool Mp4Parser_t::Read(ReadBytesFunc_t ReadBytes,std::function<void(const Sample_t&)> EnumNewSample)
{
	try
	{
		DataReader_t Reader(mFilePosition,ReadBytes);
		//	get next atom
		auto Atom = Reader.ReadNextAtom();
	
		//	do specific atom decode
		if ( Atom.Fourcc == "moov" )
			DecodeAtom_Moov(Atom,ReadBytes);
		
		std::cout << "Got atom " << Atom.Fourcc << std::endl;
		mFilePosition += Atom.AtomSize();
		
		//	flush new samples
		for ( int s=0;	s<mNewSamples.size();	s++ )
		{
			auto& Sample = mNewSamples[s];
			EnumNewSample(Sample);
		}
		mNewSamples.resize(0);
		
		return true;
	}
	catch(TNeedMoreDataException& e)
	{
		return false;
	}	
}

void Mp4Parser_t::DecodeAtom_Moov(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	Atom.DecodeChildAtoms( ReadBytes );
	
	//	decode movie header mvhd as it has timing meta
	
	//	decode tracks
	auto Traks = Atom.GetChildAtoms("trak");
	for ( int t=0;	t<Traks.size();	t++ )
	{
		auto& Trak = *Traks[t];
		DecodeAtom_Trak(Trak,ReadBytes);
	}
}

void Mp4Parser_t::DecodeAtom_Trak(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	Atom.DecodeChildAtoms( ReadBytes );

	auto Mdias = Atom.GetChildAtoms("mdia");
	for ( int t=0;	t<Mdias.size();	t++ )
	{
		auto& Mdia = *Mdias[t];
		DecodeAtom_Media(Mdia,ReadBytes);
	}
}

void Mp4Parser_t::DecodeAtom_Media(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	Atom.DecodeChildAtoms( ReadBytes );
	DecodeAtom_MediaInfo( Atom.GetChildAtomRef("minf"), ReadBytes );
}

void Mp4Parser_t::DecodeAtom_MediaInfo(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	Atom.DecodeChildAtoms( ReadBytes );
	//	js is
	//	samples = DecodeAtom_SampleTable
	auto Samples = DecodeAtom_SampleTable( Atom.GetChildAtomRef("stbl"), ReadBytes );
	
	OnSamples(Samples);
}

void Mp4Parser_t::OnSamples(std::vector<Sample_t>& NewSamples)
{
	mNewSamples.insert( mNewSamples.end(), std::begin(NewSamples), std::end(NewSamples) );
}


std::vector<ChunkMeta_t> Mp4Parser_t::DecodeAtom_ChunkMetas(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
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

std::vector<uint64_t> Mp4Parser_t::DecodeAtom_ChunkOffsets(Atom_t* ChunkOffsets32Atom,Atom_t* ChunkOffsets64Atom,ReadBytesFunc_t ReadBytes)
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

std::vector<uint64_t> Mp4Parser_t::DecodeAtom_SampleSizes(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
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

std::vector<bool> Mp4Parser_t::DecodeAtom_SampleKeyframes(Atom_t* pAtom,int SampleCount,ReadBytesFunc_t ReadBytes)
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


std::vector<uint64_t> Mp4Parser_t::DecodeAtom_SampleDurations(Atom_t* pAtom,int SampleCount,int Default,ReadBytesFunc_t ReadBytes)
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


std::vector<Sample_t> Mp4Parser_t::DecodeAtom_SampleTable(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
{
	Atom.DecodeChildAtoms( ReadBytes );
	
	auto ChunkOffsets32Atom = Atom.GetChildAtom("stco");
	auto ChunkOffsets64Atom = Atom.GetChildAtom("co64");
	auto SampleSizesAtom = Atom.GetChildAtom("stsz");
	auto SampleToChunkAtom = Atom.GetChildAtom("stsc");
	auto SyncSamplesAtom = Atom.GetChildAtom("stss");
	auto SampleDecodeDurationsAtom = Atom.GetChildAtom("stts");
	auto SamplePresentationTimeOffsetsAtom = Atom.GetChildAtom("ctts");
	
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
	
	auto TimeScale = 1.0f;//MovieHeader ? MovieHeader.TimeScale : 1;

	auto TimeToMs = [&](float TimeUnit) -> uint64_t
	{
		//	to float
		auto Timef = TimeUnit * TimeScale;
		auto TimeMs = Timef * 1000.0;
		return floorf(TimeMs);	//	round to int
	};

	int SampleIndex = 0;
	for ( int i=0;	i<ChunkMetas.size();	i++)
	{
		auto SampleMeta = ChunkMetas[i];
		auto ChunkIndex = i;
		auto ChunkFileOffset = ChunkOffsets[ChunkIndex];

		for ( int s=0;	s<SampleMeta.SamplesPerChunk;	s++ )
		{
			Sample_t Sample;

			if ( MdatStartPosition != nullptr )
				Sample.DataPosition = ChunkFileOffset - (*MdatStartPosition);
			else
				Sample.DataFilePosition = ChunkFileOffset;

			Sample.DataSize = SampleSizes[SampleIndex];
			Sample.IsKeyframe = SampleKeyframes[SampleIndex];
			Sample.DecodeTimeMs = TimeToMs( SampleDecodeTimes[SampleIndex] );
			Sample.DurationMs = TimeToMs( SampleDurations[SampleIndex] );
			Sample.PresentationTimeMs = TimeToMs( SampleDecodeTimes[SampleIndex] + SamplePresentationTimeOffsets[SampleIndex] );
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


