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



public struct AtomMeta: Decodable, Identifiable
{
	//	uid required to be iterable. todo: generate uid for tree so elements stay persistent
	public let id = UUID()
	
	public let Fourcc: String
	public let AtomSizeBytes : Int		//	header + content size
	public let HeaderSizeBytes : Int
	public let ContentSizeBytes : Int
	public let ContentsFilePosition : Int

	public let Children : [AtomMeta]?
	
	
}

public struct Mp4Meta: Decodable
{
	//	gr: using =nil seems to be breaking parsing
	public let Error: String?
	public let RootAtoms : [String]?	//	will be a tree
	public let IsFinished: Bool?		//	decoder has finished - will be missing if just an error
	public let Mp4BytesParsed : Int?
	public let AtomTree : [AtomMeta]?
	
	init(Error:String)
	{
		self.Error = Error
		RootAtoms = nil
		IsFinished = true
		Mp4BytesParsed = nil
		AtomTree = nil
	}
	
	init()
	{
		Error = nil
		RootAtoms = nil
		IsFinished = true
		Mp4BytesParsed = nil
		AtomTree = nil
	}
	
	public var debug: String
	{
		let BytesParsed = Mp4BytesParsed ?? 0
		return "Parsed \(BytesParsed) bytes"
	}
}

//	based on public class CondenseStream
class PopMp4Instance
{
	var instanceWrapper : Mp4DecoderWrapper
	//var instance : CInt = 0
	var allocationError : String?

	init(Filename:String)
	{
		do
		{
			self.instanceWrapper = Mp4DecoderWrapper()
			try self.instanceWrapper.allocate(withFilename: Filename)
			//self.instance = PopMp4_AllocDecoder(Filename)
			var Version = PopMp4_GetVersion()
			//print("Allocated instance \(self.instance); PopMp4 version \(Version)")
			print("Allocated instance \(self.instanceWrapper); PopMp4 version \(Version)")
		}
		catch
		{
			allocationError = error.localizedDescription
		}
	}
	
	//	returns null when finished/eof
	func WaitForMetaChange() async -> Mp4Meta
	{
		if ( allocationError != nil )
		{
			//return Mp4Meta( Error:allocationError, RootAtoms:nil, IsFinished:true, Mp4BytesParsed:0 )
			return Mp4Meta( Error:allocationError! )
		}
		
		do
		{
			//var StateJson = PopMp4_GetDecodeStateJson(self.instance);
			var StateJson = try self.instanceWrapper.getDecoderStateJson()
			print(StateJson)
			let StateJsonData = StateJson.data(using: .utf8)!
			let Meta: Mp4Meta = try! JSONDecoder().decode(Mp4Meta.self, from: StateJsonData)
			return Meta
		}
		catch let error as Error
		{
			let OutputError = "Error getting decoder state; \(error.localizedDescription)"
			return Mp4Meta( Error:OutputError )
		}
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


	@Published public var loadingStatus = LoadingStatus.Init
	@Published public var lastMeta : Mp4Meta
	public var error: String?	{	return lastMeta.Error;	}
	var mp4Decoder : PopMp4Instance?
	
	
	public init()
	{
		self.mp4Decoder = nil
		self.lastMeta = Mp4Meta()
	}
	
	@MainActor // as we change published variables, we need to run on the main thread
	public func Load(filename: String) async throws
	{
		self.loadingStatus = LoadingStatus.Loading
		self.mp4Decoder = PopMp4Instance(Filename: filename)
		
		while ( true )
		{
			try await Task.sleep(nanoseconds: 10_000_000)

			//	todo: return struct with error, tree, other meta
			var NewMeta = try await self.mp4Decoder!.WaitForMetaChange()
			self.lastMeta = NewMeta
			
			/*
			//	update data
			if ( NewMeta.RootAtoms != nil )
			{
				self.atomTree = NewMeta.RootAtoms!
			}
			*/
			if ( NewMeta.Error != nil )
			{
				self.loadingStatus = LoadingStatus.Error
				return
			}
			
			//	eof - if it's missing, we'll have to assume processing has failed (eg. just an error present)
			if ( NewMeta.IsFinished ?? true )
			{
				break
			}
		}
		self.loadingStatus = LoadingStatus.Finished
	}
}
