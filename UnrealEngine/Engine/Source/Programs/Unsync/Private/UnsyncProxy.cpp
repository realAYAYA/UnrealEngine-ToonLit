// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncProxy.h"
#include "UnsyncAuth.h"
#include "UnsyncCompression.h"
#include "UnsyncFile.h"
#include "UnsyncJupiter.h"
#include "UnsyncPool.h"
#include "UnsyncProgress.h"

#include <fmt/format.h>
#include <atomic>
#include <json11.hpp>

namespace unsync {

struct FUnsyncProtocolImpl : FRemoteProtocolBase
{
	FUnsyncProtocolImpl(const FRemoteDesc&			   InRemoteDesc,
						const FRemoteProtocolFeatures& InFeatures,
						const FAuthDesc*			   InAuthDesc,
						const FBlockRequestMap*		   InRequestMap,
						const FTlsClientSettings*	   TlsSettings);
	virtual ~FUnsyncProtocolImpl() override;
	virtual bool			 IsValid() const override;
	virtual FDownloadResult	 Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback) override;
	virtual TResult<FBuffer> DownloadManifest(std::string_view ManifestName) override
	{
		return AppError(L"Manifests can't be downloaded from UNSYNC proxy.");
	};
	virtual void Invalidate() override;
	virtual bool Contains(const FDirectoryManifest& Manifest) override { return true; }	 // TODO: check files on the unsync proxy

	ESocketSecurity GetSocketSecurity() const;

	bool						 bIsConnetedToHost = false;
	std::unique_ptr<FSocketBase> SocketHandle;

	const FRemoteProtocolFeatures Features;

	static void SendTelemetryEvent(const FRemoteDesc& RemoteDesc, const FTelemetryEventSyncComplete& Event);
};

FProxy::FProxy(const FRemoteDesc& RemoteDesc, const FRemoteProtocolFeatures& InFeatures, const FAuthDesc* InAuthDesc, const FBlockRequestMap* InRequestMap)
{
	UNSYNC_ASSERT(InRequestMap);

	FTlsClientSettings TlsSettings = RemoteDesc.GetTlsClientSettings();

	if (RemoteDesc.Protocol == EProtocolFlavor::Jupiter)
	{
		auto Inner	 = new FJupiterProtocolImpl(RemoteDesc, InRequestMap, &TlsSettings, RemoteDesc.HttpHeaders);
		ProtocolImpl = std::unique_ptr<FRemoteProtocolBase>(Inner);
	}
	else if (RemoteDesc.Protocol == EProtocolFlavor::Unsync)
	{
		auto* Inner	 = new FUnsyncProtocolImpl(RemoteDesc, InFeatures, InAuthDesc, InRequestMap, &TlsSettings);
		ProtocolImpl = std::unique_ptr<FRemoteProtocolBase>(Inner);
	}
	else
	{
		UNSYNC_FATAL(L"Unknown remote protocol %d", (int)RemoteDesc.Protocol);
	}
}

FUnsyncProtocolImpl::FUnsyncProtocolImpl(const FRemoteDesc&				RemoteDesc,
										 const FRemoteProtocolFeatures& InFeatures,
										 const FAuthDesc*				InAuthDesc,
										 const FBlockRequestMap*		InRequestMap,
										 const FTlsClientSettings*		TlsSettings)
