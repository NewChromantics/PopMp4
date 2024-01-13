#include "PopMp4.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <functional>

#include "gtest/gtest.h"
#define GTEST_TEST_NAME	GetCurrentTestName()

std::string GetCurrentTestName()
{
	//	https://github.com/google/googletest/blob/main/docs/advanced.md#getting-the-current-tests-name
	auto* const test_info = testing::UnitTest::GetInstance()->current_test_info();
	
	//	gr: todo: make sure this in the format that matches the filter
	std::stringstream TestName;
	TestName << test_info->test_suite_name() << "." << test_info->name();
	return TestName.str();
}

/*
void InputThread(int Instance,std::istream& Input)
{
	std::cout << "starting input thread..." << std::endl;
	while ( Input.good() )
	{
		//	gr: without peek()... readsome returns nothing??
		auto Peek = Input.peek();
		if ( Peek == std::char_traits<char>::eof() )
			break;

		auto eof = Input.eof();
		auto bad = Input.bad();
		auto fail = Input.fail();
		auto good = Input.good();
		if ( bad || fail || eof )
			break;

		char Buffer[1024*64];
		auto Size = Input.readsome( Buffer, std::size(Buffer) );
		auto BytesRead = Input.gcount();	//	should match size
		auto* Buffer8 = reinterpret_cast<uint8_t*>(Buffer);
		PopMp4_PushMp4Data( Instance, Buffer8, Size, eof );
		//std::cout << "read " << Size << std::endl;
	}
	//	send EOF
	PopMp4_PushMp4Data( Instance, nullptr, 0, true );
	std::cout << "finished input thread." << std::endl;
}


void OutputThread(int Instance,std::ostream& Output)
{
	std::cout << "starting output thread..." << std::endl;

	bool EndOfFile = false;
	int SampleCounter = 0;
	while(!EndOfFile)
	{
		//	may want a blocking call, but for now we're assuming an engine is calling this basically once per frame
		uint64_t PresentationTimeMs = 0;
		uint64_t DecodeTimeMs = 0;
		uint64_t DurationMs = 0;
		bool IsKeyframe = false;
		uint8_t SampleBuffer[1024];
		uint32_t SampleSize = std::size(SampleBuffer);
		uint16_t Stream = 0;
		if ( !PopMp4_PopSample( Instance, &EndOfFile, SampleBuffer, &SampleSize, &PresentationTimeMs, &DecodeTimeMs, &Stream, &IsKeyframe, &DurationMs ) )
		{
			if ( EndOfFile )
				continue;
			//	nothing waiting
			std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
			continue;
		}
		
		std::stringstream Debug;
		Debug << "Got sample #" << SampleCounter;
		Debug << " PresentationTime=" << PresentationTimeMs;
		Debug << " DecodeTimeMs=" << DecodeTimeMs;
		Debug << " DurationMs=" << DurationMs;
		Debug << " IsKeyframe=" << IsKeyframe;
		Debug << " SampleSize=" << SampleSize;
		Debug << " Stream=" << Stream;
		
		Debug << " Data[8]=";
		for ( int i=0;	i<std::min( std::min<int>( SampleSize, std::size(SampleBuffer) ), 20 );	i++ )
			Debug << " " << std::hex << ((SampleBuffer[i] < 0xa) ? "0":"") << (int)(SampleBuffer[i]);
		
		std::cout << Debug.str() << std::endl;
		SampleCounter++;
	}

	std::cout << "finished output thread." << std::endl;
}


int main(int argc, const char * argv[]) 
{
	auto Instance = PopMp4_CreateDecoder();
	
	//	todo: file from args
	//	todo: file from stdin
	std::ifstream Mp4Stream("/Volumes/Code/PopMp4/cr.mp4", std::ios_base::binary);
	std::ostream& DebugStream = std::cout;

	auto InputThreadRunner = [&](void*)
	{
		InputThread( Instance, Mp4Stream );
	};
	auto OutputThreadRunner = [&](void*)
	{
		OutputThread( Instance, DebugStream );
	};

	std::thread InputThread(InputThreadRunner,nullptr);
	std::thread OutputThread(OutputThreadRunner,nullptr);
	
	
	//	wait for threads to finish
	if (InputThread.joinable())
		InputThread.join();
	if (OutputThread.joinable())
		OutputThread.join();
	
	return 0;
}
*/

std::string GetVersion()
{
	auto Version = PopMp4_GetVersionThousand();
	auto VersionMajor = (Version / (1000*1000)) % 1000;
	auto VersionMinor = (Version / (1000)) % 1000;
	auto VersionPatch = (Version / 1) % 1000;
	std::stringstream VersionString;
	VersionString << VersionMajor << "." << VersionMinor << "." << VersionPatch;
	return VersionString.str();
}

