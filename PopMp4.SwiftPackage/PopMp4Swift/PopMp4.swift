import SwiftUI
import PopMp4Objc



/*
public func PopMp4_PrintVersion()
{
	var VersionThousand = CRStreamer_GetVersionThousand()
	var Version = CRStreamer_GetVersion()
	//var Version = "x"
	//Version = CRStreamer_GetVersionThousand_Objc()
	print("CRStreamer version \(Version) (\(VersionThousand))")
}
 */

struct PopMp4Error : LocalizedError
{
	let description: String

	init(_ description: String) {
		self.description = description
	}

	var errorDescription: String? {
		description
	}
}

public struct Mp4Meta: Decodable
{
	let Error: String?
	let AtomTree : [String]?
	let EndOfFile: Bool
}

//	based on public class CondenseStream
class PopMp4Instance
{
	var instance : CInt = 0
	
	//	these will come out of CAPI calls
	var atomTree : [String] = ["Hello"]
	var atomTreeCounter = 0
	
	init(Filename:String) throws
	{
		self.instance = PopMp4_AllocDecoder(Filename)

		if ( self.instance == 0 )
		{
			//throw PopMp4Error("Failed to allocate MP4 decoder for \(Filename)")
		}
		
		var Version = PopMp4_GetVersion()
		print("Allocated instance \(self.instance); PopMp4 version \(Version))")
	}
	
	//	returns null when finished/eof
	func WaitForMetaChange() async -> Mp4Meta
	{
		//	gr: replace this will call to CAPI to get new meta
		var SleepMs = 10
		var SleepNano = (SleepMs * 1_000_000)
		do
		{
			try await Task.sleep(nanoseconds: UInt64(SleepNano) )
		}
		catch let error as Error
		{
			return Mp4Meta( Error:error.localizedDescription, AtomTree:nil, EndOfFile:true )
		}
		atomTreeCounter += 1
		
		//	send eof
		if ( atomTreeCounter >= 1000 )
		{
			return Mp4Meta( Error:nil, AtomTree:nil, EndOfFile:true )
		}
		
		var NewAtom = "Atom #\(atomTreeCounter)"
		atomTree.append(NewAtom)
			
		return Mp4Meta( Error:nil, AtomTree:atomTree, EndOfFile:false )
	}
			
}


public class Mp4ViewModel : ObservableObject
{
	public enum LoadingStatus : CustomStringConvertible
	{
		case Init, Loading, Finished, Error

		public var description: String
		{
			switch self
			{
				case .Init: return "Init"
				case .Loading: return "Loading"
				case .Finished: return "Finished"
				case .Error: return "Error"
			}
		}
	}


	@Published public var atomTree : [String]
	@Published public var loadingStatus = LoadingStatus.Init
	var mp4Decoder : PopMp4Instance?
	
	
	public init()
	{
		self.mp4Decoder = nil
		self.atomTree = ["Model's initial Tree"]
	}
	
	@MainActor // as we change published variables, we need to run on the main thread
	public func Load(filename: String) async throws
	{
		self.loadingStatus = LoadingStatus.Loading
		try self.mp4Decoder = PopMp4Instance(Filename: filename)
		
		while ( true )
		{
			//	todo: return struct with error, tree, other meta
			var NewMeta = try await self.mp4Decoder!.WaitForMetaChange()
			
			//	todo: do something with error
			if ( NewMeta.Error != nil )
			{
				self.atomTree = [NewMeta.Error!]
				self.loadingStatus = LoadingStatus.Error
				return
			}
			
			//	update data
			if ( NewMeta.AtomTree != nil )
			{
				self.atomTree = NewMeta.AtomTree!
			}
			
			//	eof
			if ( NewMeta.EndOfFile )
			{
				break
			}
		}
		self.loadingStatus = LoadingStatus.Finished
	}
}
