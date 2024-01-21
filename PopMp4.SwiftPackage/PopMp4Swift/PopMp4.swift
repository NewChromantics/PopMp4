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
	let RootAtoms : [String]?	//	will be a tree
	let IsFinished: Bool?		//	decoder has finished - will be missing if just an error
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
			return Mp4Meta( Error:allocationError, RootAtoms:nil, IsFinished:true )
		}
		
		do
		{
			//var StateJson = PopMp4_GetDecodeStateJson(self.instance);
			var StateJson = try self.instanceWrapper.getDecoderStateJson()
			let StateJsonData = StateJson.data(using: .utf8)!
			let Meta: Mp4Meta = try! JSONDecoder().decode(Mp4Meta.self, from: StateJsonData)
			return Meta
		}
		catch let error as Error
		{
			let OutputError = "Error getting decoder state; \(error.localizedDescription)"
			return Mp4Meta( Error:OutputError, RootAtoms:nil, IsFinished:true )
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


	@Published public var atomTree : [String]
	@Published public var loadingStatus = LoadingStatus.Init
	@Published public var error : String?
	var mp4Decoder : PopMp4Instance?
	
	
	public init()
	{
		self.mp4Decoder = nil
		self.atomTree = []
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
			
			//	update data
			if ( NewMeta.RootAtoms != nil )
			{
				self.atomTree = NewMeta.RootAtoms!
			}
			
			if ( NewMeta.Error != nil )
			{
				self.error = NewMeta.Error
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