: FRemoteProtocolBase(RemoteDesc, InRequestMap)
, Features(InFeatures)
{
	if (RemoteDesc.bTlsEnable && TlsSettings)
	{
		FSocketHandle RawSocketHandle = SocketConnectTcp(RemoteDesc.Host.Address.c_str(), RemoteDesc.Host.Port);
		SocketSetRecvTimeout(RawSocketHandle, RemoteDesc.RecvTimeoutSeconds);

		if (RawSocketHandle)
		{
			FSocketTls* TlsSocket = new FSocketTls(RawSocketHandle, *TlsSettings);
			if (TlsSocket->IsTlsValid())
			{
				SocketHandle = std::unique_ptr<FSocketTls>(TlsSocket);
			}
			else
			{
				delete TlsSocket;
			}
		}
	}

	if (!SocketHandle)
	{
		FSocketHandle RawSocketHandle = SocketConnectTcp(RemoteDesc.Host.Address.c_str(), RemoteDesc.Host.Port);
		SocketSetRecvTimeout(RawSocketHandle, RemoteDesc.RecvTimeoutSeconds);

		SocketHandle = std::unique_ptr<FSocketRaw>(new FSocketRaw(RawSocketHandle));
	}

	if (SocketHandle)
	{
		bIsConnetedToHost = [this]()
		{
			FHandshakePacket HandshakePacketTx;
			if (!SocketSendT(*SocketHandle, HandshakePacketTx))
			{
				UNSYNC_LOG(L"Failed to send the handshake packet");
				return false;
			}

			FHandshakePacket HandshakePacketRx;
			memset(&HandshakePacketRx, 0, sizeof(HandshakePacketRx));
			if (!SocketRecvT(*SocketHandle, HandshakePacketRx))
			{
				UNSYNC_LOG(L"Failed to receive the handshake packet");
				return false;
			}

			if (HandshakePacketRx.Magic != HandshakePacketTx.Magic || HandshakePacketRx.Protocol != HandshakePacketTx.Protocol ||
				HandshakePacketRx.Size != HandshakePacketTx.Size)
			{
				UNSYNC_LOG(L"Failed to receive the handshake packet");
				return false;
			}

			return true;
		}();
	}

	if (IsValid() && Features.bAuthentication && RemoteDesc.bAuthenticationRequired)
	{
		if (InAuthDesc)
		{
			bool bOk = IsValid();

			TResult<FAuthToken> AuthTokenResult = Authenticate(*InAuthDesc, 15 * 60);

			if (AuthTokenResult.IsOk())
			{
				FBufferView AccessToken = {(const uint8*)AuthTokenResult->Access.data(), AuthTokenResult->Access.length()};

				FCommandPacket Packet;
				Packet.CommandId = COMMAND_ID_AUTHENTICATE;
				bOk &= SendStruct(*SocketHandle, Packet);
				bOk &= SendBuffer(*SocketHandle, AccessToken);

				int32 ResultSize = 0;
				bOk &= SocketRecvT(*SocketHandle, ResultSize);

				FBuffer ResultBuffer;
				if (ResultSize)
				{
					ResultBuffer.Resize(ResultSize);
					bOk &= (SocketRecvAll(*SocketHandle, ResultBuffer.Data(), ResultSize) == ResultSize);
				}

				// TODO: parse authentication result packet and report errors
			}
			else
			{
				LogError(AuthTokenResult.GetError());
				UNSYNC_ERROR(L"Server requires authentication, but access token could not be acquired");
				Invalidate();
			}
		}
		else
		{
			UNSYNC_ERROR(L"Server requires authentication, but required parameters were not provided");
			Invalidate();
		}
	}
}

FProxy::~FProxy()
{
}

bool
FProxy::Contains(const FDirectoryManifest& Manifest)
{
	return ProtocolImpl.get() && ProtocolImpl->Contains(Manifest);
}

bool
FProxy::IsValid() const
{
	return ProtocolImpl.get() && ProtocolImpl->IsValid();
}

TResult<FBuffer>
FProxy::DownloadManifest(std::string_view ManifestName)
{
	if (ProtocolImpl.get())
	{
		return ProtocolImpl->DownloadManifest(ManifestName);
	}
	else
	{
		return AppError(L"Server connection is invalid");
	}
}

FDownloadResult
FProxy::Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback)
{
	if (ProtocolImpl.get())
	{
		return ProtocolImpl->Download(NeedBlocks, CompletionCallback);
	}
	else
	{
		return FDownloadResult(EDownloadRetryMode::Abort);
	}
}

bool
FUnsyncProtocolImpl::IsValid() const
{
	return bIsConnetedToHost && SocketHandle && SocketValid(*SocketHandle);
}

