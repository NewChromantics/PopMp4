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
	
	public var Fourcc: String=""
	public var AtomSizeBytes : Int=0		//	header + content size
	public var HeaderSizeBytes : Int=0
	public var ContentSizeBytes : Int=0
	public var ContentsFilePosition : Int=0

	public var Children : [AtomMeta]? = nil
	
	public init(fourcc:String)
	{
		Fourcc = fourcc
	}
	
	public func FindAtom(match:UUID) -> AtomMeta?
	{
		if ( id == match )
		{
			return self
		}
		for child in Children ?? []
		{
			var childMatch = child.FindAtom(match: match)
			if ( childMatch != nil )
			{
				return childMatch
			}
		}
		return nil
	}
	
	public func MatchFourcc(matchFourcc:String) -> Bool
	{
		return Fourcc.lowercased().contains(matchFourcc)
	}
	
	public func ContainsFourcc(matchFourcc:String) -> Bool
	{
		if ( MatchFourcc(matchFourcc: matchFourcc) )
		{
			return true
		}
			
		//	check children
		for child in Children ?? []
		{
			if ( child.ContainsFourcc(matchFourcc:matchFourcc) )
			{
				return true
			}
		}
		
		return false
	}
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
	
	//	only this keys are decoded by json
	private enum CodingKeys: String, CodingKey
	{
		case Codec
		case Samples
		case SampleDecodeTimes
	}
	
	//	uid required to be iterable. todo: generate uid for tree so elements stay persistent
	public var id = UUID()
	
	public var Codec: String=""
	//public var TrackNumber : Int		//	these start at 1 in mp4s!
	public var Samples : [SampleMeta]? = nil
	public var SampleDecodeTimes : [Int]=[]	//	to save json memory (structs are too big!) just a list of decode ms's for now
	
	public init(codec:String)
	{
		Codec = codec
	}
}

public struct Mp4Meta: Decodable
{
	//	gr: using =nil seems to be breaking parsing
	public var Error: String?
	public var RootAtoms : [String]?	//	will be a tree
	public var IsFinished: Bool?		//	decoder has finished - will be missing if just an error
	public var Mp4BytesParsed : Int?
	public var AtomTree : [AtomMeta]?
	public var Instance : Int?	//	debugging
	public var Tracks : [TrackMeta]?

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
		IsFinished = false
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

	public init()
	{
		do
		{
			instanceWrapper = Mp4DecoderWrapper()
			try instanceWrapper.allocate()
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
	
	public func PushData(data:Data)
	{
		instanceWrapper.push(data)
	}
	
	public func PushEndOfFile()
	{
		instanceWrapper.pushEndOfFile()
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
			print(StateJson)
			
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

