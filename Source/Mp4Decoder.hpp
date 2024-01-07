#pragma once

#include <span>
#include <thread>


//	class which uses the MP4 _parser_ against incoming data
//	to output meta, samples etc
namespace PopMp4
{
	class Decoder_t;
	class DecoderParams_t;
}


class DataSource_t
{
public:
};

//	data pushed in
class DataSourceBuffer_t : public DataSource_t
{
public:
	
};

/*	todo: mmap file reader
class DataSourceFile_t : public DataSource_t
{
};
*/

class PopMp4::DecoderParams_t
{
public:
	//std::string		mFilename;	//	read directly from a file
};

class PopMp4::Decoder_t
{
public:
	void		PushData(std::span<uint8_t> Data);
	
protected:
	void		ParseThread();
	void		ParseIteration();
	
protected:
	std::thread						mParseThread;
	std::shared_ptr<DataSource_t>	mInputSource;
};