FDownloadResult
FUnsyncProtocolImpl::Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback)
{
	if (!IsValid())
	{
		return FDownloadResult(EDownloadRetryMode::Abort);
	}

	const EStrongHashAlgorithmID StrongHasher = RequestMap->GetStrongHasher();

	std::unordered_set<FHash128> UniqueFileNamesMd5;

	std::vector<FBlockRequest> Requests;
	Requests.reserve(NeedBlocks.Size());

	for (const FNeedBlock& Block : NeedBlocks)
	{
		if (const FBlockRequest* Request = RequestMap->FindRequest(Block.Hash))
		{
			Requests.push_back(*Request);
			UniqueFileNamesMd5.insert(Request->FilenameMd5);
		}
	}

	std::vector<const std::string*> FileListUtf8;
	for (const FHash128& It : UniqueFileNamesMd5)
	{
		const std::string* Name = RequestMap->FindFile(It);
		if (Name)
		{
			FileListUtf8.push_back(Name);
		}
	}

	bool bOk = bIsConnetedToHost;

	// begin the command

	{
		FCommandPacket Packet;
		Packet.CommandId = COMMAND_ID_GET_BLOCKS;
		bOk &= SendStruct(*SocketHandle, Packet);
	}

	// send file list followed by requests
	if (bOk)
	{
		FBuffer FileListData;

		{
			FVectorStreamOut Writer(FileListData);

			for (const std::string* Str : FileListUtf8)
			{
				uint64 Len = Str->length();	 // 64 bit length, compatible with rust bincode
				Writer.WriteT(Len);
				Writer.Write(Str->c_str(), Len);
			}
		}

		FFileListPacket FileListHeader;
		FileListHeader.DataSizeBytes = CheckedNarrow(FileListData.Size());
		FileListHeader.NumFiles		 = CheckedNarrow(FileListUtf8.size());

		bOk &= SendStruct(*SocketHandle, FileListHeader);
		bOk &= SendBuffer(*SocketHandle, FileListData);
	}

	uint64 BytesToDownload = 0;

	if (bOk)
	{
		FBuffer RequestData;

		{
			FVectorStreamOut Writer(RequestData);

			for (const FBlockRequest& It : Requests)
			{
				BytesToDownload += It.Size;
				Writer.WriteT(It);
			}
		}

		FBuffer RequestDataCompressed = Compress(RequestData.Data(), RequestData.Size());

		FRequestBlocksPacket RequestHeader;
		RequestHeader.CompressedSizeBytes	= CheckedNarrow(RequestDataCompressed.Size());
		RequestHeader.DecompressedSizeBytes = CheckedNarrow(RequestData.Size());
		RequestHeader.NumRequests			= CheckedNarrow(Requests.size());
		RequestHeader.StrongHashAlgorithmId = uint64(StrongHasher);

		bOk &= SendStruct(*SocketHandle, RequestHeader);
		bOk &= SendBuffer(*SocketHandle, RequestDataCompressed);
	}

	if (!bOk)
	{
		bIsConnetedToHost = false;
		return FDownloadResult(EDownloadRetryMode::Abort);
	}

	uint64 BytesDownloaded = 0;

	FBlockPacket BlockPacket;

	for (uint64 I = 0; I < (Requests.size() + 1) && bOk; ++I)
	{
		BlockPacket.DecompressedSize = 0;
		BlockPacket.Hash			 = {};

		uint32 PacketSize = 0;
		bOk &= SocketRecvT(*SocketHandle, PacketSize);
		bOk &= SocketRecvT(*SocketHandle, BlockPacket.Hash);
		bOk &= SocketRecvT(*SocketHandle, BlockPacket.DecompressedSize);

		uint64 CompressedDataSize = 0;
		bOk &= SocketRecvT(*SocketHandle, CompressedDataSize);

		if (!bOk)
		{
			UNSYNC_WARNING(L"Failed to receive block header");
			break;
		}

		BlockPacket.CompressedData.Resize(CompressedDataSize);

		bOk &= (SocketRecvAll(*SocketHandle, BlockPacket.CompressedData.Data(), BlockPacket.CompressedData.Size()) == CompressedDataSize);

		if (!bOk)
		{
			UNSYNC_WARNING(L"Failed to receive block buffer");
			break;
		}

		static const FHash128 TerminatorHash = FHash128{};	// response is always terminated with an empty packet

		if (BlockPacket.Hash == TerminatorHash)
		{
			// TODO: Termination packet payload may contain diagnostic messages that can be reported to the user
			break;
		}

		if (bOk)
		{
			FDownloadedBlock DownloadedBlock;
			DownloadedBlock.DecompressedSize = BlockPacket.DecompressedSize;
			DownloadedBlock.CompressedSize	 = BlockPacket.CompressedData.Size();
			DownloadedBlock.Data			 = BlockPacket.CompressedData.Data();
			CompletionCallback(DownloadedBlock, BlockPacket.Hash);
		}

		BytesDownloaded += BlockPacket.DecompressedSize;
	}

	if (!bOk)
	{
		SocketHandle	  = {};
		bIsConnetedToHost = false;
	}

	return ResultOk<FDownloadError>();
}

void
FUnsyncProtocolImpl::Invalidate()
{
	bIsConnetedToHost = false;
	SocketHandle	  = {};
}

