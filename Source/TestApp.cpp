#include "PopMp4.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iostream>
#include <array>
#include <thread>

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
	
		//Stream.read( Data.GetArray(), Data.GetDataSize() );
		//if ( Stream.fail() && !Stream.eof() )
		//throw Soy::AssertException("Reading stream failed (not eof)");
	
		//auto BytesRead = Stream.gcount();
		//Data.SetSize( BytesRead );
		
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
	while(!EndOfFile)
	{
		//	may want a blocking call, but for now we're assuming an engine is calling this basically once per frame
		uint64_t Timestamp = 0;
		uint8_t SampleBuffer[1024];
		uint32_t SampleSize = std::size(SampleBuffer);
		uint16_t Stream = 0;
		if ( !PopMp4_PopSample( Instance, &Timestamp, &Stream, SampleBuffer, &SampleSize, &EndOfFile ) )
		{
			if ( EndOfFile )
				continue;
			//	nothing waiting
			std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
			continue;
		}
		
		std::cout << "Got sample Timestamp=" << Timestamp << " Stream=" << Stream << " Size=" << SampleSize <<  std::endl;
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