int main(int argc, const char * argv[])
{
	std::cout << "PopMp4 version " << GetVersion() << std::endl;
	
	//	must be called prior to running any tests
	testing::InitGoogleTest();
	
	std::string_view GTestFilter = "";
	//std::string_view GTestFilter = "**DecodeFile**";
	if ( !GTestFilter.empty() )
	{
		using namespace testing;
		GTEST_FLAG(filter) = std::string(GTestFilter);
	}
	
	static bool BreakOnTestError = false;
	if ( BreakOnTestError )
		GTEST_FLAG_SET(break_on_failure,true);
	
	const auto ReturnCode = RUN_ALL_TESTS();
	
	if ( ReturnCode != 0 )
		return ReturnCode;
	
	std::cerr << "Integration tests succeeded" << std::endl;
	return 0;
}



class General_Tests : public testing::TestWithParam<bool>
{
protected:
};


TEST(General_Tests, PopMp4UnitTest )
{
	//EXPECT_NO_THROW( PopJson::UnitTest() );
}


class DecodeResults_t
{
public:
	int					RootAtomCount = 0;
	
	friend std::ostream& operator<<(std::ostream& os, const DecodeResults_t& Params)
	{
		os << "DecodeResults_t-->";
		os << " RootAtomCount=" << Params.RootAtomCount;
		return os;
	}
};


class DecodeTestParams_t
{
public:
	std::string		Filename;
	DecodeResults_t	ExpectedResults;
	
	friend std::ostream& operator<<(std::ostream& os, const DecodeTestParams_t& Params)
	{
		os << "DecodeTestParams_t-->";
		os << " Filename=" << Params.Filename;
		os << " ExpectedResults=" << Params.ExpectedResults;
		return os;
	}
};

class Decode_Tests : public testing::TestWithParam<DecodeTestParams_t>
{
};

auto DecodeTestValues = ::testing::Values
(
	DecodeTestParams_t{.Filename="TestData/Test.mp4", .ExpectedResults{.RootAtomCount=1} }
);
	
INSTANTIATE_TEST_SUITE_P( Decode_Tests, Decode_Tests, DecodeTestValues );



std::thread RunThreadSafely(std::function<void()> Functor,std::string& Error,std::mutex& ErrorLock)
{
	auto RunAndCatch = [=,&Error,&ErrorLock]()
	{
		try
		{
			Functor();
		}
		catch(std::exception& e)
		{
			std::lock_guard Lock(ErrorLock);
			Error = e.what();
		}
	};
	return std::thread( RunAndCatch );
}
	


TEST_P(Decode_Tests,DecodeAtomTree)
{
	auto Params = GetParam();
	
	auto Decoder = PopMp4_CreateDecoder();

	auto TestStartTime = std::chrono::system_clock::now();
	auto TestMaxDuration = std::chrono::seconds(20);

	std::mutex OutputLock;
	std::string Error;
	bool FinishedDecode = false;
	
	auto ThrowIfTimeout = [&]()
	{
		auto ElapsedSecs = std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now() - TestStartTime );
		if ( ElapsedSecs.count() > TestMaxDuration.count() )
			throw std::runtime_error("Test timedout");
	};
	auto IsStillRunning = [&]()
	{
		try
		{
			ThrowIfTimeout();
			std::scoped_lock Lock(OutputLock);
			if ( FinishedDecode )
				return false;
			if ( !Error.empty() )
				return false;
			return true;
		}
		catch(...)
		{
			return false;
		}
	};
	

	//	monitor state
	auto ReadStateThread = [&]()
	{
		std::vector<char> JsonBuffer( 1024 * 1024 * 1 );
		while ( IsStillRunning() )
		{
			ThrowIfTimeout();
			
			PopMp4_GetDecoderState( Decoder, JsonBuffer.data(), JsonBuffer.size() );
			//	parse
			throw std::runtime_error("todo: parse state json");
		}
	};
	auto ReadThread = RunThreadSafely( ReadStateThread, Error, OutputLock );
	
	//	stream data into decoder
	auto OnReadData = [&](std::span<uint8_t> Data,bool Eof)
	{
		PopMp4_PushMp4Data( Decoder, Data.data(), Data.size(), Eof );
	};
	
	while ( IsStillRunning() )
	{
		std::cerr << GTEST_TEST_NAME << " waiting to finish..." << std::endl;
		std::this_thread::sleep_for( std::chrono::seconds(1) );
	}
	
	if ( ReadThread.joinable() )
		ReadThread.join();
	
	if ( !Error.empty() )
		FAIL() << Error;
	//auto Image = DecodeFileFirstFrame( Params.Filename, Params.DecoderName );
}