ESocketSecurity
FUnsyncProtocolImpl::GetSocketSecurity() const
{
	if (SocketHandle)
	{
		return SocketHandle->Security;
	}
	else
	{
		return ESocketSecurity::None;
	}
}

TResult<ProxyQuery::FHelloResponse>
ProxyQuery::Hello(const FRemoteDesc& RemoteDesc, const FAuthDesc* OptAuthDesc)
{
	FTlsClientSettings TlsSettings = RemoteDesc.GetTlsClientSettings();
	FHttpConnection	   Connection(RemoteDesc.Host.Address, RemoteDesc.Host.Port, RemoteDesc.bTlsEnable ? &TlsSettings : nullptr);
	return Hello(Connection, OptAuthDesc);
}

TResult<ProxyQuery::FHelloResponse>
ProxyQuery::Hello(FHttpConnection& HttpConnection, const FAuthDesc* OptAuthDesc)
{
	const char* Url = "/api/v1/hello";

	std::string BearerToken;
	if (OptAuthDesc)
	{
		TResult<FAuthToken> AuthTokenResult = Authenticate(*OptAuthDesc, 15 * 60);
		if (AuthTokenResult.IsOk())
		{
			BearerToken = std::move(AuthTokenResult.GetData().Access);
		}
	}

	FHttpResponse Response = HttpRequest(HttpConnection, EHttpMethod::GET, Url, {} /*CustomHeaders*/, BearerToken);

	if (!Response.Success())
	{
		UNSYNC_ERROR(L"Failed to establish connection to UNSYNC server. Error code: %d.", Response.Code);
		return HttpError(fmt::format("{}:{}{}", HttpConnection.HostAddress.c_str(), HttpConnection.HostPort, Url), Response.Code);
	}

	FHelloResponse Result;

	using namespace json11;
	std::string JsonString = std::string(Response.AsStringView());

	std::string JsonErrorString;
	Json		JsonObject = Json::parse(JsonString, JsonErrorString);

	if (!JsonErrorString.empty())
	{
		return AppError(std::string("JSON parse error while connecting to UNSYNC server: ") + JsonErrorString);
	}

	if (auto& Field = JsonObject["service"]; Field.is_string())
	{
		const std::string& FieldVal = Field.string_value();
		if (FieldVal != "unsync")
		{
			return AppError(fmt::format("Expected service name 'unsync', but found '{}'.", FieldVal.c_str()));
		}
	}

	if (auto& Field = JsonObject["name"]; Field.is_string())
	{
		Result.Name = Field.string_value();
	}

	if (auto& Field = JsonObject["version"]; Field.is_string())
	{
		Result.VersionNumber = Field.string_value();
	}

	if (auto& Field = JsonObject["git"]; Field.is_string())
	{
		Result.VersionGit = Field.string_value();
	}

	if (auto& Field = JsonObject["session"]; Field.is_string())
	{
		Result.SessionId = Field.string_value();
	}

	if (auto& Field = JsonObject["auth"]; Field.is_object())
	{
		Result.AuthServerUri = Field["server"].string_value();
		Result.AuthClientId	 = Field["client_id"].string_value();
		Result.AuthAudience	 = Field["audience"].string_value();
		Result.CallbackUri	 = Field["callback"].string_value();
	}

	if (auto& Field = JsonObject["features"]; Field.is_array())
	{
		Result.FeatureNames.reserve(Field.array_items().size());
		for (auto& Elem : Field.array_items())
		{
			if (Elem.is_string())
			{
				Result.FeatureNames.push_back(Elem.string_value());

				if (Elem.string_value() == "telemetry")
				{
					Result.Features.bTelemetry = true;
				}
				else if (Elem.string_value() == "mirrors")
				{
					Result.Features.bMirrors = true;
				}
				else if (Elem.string_value() == "authentication")
				{
					Result.Features.bAuthentication = true;
				}
				else if (Elem.string_value() == "list")
				{
					Result.Features.bDirectoryListing = true;
				}
				else if (Elem.string_value() == "file")
				{
					Result.Features.bFileDownload = true;
				}
			}
		}
	}

	if (auto& Field = JsonObject["primary"]; Field.is_string())
	{
		const std::string& PrimaryHostStr = Field.string_value();
		TResult<FRemoteDesc> PrimaryHostDesc = FRemoteDesc::FromUrl(PrimaryHostStr);
		if (PrimaryHostDesc.IsOk())
		{
			Result.PrimaryHost = PrimaryHostDesc->Host;
		}
	}

	return ResultOk(std::move(Result));
}

