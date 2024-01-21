#pragma once

#include <span>
#include <thread>
#include "../Source_TestApp/PopJson/PopJson.hpp"

class Atom_t;



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
	virtual ~DataSource_t(){};
	
	virtual void		PushData(std::span<uint8_t> Data)=0;
	virtual void		PushEndOfFile()=0;
	virtual void		LockData(size_t FilePosition,size_t Size,std::function<void(std::span<uint8_t>)> OnPeekData)=0;
	virtual void		LockData(std::function<void(std::span<uint8_t>,bool HadEof)> OnPeekData)=0;
	virtual bool		HadEof()=0;
};

//	data pushed in
class DataSourceBuffer_t : public DataSource_t
{
public:
	virtual void		PushData(std::span<uint8_t> Data) override;
	virtual void		PushEndOfFile() override;
	virtual void		LockData(size_t FilePosition,size_t Size,std::function<void(std::span<uint8_t>)> OnPeekData) override;
	virtual void		LockData(std::function<void(std::span<uint8_t>,bool HadEof)> OnPeekData) override;
	virtual bool		HadEof() override;

public:
	std::mutex				mDataLock;
	bool					mHadEof = false;
	std::vector<uint8_t>	mData;
};


class DataSourceFile_t : public DataSource_t
{
public:
	DataSourceFile_t(std::string_view Filename);
	~DataSourceFile_t();

	virtual void		PushData(std::span<uint8_t> Data) override;
	virtual void		PushEndOfFile() override;
	virtual void		LockData(size_t FilePosition,size_t Size,std::function<void(std::span<uint8_t>)> OnPeekData) override;
	virtual void		LockData(std::function<void(std::span<uint8_t>,bool HadEof)> OnPeekData) override;
	virtual bool		HadEof() override;
	void				OnError(std::string_view Error);
	
protected:
	void				ReadFileThread();
	void				ThrowIfError();
	
	//	todo: mmap file reader
	//		but for now, stream file into a buffer
	DataSourceBuffer_t	mFileReadBufferSource;
	std::string			mFilename;
	std::thread			mReadFileThread;
	
	std::mutex			mDataLock;
	std::string			mError;
};


class PopMp4::DecoderParams_t
{
public:
	//std::string		mFilename;	//	read directly from a file
};



/*
class PopMp4::TSample : public Sample_t
{
public:
	TSample(const Sample_t& Sample) :
		Sample_t	( Sample )
	{
	}
	TSample(){};
	uint16_t				mStream = 0;
	std::vector<uint8_t>	mData;
};


class PopMp4::TDecoderParams
{
public:
	bool	mConvertH264ToAnnexB = true;	//	data in mp4s will be prefixed with length (8,16 or 32bit). Enable this to convert sample data to Nalu-prefixed annexb 0 0 0 1 (no length)
};

class PopMp4::TDecoder
{
public:
	TDecoder(TDecoderParams Params);
	~TDecoder();

	void						PushData(const uint8_t* Data,size_t DataSize,bool EndOfFile);	//	std::span would be better, c++20
	std::shared_ptr<TSample>	PopSample();
	bool						HasDecoderThreadFinished()	{	return mDecoderThreadFinished;	}
	
private:
	void						DecoderThread();
	void						WakeDecoderThread();

	//	this function is basically the only mp4-specific interface,
	//	if we wanted to support more containers, do it here
	bool						DecodeNext();	//	returns true if we decoded something, false if we need more data
	
	void						OnNewSample(const Sample_t& Sample);
	void						OnNewCodec(Codec_t& Codec);
	bool						ReadBytes(std::span<uint8_t>& Buffer,size_t FilePosition);

private:
	std::mutex					mPendingDataLock;
	std::vector<uint8_t>		mPendingData;
	size_t						mPendingDataFilePosition = 0;	//	pendingdata[0] is at this position in the file
	bool						mHadEndOfFile = false;
	Mp4::Parser_t				mParser;					//	the actual data decoder

	TDecoderParams				mParams;

	std::mutex					mSamplesLock;
	std::vector<std::shared_ptr<TSample>>	mSamples;
	
	bool						mRunThread = true;
	std::thread					mDecodeThread;
	bool						mDecoderThreadFinished = false;	//	will parse no more data
};
*/

class PopMp4::Decoder_t
{
public:
	Decoder_t(PopJson::ViewBase_t& Options);
	~Decoder_t();
	
	void			PushData(std::span<uint8_t> Data);
	void			PushEndOfFile();
	PopJson::Json_t	GetState();
	
protected:
	void		DecoderThread();
	void		WakeDecoderThread();
	bool		DecodeIteration();		//	throw on error, false to stop
	void		OnError(std::exception& Error)	{	OnError(Error.what());	}
	void		OnError(std::string_view Error);
	
	void		ValidateAtom(Atom_t& Atom);
	
protected:
	std::shared_ptr<DataSource_t>	mInputSource;
	
	bool						mRunThread = true;
	std::thread					mDecodeThread;
	bool						mDecoderThreadFinished = false;	//	will parse no more data

	std::mutex					mDataLock;
	size_t						mMp4BytesRead = 0;
	std::string					mError;
	std::vector<uint32_t>		mExtractedMp4RootAtoms;
};
