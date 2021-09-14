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

bool Mp4Parser_t::Read(ReadBytesFunc_t ReadBytes)
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
	DecodeAtom_SampleTable( Atom.GetChildAtomRef("stbl"), ReadBytes );
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


void Mp4Parser_t::DecodeAtom_SampleTable(Atom_t& Atom,ReadBytesFunc_t ReadBytes)
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
	/*	const ChunkOffsets = await this.DecodeAtom_ChunkOffsets( ChunkOffsets32Atom, ChunkOffsets64Atom );
		const SampleSizes = await this.DecodeAtom_SampleSizes(SampleSizesAtom);
		const SampleKeyframes = await this.DecodeAtom_SampleKeyframes(SyncSamplesAtom, SampleSizes.length);
		const SampleDurations = await this.DecodeAtom_SampleDurations( SampleDecodeDurationsAtom, SampleSizes.length);
		const SamplePresentationTimeOffsets = await this.DecodeAtom_SampleDurations(SamplePresentationTimeOffsetsAtom, SampleSizes.length, 0 );
		
		//	durations start at zero (proper time must come from somewhere else!) and just count up over durations
		const SampleDecodeTimes = [];//new int[SampleSizes.Count];
		for ( let i=0;	i<SampleSizes.length;	i++ )
		{
			const LastDuration = (i == 0) ? 0 : SampleDurations[i - 1];
			const LastTime = (i == 0) ? 0 : SampleDecodeTimes[i - 1];
			const DecodeTime = LastTime + LastDuration;
			SampleDecodeTimes.push( DecodeTime );
		}

		//	pad (fill in gaps) the metas to fit offset information
		//	https://sites.google.com/site/james2013notes/home/mp4-file-format
		const ChunkMetas = [];
		for ( let i=0;	i<PackedChunkMetas.length;	i++ )
		{
			const ChunkMeta = PackedChunkMetas[i];
			//	first begins at 1. despite being an index...
			const FirstChunk = ChunkMeta.FirstChunk - 1;
			//	pad previous up to here
			while ( ChunkMetas.length < FirstChunk )
				ChunkMetas.push(ChunkMetas[ChunkMetas.length - 1]);

			ChunkMetas.push(ChunkMeta);
		}
		//	and pad the end
		while (ChunkMetas.length < ChunkOffsets.length)
			ChunkMetas.push(ChunkMetas[ChunkMetas.length - 1]);

		/*
		//	we're now expecting this to be here
		var MdatStartPosition = MdatAtom.HasValue ? MdatAtom.Value.AtomDataFilePosition : (long?)null;
* /
		Pop.Debug(`todo; grab last mdat?`);
		let MdatStartPosition = null;
		/  *
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
		* /
		const Samples = [];	//	array of Sample_t

		const TimeScale = MovieHeader ? MovieHeader.TimeScale : 1;

		function TimeToMs(TimeUnit)
		{
			//	to float
			const Timef = TimeUnit * TimeScale;
			const TimeMs = Timef * 1000.0;
			return Math.floor(TimeMs);	//	round to int
		}

		let SampleIndex = 0;
		for ( let i=0;	i<ChunkMetas.length;	i++)
		{
			const SampleMeta = ChunkMetas[i];
			const ChunkIndex = i;
			let ChunkFileOffset = ChunkOffsets[ChunkIndex];

			for ( let s=0;	s<SampleMeta.SamplesPerChunk;	s++ )
			{
				const Sample = new Sample_t();

				if ( MdatStartPosition !== null )
					Sample.DataPosition = ChunkFileOffset - MdatStartPosition.Value;
				else
					Sample.DataFilePosition = ChunkFileOffset;

				Sample.DataSize = SampleSizes[SampleIndex];
				Sample.IsKeyframe = SampleKeyframes[SampleIndex];
				Sample.DecodeTimeMs = TimeToMs( SampleDecodeTimes[SampleIndex] );
				Sample.DurationMs = TimeToMs( SampleDurations[SampleIndex] );
				Sample.PresentationTimeMs = TimeToMs( SampleDecodeTimes[SampleIndex] + SamplePresentationTimeOffsets[SampleIndex] );
				Samples.push(Sample);

				ChunkFileOffset += Sample.DataSize;
				SampleIndex++;
			}
		}

		if (SampleIndex != SampleSizes.length)
			Pop.Warning(`Enumerated ${SampleIndex} samples, expected ${SampleSizes.length}`);

		return Samples;
		*/
}