TResult<ProxyQuery::FDirectoryListing>
ProxyQuery::FDirectoryListing::FromJson(const char* JsonString)
{
	FDirectoryListing Result;

	std::string	 JsonErrorString;
	json11::Json JsonObject = json11::Json::parse(JsonString, JsonErrorString);

	if (!JsonErrorString.empty())
	{
		return AppError(fmt::format("JSON error: {}", JsonErrorString.c_str()));
	}

	const json11::Json& EntriesObject = JsonObject["entries"];

	if (EntriesObject.is_array())
	{
		for (const auto& Elem : EntriesObject.array_items())
		{
			FDirectoryListingEntry Entry;
			for (const auto& Field : Elem.object_items())
			{
				if (Field.first == "name" && Field.second.is_string())
				{
					Entry.Name = Field.second.string_value();
				}
				else if (Field.first == "is_directory" && Field.second.is_bool())
				{
					Entry.bDirectory = Field.second.bool_value();
				}
				else if (Field.first == "mtime" && Field.second.is_number())
				{
					Entry.Mtime = uint64(Field.second.number_value());
				}
				else if (Field.first == "size" && Field.second.is_number())
				{
					Entry.Size = uint64(Field.second.number_value());
				}
			}
			Result.Entries.push_back(Entry);
		}
	}

	return ResultOk(std::move(Result));
}

TResult<ProxyQuery::FDirectoryListing>
ProxyQuery::ListDirectory(const FRemoteDesc& Remote, const FAuthDesc* AuthDesc, const std::string& Path)
{
	std::string Url = fmt::format("/api/v1/list?{}", Path);

	std::string BearerToken;
	if (AuthDesc)
	{
		TResult<FAuthToken> AuthToken = Authenticate(*AuthDesc, 5 * 60);
		if (!AuthToken.IsOk())
		{
			return MoveError<FDirectoryListing>(AuthToken);
		}

		BearerToken = std::move(AuthToken->Access);
	}

	FHttpConnection Connection = FHttpConnection::CreateDefaultHttps(Remote);

	FHttpRequest Request;
	Request.Url			= Url;
	Request.Method		= EHttpMethod::GET;
	Request.BearerToken = BearerToken;

	FHttpResponse Response = HttpRequest(Connection, Request);

	if (!Response.Success())
	{
		return HttpError(Response.Code);
	}

	Response.Buffer.PushBack(0);

	return FDirectoryListing::FromJson((const char*)Response.Buffer.Data());
}

