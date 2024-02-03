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


//	recursive atom struct
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

public struct SampleMeta : Decodable, Identifiable, Hashable
{
	public static func == (lhs: SampleMeta, rhs: SampleMeta) -> Bool {
		lhs.id == rhs.id
	}
	
	//	uid required to be iterable. todo: generate uid for tree so elements stay persistent
	public let id = UUID()
	
	public let Keyframe: Bool
	public let FilePosition: Int
	public let DataSize: Int
	public let DecodeTimeMs: Int
	public let PresentationTimeMs: Int
	public let DurationMs: Int
}

public struct TrackMeta : Decodable, Identifiable, Hashable
{
	public static func == (lhs: TrackMeta, rhs: TrackMeta) -> Bool {
		lhs.id == rhs.id
	}
	
	//	uid required to be iterable. todo: generate uid for tree so elements stay persistent
	public let id = UUID()
	
	public let Codec: String
	//public let TrackNumber : Int		//	these start at 1 in mp4s!
	public let Samples : [SampleMeta]?
	public let SampleDecodeTimes : [Int]	//	to save json memory (structs are too big!) just a list of decode ms's for now
}

public struct Mp4Meta: Decodable
{
	//	gr: using =nil seems to be breaking parsing
	public let Error: String?
	public let RootAtoms : [String]?	//	will be a tree
	public let IsFinished: Bool?		//	decoder has finished - will be missing if just an error
	public let Mp4BytesParsed : Int?
	public let AtomTree : [AtomMeta]?
	public let Instance : Int?	//	debugging
	let Tracks : [TrackMeta]?

	public init(error:String)
	{
		Error = error
		RootAtoms = nil
		IsFinished = true
		Mp4BytesParsed = nil
		AtomTree = nil
		Instance = nil
		Tracks = nil
	}
	
	public init()
	{
		Error = nil
		RootAtoms = nil
		IsFinished = true
		Mp4BytesParsed = nil
		AtomTree = nil
		Instance = nil
		Tracks = nil
	}

	public var tracks : [TrackMeta]
	{
		get
		{
			return Tracks ?? []
		}
	}
}


public class PopMp4Instance
{
	var instanceWrapper : Mp4DecoderWrapper
	//var instance : CInt = 0
	var allocationError : String?

	public init(Filename:String)
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
	public func WaitForMetaChange() async -> Mp4Meta
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

