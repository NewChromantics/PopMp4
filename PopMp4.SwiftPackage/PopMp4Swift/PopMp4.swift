import SwiftUI
import PopMp4Objc


struct PopMp4Error : LocalizedError
{
	let error: String

	init(_ description: String) {
		error = description
	}

	var errorDescription: String? {
		error
	}
}



public struct AtomMeta: Decodable, Identifiable, Hashable
{
	public static func == (lhs: AtomMeta, rhs: AtomMeta) -> Bool {
		lhs.id == rhs.id
	}
	
	//	uid required to be iterable. todo: generate uid for tree so elements stay persistent
	public let id = UUID()
	
	public let Fourcc: String
	public let AtomSizeBytes : Int		//	header + content size
	public let HeaderSizeBytes : Int
	public let ContentSizeBytes : Int
	public let ContentsFilePosition : Int

	public let Children : [AtomMeta]?
	
	
}

public struct Mp4Meta: Decodable//, Identifiable
{
	//public let id = UUID()	//	no help with multiple documents showing same data

	//	gr: using =nil seems to be breaking parsing
	public let Error: String?
	public let RootAtoms : [String]?	//	will be a tree
	public let IsFinished: Bool?		//	decoder has finished - will be missing if just an error
	public let Mp4BytesParsed : Int?
	public let AtomTree : [AtomMeta]?
	public let Instance : Int?	//	debugging

	init(error:String)
	{
		Error = error
		RootAtoms = nil
		IsFinished = true
		Mp4BytesParsed = nil
		AtomTree = nil
		Instance = nil
	}
	
	init()
	{
		Error = nil
		RootAtoms = nil
		IsFinished = true
		Mp4BytesParsed = nil
		AtomTree = nil
		Instance = nil
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
			instanceWrapper = Mp4DecoderWrapper()
			try instanceWrapper.allocate(withFilename: Filename)
			//instance = PopMp4_AllocDecoder(Filename)
			var Version = PopMp4_GetVersion()
			//print("Allocated instance \(instance); PopMp4 version \(Version)")
			print("Allocated instance \(instanceWrapper); PopMp4 version \(Version)")
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
			//return Mp4Meta( eror:allocationError, RootAtoms:nil, IsFinished:true, Mp4BytesParsed:0 )
			return Mp4Meta( error:allocationError! )
		}
		
		do
		{
			//var StateJson = PopMp4_GetDecodeStateJson(instance);
			var StateJson = try instanceWrapper.getDecoderStateJson()
			//print(StateJson)
			let StateJsonData = StateJson.data(using: .utf8)!
			let Meta: Mp4Meta = try! JSONDecoder().decode(Mp4Meta.self, from: StateJsonData)
			return Meta
		}
		catch let error as Error
		{
			let OutputError = "Error getting decoder state; \(error.localizedDescription)"
			return Mp4Meta( error:OutputError )
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
		mp4Decoder = nil
		lastMeta = Mp4Meta()
		print("new Mp4ViewModel")
	}
	
	@MainActor // as we change published variables, we need to run on the main thread
	public func Load(filename: String) async throws
	{
		loadingStatus = LoadingStatus.Loading
		mp4Decoder = PopMp4Instance(Filename: filename)
		
		while ( true )
		{
			try await Task.sleep(nanoseconds: 10_000_000)

			//	todo: return struct with error, tree, other meta
			var NewMeta = try await mp4Decoder!.WaitForMetaChange()
			lastMeta = NewMeta
			
			/*
			//	update data
			if ( NewMeta.RootAtoms != nil )
			{
				atomTree = NewMeta.RootAtoms!
			}
			*/
			if ( NewMeta.Error != nil )
			{
				loadingStatus = LoadingStatus.Error
				return
			}
			
			//	eof - if it's missing, we'll have to assume processing has failed (eg. just an error present)
			if ( NewMeta.IsFinished ?? true )
			{
				break
			}
		}
		loadingStatus = LoadingStatus.Finished
	}
}