TResult<>
ProxyQuery::DownloadFile(const FRemoteDesc&					 Remote,
						 const FAuthDesc*					 AuthDesc,
						 const std::string&					 Path,
						 ProxyQuery::FDownloadOutputCallback OutputCallback)
{
	auto CreateConnection = [Remote]
	{
		FTlsClientSettings TlsSettings = Remote.GetTlsClientSettings();
		return new FHttpConnection(Remote.Host.Address, Remote.Host.Port, &TlsSettings);
	};

	TObjectPool<FHttpConnection> ConnectionPool(CreateConnection);

	std::string Url = fmt::format("/api/v1/file?{}", Path);

	uint64 FileSize = 0;

	{
		std::unique_ptr<FHttpConnection> Connection = ConnectionPool.Acquire();

		std::string BearerToken;
		if (AuthDesc)
		{
			TResult<FAuthToken> AuthToken = Authenticate(*AuthDesc, 5 * 60);
			if (!AuthToken.IsOk())
			{
				return std::move(AuthToken.GetError());
			}
			BearerToken = std::move(AuthToken->Access);
		}

		FHttpRequest HeadRequest;
		HeadRequest.Url			   = Url;
		HeadRequest.Method		   = EHttpMethod::HEAD;
		HeadRequest.BearerToken	   = BearerToken;
		FHttpResponse HeadResponse = HttpRequest(*Connection, HeadRequest);
		if (!HeadResponse.Success())
		{
			return HttpError(HeadResponse.Code);
		}

		FileSize = HeadResponse.ContentLength;
		UNSYNC_VERBOSE(L"File size: %llu bytes (%.3f MB)", llu(FileSize), SizeMb(FileSize));

		ConnectionPool.Release(std::move(Connection));
	}

	const uint64 MaxChunkSize = 16_MB;

	FIOWriter& Result = OutputCallback(FileSize);
	if (!Result.IsValid())
	{
		return AppError(L"Failed to create download output stream");
	}

	const uint64		NumChunks = DivUp(FileSize, MaxChunkSize);
	std::vector<FRange> Chunks;
	Chunks.reserve(NumChunks);

	for (uint64 i = 0; i < NumChunks; ++i)
	{
		FRange Range;
		Range.Offset = i * MaxChunkSize;
		Range.Size	 = CalcChunkSize(i, MaxChunkSize, FileSize);
		Chunks.push_back(Range);
	}

	FAtomicError Error;
	FSemaphore	 DownloadSempahore(4);	// up to 4 concurrent connections

	FLogProgressScope DownloadProgress(FileSize, ELogProgressUnits::MB);

	std::string BearerToken;
	if (AuthDesc)
	{
		TResult<FAuthToken> AuthToken = Authenticate(*AuthDesc, 5 * 60);
		if (!AuthToken.IsOk())
		{
			Error.Set(std::move(AuthToken.GetError()));
			return AppError(L"Failed to acquire access token");
		}
		BearerToken = std::move(AuthToken->Access);
	}

	auto ProcessChunk =
		[&Error, &Result, &Url, &Remote, &ConnectionPool, &DownloadSempahore, &DownloadProgress, &BearerToken](
			const FRange& Range)
	{
		FLogIndentScope	   IndentScope(DownloadProgress.ParentThreadIndent, true);
		FLogVerbosityScope VerbosityScope(DownloadProgress.bParentThreadVerbose);

		if (Error.Test())
		{
			return;
		}

		DownloadSempahore.Acquire();

		std::unique_ptr<FHttpConnection> Connection = ConnectionPool.Acquire();

		std::string RequestHeaders = fmt::format("Range: bytes={}-{}", Range.Offset, Range.Offset + Range.Size - 1);

		FHttpRequest Request;
		Request.Url			   = Url;
		Request.Method		   = EHttpMethod::GET;
		Request.BearerToken	   = BearerToken;
		Request.CustomHeaders  = RequestHeaders;
		FHttpResponse Response = HttpRequest(*Connection, Request);

		if (!Response.Success())
		{
			Error.Set(HttpError(Response.Code));
			return;
		}

		if (Range.Size != Response.Buffer.Size())
		{
			Error.Set(AppError(L"Downloaded file chunk size mismatch"));
		}

		uint64 WrittenBytes = Result.Write(Response.Buffer.Data(), Range.Offset, Range.Size);

		DownloadProgress.Add(WrittenBytes);

		ConnectionPool.Release(std::move(Connection));

		DownloadSempahore.Release();
	};

	ParallelForEach(Chunks, ProcessChunk);

	DownloadProgress.Complete();

	if (Error.Test())
	{
		return std::move(*Error.Data);
	}

	return ResultOk();
}

TResult<FBuffer>
ProxyQuery::DownloadFile(const FRemoteDesc& Remote, const FAuthDesc* AuthDesc, const std::string& Path)
{
	FBuffer Result;

	std::unique_ptr<FMemReaderWriter> ResultWriter;

	auto OutputCallback = [&Result, &ResultWriter](uint64 Size) -> FIOWriter&
	{
		Result.Resize(Size);
		ResultWriter = std::make_unique<FMemReaderWriter>(Result.Data(), Result.Size());
		return *ResultWriter;
	};

	TResult<> DownloadResult = DownloadFile(Remote, AuthDesc, Path, OutputCallback);
	if (DownloadResult.IsOk())
	{
		return ResultOk(std::move(Result));
	}
	else
	{
		return MoveError<FBuffer>(DownloadResult);
	}
}

void
FUnsyncProtocolImpl::SendTelemetryEvent(const FRemoteDesc& RemoteDesc, const FTelemetryEventSyncComplete& Event)
{
	using namespace json11;
	json11::Json::object Obj;

	if (!Event.Session.empty())
	{
		Obj["session"] = Event.Session;
	}

	Obj["type"]			  = "sync_complete";
	Obj["client_version"] = Event.ClientVersion;
	if (!Event.ClientHostNameHash.empty())
	{
		Obj["client_host_name_hash"] = Event.ClientHostNameHash;
	}
	Obj["source"]	 = Event.Source;
	Obj["total_mb"]	 = SizeMb(Event.TotalBytes);  // use size in megabytes due lack of JSON 64 bit int support and better human readability
	Obj["source_mb"] = SizeMb(Event.SourceBytes);
	Obj["base_mb"]	 = SizeMb(Event.BaseBytes);
	Obj["files_skipped"] = int(Event.SkippedFiles);
	Obj["files_full"]	 = int(Event.FullCopyFiles);
	Obj["files_partial"] = int(Event.PartialCopyFiles);
	Obj["elapsed"]		 = Event.Elapsed;
	Obj["success"]		 = Event.bSuccess;

	std::string EventJson = Json(Obj).dump();

	FBufferView EventJsonView;
	EventJsonView.Data = (const uint8*)EventJson.c_str();
	EventJsonView.Size = (uint64)EventJson.length();

	const char* Url = "/api/v1/telemetry";
	HttpRequest(RemoteDesc, EHttpMethod::POST, Url, EHttpContentType::Application_Json, EventJsonView);
}

FUnsyncProtocolImpl::~FUnsyncProtocolImpl()
{
	if (IsValid())
	{
		FCommandPacket Packet;
		Packet.CommandId = COMMAND_ID_DISCONNECT;
		SendStruct(*SocketHandle, Packet);
	}

	SocketHandle = {};

	bIsConnetedToHost = false;
}

void
FBlockRequestMap::AddFileBlocks(const FPath& OriginalFilePath, const FPath& ResolvedFilePath, const FFileManifest& FileManifest)
{
	UNSYNC_ASSERTF(StrongHasher != EStrongHashAlgorithmID::Invalid, L"Request map is not initialized");

	std::string OriginalFilePathUtf8 = ConvertWideToUtf8(OriginalFilePath.wstring());
	std::string ResolvedFilePathUtf8 = ConvertWideToUtf8(ResolvedFilePath.wstring());

	FHash128 OriginalNameHash = HashMd5Bytes((const uint8*)OriginalFilePathUtf8.c_str(), OriginalFilePathUtf8.length());
	FHash128 ResolvedNameHash = HashMd5Bytes((const uint8*)ResolvedFilePathUtf8.c_str(), ResolvedFilePathUtf8.length());

	auto FindResult = HashToFile.find(OriginalNameHash);
	if (FindResult == HashToFile.end())
	{
		HashToFile[OriginalNameHash] = uint32(FileListUtf8.size());
		HashToFile[ResolvedNameHash] = uint32(FileListUtf8.size());
		FileListUtf8.push_back(OriginalFilePathUtf8);
	}

	for (const FGenericBlock& Block : FileManifest.Blocks)
	{
		FBlockRequest Request;
		Request.FilenameMd5				 = OriginalNameHash;
		Request.BlockHash				 = Block.HashStrong.ToHash128();  // #wip-widehash
		Request.Offset					 = Block.Offset;
		Request.Size					 = Block.Size;
		BlockRequests[Request.BlockHash] = Request;

		if (!FileManifest.MacroBlocks.empty())
		{
			// TODO: could also just do the search in GetMacroBlockRequest() directly instead of pre-caching
			// TODO: since we know that blocks and macro-blocks are sorted, then we don't need a binary search here
			auto MacroBlockIt = std::lower_bound(FileManifest.MacroBlocks.begin(),
												 FileManifest.MacroBlocks.end(),
												 Block.Offset,
												 [](const FGenericBlock& A, uint64 B) { return (A.Offset + A.Size) < (B + 1); });

			if (MacroBlockIt != FileManifest.MacroBlocks.end())
			{
				const FGenericBlock& MacroBlock = *MacroBlockIt;
				UNSYNC_ASSERT(Block.Offset >= MacroBlock.Offset);
				UNSYNC_ASSERT(Block.Offset + Block.Size <= MacroBlock.Offset + MacroBlock.Size);

				FHash128 RequestKey = Block.HashStrong.ToHash128();

				if (MacroBlockRequests.find(RequestKey) == MacroBlockRequests.end())
				{
					FMacroBlockRequest MacroRequest;
					MacroRequest.Hash				  = MacroBlock.HashStrong;
					MacroRequest.Offset				  = Block.Offset - MacroBlock.Offset;
					MacroRequest.Size				  = Block.Size;
					MacroRequest.MacroBlockBaseOffset = MacroBlock.Offset;
					MacroRequest.MacroBlockTotalSize  = MacroBlock.Size;
					MacroBlockRequests[RequestKey]	  = MacroRequest;
				}
			}
			else
			{
				UNSYNC_FATAL(L"Found a block that does not belong to any macro block.");
			}
		}
	}
}

const FBlockRequest*
FBlockRequestMap::FindRequest(const FGenericHash& BlockHash) const
{
	FHash128 BlockHash128 = BlockHash.ToHash128();

	auto It = BlockRequests.find(BlockHash128);
	if (It == BlockRequests.end())
	{
		return nullptr;
	}
	else
	{
		return &It->second;
	}
}

const std::string*
FBlockRequestMap::FindFile(const FHash128& Hash) const
{
	auto It = HashToFile.find(Hash);
	if (It == HashToFile.end())
	{
		return nullptr;
	}
	else
	{
		return &FileListUtf8[It->second];
	}
}

FMacroBlockRequest
FBlockRequestMap::GetMacroBlockRequest(const FGenericHash& BlockHash) const
{
	FMacroBlockRequest Result = {};
	auto			   It	  = MacroBlockRequests.find(BlockHash.ToHash128());
	if (It != MacroBlockRequests.end())
	{
		Result = It->second;
	}
	return Result;
}

FProxyPool::FProxyPool() : FProxyPool(FRemoteDesc(), nullptr)
{
}

FProxyPool::FProxyPool(const FRemoteDesc& InRemoteDesc, const FAuthDesc* InAuthDesc)
: ParallelDownloadSemaphore(InRemoteDesc.MaxConnections)
, RemoteDesc(InRemoteDesc)
, AuthDesc(InAuthDesc)
, bValid(InRemoteDesc.IsValid())
{
	if (!bValid)
	{
		return;
	}

	if (RemoteDesc.Protocol == EProtocolFlavor::Unsync)
	{
		UNSYNC_VERBOSE(L"Connecting to %hs server '%hs:%d' ...",
					   ToString(RemoteDesc.Protocol),
					   RemoteDesc.Host.Address.c_str(),
					   RemoteDesc.Host.Port);

		TResult<ProxyQuery::FHelloResponse> Response = ProxyQuery::Hello(RemoteDesc, AuthDesc);

		if (Response.IsError())
		{
			LogError(Response.GetError());
		}
		else
		{
			const ProxyQuery::FHelloResponse& Data = Response.GetData();
			UNSYNC_VERBOSE(L"Connection established. Server name: %hs, version: %hs, git: %hs.",
						   Data.Name.empty() ? "unknown" : Data.Name.c_str(),
						   Data.VersionNumber.empty() ? "unknown" : Data.VersionNumber.c_str(),
						   Data.VersionGit.empty() ? "unknown" : Data.VersionGit.c_str());

			Features  = Data.Features;
			SessionId = Data.SessionId;
		}

		bValid = Response.IsOk();
	}
	else if (RemoteDesc.Protocol == EProtocolFlavor::Jupiter)
	{
		Features.bAuthentication = true;
		Features.bDownloadByHash = true;
	}
}

std::unique_ptr<FProxy>
FProxyPool::Alloc()
{
	if (!bValid)
	{
		return nullptr;
	}

	std::lock_guard<std::mutex> LockGuard(Mutex);
	std::unique_ptr<FProxy>		Result;
	if (!Pool.empty())
	{
		std::swap(Pool.back(), Result);
		Pool.pop_back();
	}

	if (!Result || !Result->IsValid())
	{
 		Result = std::make_unique<FProxy>(RemoteDesc, Features, AuthDesc, &RequestMap);
	}

	return Result;
}

void
FProxyPool::Dealloc(std::unique_ptr<FProxy>&& Proxy)
{
	if (Proxy.get() && Proxy->IsValid())
	{
		std::lock_guard<std::mutex> LockGuard(Mutex);
		Pool.push_back(std::move(Proxy));
	}
}

void
FProxyPool::Invalidate()
{
	bValid = false;
}

bool
FProxyPool::IsValid() const
{
	return bValid;
}

void
FProxyPool::BuildFileBlockRequests(const FPath& OriginalFilePath, const FPath& ResolvedFilePath, const FFileManifest& FileManifest)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	RequestMap.AddFileBlocks(OriginalFilePath, ResolvedFilePath, FileManifest);
}

void
FProxyPool::SendTelemetryEvent(const FTelemetryEventSyncComplete& Event)
{
	if (RemoteDesc.Protocol == EProtocolFlavor::Unsync && Features.bTelemetry)
	{
		FUnsyncProtocolImpl::SendTelemetryEvent(RemoteDesc, Event);
	}
}

void
FProxyPool::InitRequestMap(EStrongHashAlgorithmID InStrongHasher)
{
	std::lock_guard<std::mutex> LockGuard(Mutex);
	RequestMap.Init(InStrongHasher);
}

}  // namespace unsync
