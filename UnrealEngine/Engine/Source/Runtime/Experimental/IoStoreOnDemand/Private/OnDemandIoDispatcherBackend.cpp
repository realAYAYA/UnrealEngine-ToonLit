// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoDispatcherBackend.h"
#include "Statistics.h"

#include "CancellationToken.h"
#include "Containers/StringView.h"
#include "CoreHttp/Client.h"
#include "EncryptionKeyManager.h"
#include "FileIoCache.h"
#include "HAL/Event.h"
#include "HAL/Platform.h"
#include "HAL/PlatformTime.h"
#include "HAL/PreprocessorHelpers.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Http.h"
#include "HttpManager.h"
#include "IO/IoAllocators.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoDispatcher.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "IO/IoStore.h"
#include "IO/IoStoreOnDemand.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Modules/ModuleManager.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tasks/Task.h"

#include <atomic>

///////////////////////////////////////////////////////////////////////////////
int32 GIoDispatcherMaxHttpConnectionCount = 8;
static FAutoConsoleVariableRef CVar_IoDispatcherMaxHttpConnectionCount (
	TEXT("s.IasMaxHttpConnectionCount"),
	GIoDispatcherMaxHttpConnectionCount,
	TEXT("Max number of open HTTP connections to the on demand endpoint(s).")
);

int32 GIoDispatcherMaxHttpRetryCount = 2;
static FAutoConsoleVariableRef CVar_IoDispatcherMaxHttpRetryCount (
	TEXT("s.IasMaxHttpRetryCount"),
	GIoDispatcherMaxHttpRetryCount,
	TEXT("Max number of HTTP request retries before failing the I/O request.")
);

int32 GIoDispatcherHttpPollTimeoutMs = 0;
static FAutoConsoleVariableRef CVar_IoDispatcherMaxHttpPollTimeoutMs (
	TEXT("s.IasHttpPollTimeout"),
	GIoDispatcherHttpPollTimeoutMs,
	TEXT("Tick() poll timeout in milliseconds")
);

namespace UE::IO::Private
{

using namespace UE::Tasks;

///////////////////////////////////////////////////////////////////////////////
FIoHash GetChunkKey(const FIoHash& ChunkHash, const FIoOffsetAndLength& Range)
{
	FIoHashBuilder HashBuilder;
	HashBuilder.Update(ChunkHash.GetBytes(), sizeof(FIoHash::ByteArray));
	HashBuilder.Update(&Range, sizeof(FIoOffsetAndLength));

	return HashBuilder.Finalize();
}

///////////////////////////////////////////////////////////////////////////////
class FDistributionEndpoints
{
public:
	using FOnEndpointResolved = TFunction<void(const FString&, TConstArrayView<FString>)>;
	
	FDistributionEndpoints() = default;
	~FDistributionEndpoints();

	void Resolve(const FString& DistributionUrl, FOnEndpointResolved&& OnResolved);
	void ResolveDeferredEndpoints();

private:
	struct FResolvedEndpoint
	{
		TArray<FString> ServiceUrls;
	};

	struct FResolveRequest
	{
		FString DistributionUrl;
		FHttpRequestPtr HttpRequest;
		TArray<FOnEndpointResolved> Callbacks;
		int32 RetryCount = 0;
	};

	void IssueRequests();
	void CancelRequests();
	void CompleteRequest(FResolveRequest& ResolveRequest, FHttpResponsePtr HttpResponse);

	TMap<FString, TUniquePtr<FResolvedEndpoint>> ResolvedEndpoints;
	TMap<FString, TUniquePtr<FResolveRequest>> PendingRequests;
	FRWLock Lock;
	bool bInitialized = false;
};

FDistributionEndpoints::~FDistributionEndpoints()
{
	CancelRequests();
}

void FDistributionEndpoints::Resolve(const FString& DistributionUrl, FOnEndpointResolved&& OnResolved)
{
	const FResolvedEndpoint* Ep = nullptr;
	{
		FReadScopeLock _(Lock);
		if (TUniquePtr<FResolvedEndpoint>* Entry = ResolvedEndpoints.Find(DistributionUrl))
		{
			Ep = Entry->Get();
		}
	}

	if (Ep)
	{
		return OnResolved(DistributionUrl, Ep->ServiceUrls);
	}

	bool bIssueRequest = false;
	{
		FWriteScopeLock _(Lock);
		TUniquePtr<FResolveRequest>& Request = PendingRequests.FindOrAdd(DistributionUrl);
		if (!Request.IsValid())
		{
			Request.Reset(new FResolveRequest{DistributionUrl});
			bIssueRequest = bInitialized;
		}
		Request->Callbacks.Add(MoveTemp(OnResolved));
	}

	if (bIssueRequest)
	{
		IssueRequests();
	}
}

void FDistributionEndpoints::ResolveDeferredEndpoints()
{
	{
		FWriteScopeLock _(Lock);
		bInitialized = true;
	}

	IssueRequests();
}

void FDistributionEndpoints::IssueRequests()
{
	// Currenlty we need to use the HTTP module in order to resolve service endpoints due to HTTPS
	FHttpModule& HttpModule = FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");
	const int32 MaxAttempts = GIoDispatcherMaxHttpRetryCount;

	TArray<FHttpRequestPtr, TInlineAllocator<2>> HttpRequests;
	{
		FWriteScopeLock _(Lock);
		check(bInitialized);

		for (auto& Kv : PendingRequests)
		{
			if (Kv.Value->HttpRequest.IsValid())
			{
				continue;
			}

			FResolveRequest& ResolveRequest = *Kv.Value.Get();
			UE_LOG(LogIas, Log, TEXT("Resolving '%s' (#%d/%d)"), *ResolveRequest.DistributionUrl, ResolveRequest.RetryCount + 1, MaxAttempts);

			FHttpRequestPtr HttpRequest = HttpModule.Get().CreateRequest();
			HttpRequest->SetTimeout(3.0f);
			HttpRequest->SetURL(Kv.Key);
			HttpRequest->SetVerb(TEXT("GET"));
			HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
			HttpRequest->OnProcessRequestComplete().BindLambda(
				[this, &ResolveRequest, MaxAttempts]
				(FHttpRequestPtr, FHttpResponsePtr Response, bool bOk)
				{
					FHttpRequestPtr Request = MoveTemp(ResolveRequest.HttpRequest);
					if (Response->GetResponseCode() != 200)
					{
						if (++ResolveRequest.RetryCount <= MaxAttempts)
						{
							Request->OnProcessRequestComplete().Unbind();
							return IssueRequests();
						}
					}

					CompleteRequest(ResolveRequest, Response);
				});

			ResolveRequest.HttpRequest = HttpRequest;
			HttpRequests.Add(HttpRequest);
		}
	}

	for (FHttpRequestPtr& Request : HttpRequests)
	{
		Request->ProcessRequest();
	}
}

void FDistributionEndpoints::CancelRequests()
{
	TArray<FHttpRequestPtr, TInlineAllocator<2>> HttpRequests;
	{
		FWriteScopeLock _(Lock);
		for (auto& Kv : PendingRequests)
		{
			if (Kv.Value->HttpRequest.IsValid())
			{
				HttpRequests.Add(Kv.Value->HttpRequest);
			}
		}
	}

	if (!HttpRequests.IsEmpty())
	{
		FHttpModule& HttpModule = FModuleManager::LoadModuleChecked<FHttpModule>("HTTP");
		for (FHttpRequestPtr& Request : HttpRequests)
		{
			HttpModule.GetHttpManager().RemoveRequest(Request.ToSharedRef());
			//TODO: Flush?
		}
	}
}

void FDistributionEndpoints::CompleteRequest(FResolveRequest& ResolveRequest, FHttpResponsePtr HttpResponse)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::CompleteDistributionRequest);

	using FJsonValuePtr = TSharedPtr<FJsonValue>;
	using FJsonObjPtr = TSharedPtr<FJsonObject>;
	using FJsonReader = TJsonReader<TCHAR>;
	using FJsonReaderPtr = TSharedRef<FJsonReader>;

	TArray<FString> ServiceUrls;
	if (HttpResponse->GetResponseCode() == 200)
	{
		FString Json = HttpResponse->GetContentAsString();
		FJsonReaderPtr JsonReader = TJsonReaderFactory<TCHAR>::Create(Json);

		FJsonObjPtr JsonObj;
		if (FJsonSerializer::Deserialize(JsonReader, JsonObj))
		{
			TArray<FJsonValuePtr> JsonValues = JsonObj->GetArrayField(TEXT("distributions"));
			for (const FJsonValuePtr& JsonValue : JsonValues)
			{
				FString ServiceUrl = JsonValue->AsString();
				if (ServiceUrl.EndsWith(TEXT("/")))
				{
					ServiceUrl.LeftInline(ServiceUrl.Len() - 1);
				}
				ServiceUrls.Add(MoveTemp(ServiceUrl));
			}
		}
	}

	const FResolvedEndpoint* ResolvedEndpoint = nullptr;
	FString DistributionUrl;
	TArray<FOnEndpointResolved> Callbacks;

	{
		FWriteScopeLock _(Lock);
		if (!ServiceUrls.IsEmpty())
		{
			ResolvedEndpoint = ResolvedEndpoints.Emplace(
				ResolveRequest.DistributionUrl,
				new FResolvedEndpoint{MoveTemp(ServiceUrls)})
			.Get();
		}

		Callbacks = MoveTemp(ResolveRequest.Callbacks);
		DistributionUrl = MoveTemp(ResolveRequest.DistributionUrl);
		PendingRequests.Remove(DistributionUrl);
	}

	TConstArrayView<FString> Urls = ResolvedEndpoint ? ResolvedEndpoint->ServiceUrls : TConstArrayView<FString>();
	for (FOnEndpointResolved& Callback : Callbacks)
	{
		Callback(DistributionUrl, Urls);
	}
}

///////////////////////////////////////////////////////////////////////////////
class FHttpClient
{
public:
	FHttpClient(const FString& ServiceUrl, int32 MaxConnectionCount = 8);
	const FString& ServiceUrl() const { return SvcsUrl; }
	int32 MaxConnectionCount() const { return MaxConnections;}
	void Get(FAnsiStringView Url, FIoReadCallback&& Callback);
	void Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FIoReadCallback&& Callback);
	bool Tick();

private:
	void Issue(UE::HTTP::FRequest&& Request, FIoReadCallback&& Callback, FAnsiStringView Url = FAnsiStringView());

	FString SvcsUrl;
	int32 MaxConnections;
	HTTP::FEventLoop EventLoop;
	TUniquePtr<HTTP::FConnectionPool> ConnectionPool;
};

FHttpClient::FHttpClient(const FString& ServiceUrl, int32 MaxConnectionCount)
	: SvcsUrl(ServiceUrl)
	, MaxConnections(MaxConnectionCount)
{
	HTTP::FConnectionPool::FParams Params;
	Params.SetHostFromUrl(StringCast<ANSICHAR>(*ServiceUrl).Get());
	Params.ConnectionCount = MaxConnectionCount;
	ConnectionPool = MakeUnique<HTTP::FConnectionPool>(Params);
}

void FHttpClient::Get(FAnsiStringView Url, const FIoOffsetAndLength& Range, FIoReadCallback&& Callback)
{
	const uint64 RangeStart = Range.GetOffset();
	const uint64 RangeEnd = Range.GetOffset() + Range.GetLength();
	UE::HTTP::FRequest Request = EventLoop.Get(Url, *ConnectionPool);
	Request.Header(ANSITEXTVIEW("Range"), WriteToAnsiString<64>(ANSITEXTVIEW("bytes="), RangeStart, ANSITEXTVIEW("-"), RangeEnd));
	
	Issue(MoveTemp(Request), MoveTemp(Callback), Url);
}

void FHttpClient::Get(FAnsiStringView Url, FIoReadCallback&& Callback)
{
	Issue(EventLoop.Get(Url, *ConnectionPool), MoveTemp(Callback), Url);
}

void FHttpClient::Issue(UE::HTTP::FRequest&& Request, FIoReadCallback&& Callback, FAnsiStringView DebugUrl)
{
	using namespace UE::HTTP;
	
	auto Sink = [
		Buffer = FIoBuffer(),
		Callback = MoveTemp(Callback),
		Url = FString(DebugUrl),
		StartTime = FPlatformTime::Cycles64(),
		StatusCode = uint32(0)]
		(const FTicketStatus& Status) mutable
		{ 
			if (FTicketStatus::EId::Response == Status.GetId())
			{
				FResponse& Response = Status.GetResponse();
				StatusCode = Response.GetStatusCode();
				Response.SetDestination(&Buffer);
			}
			else if (FTicketStatus::EId::Content == Status.GetId())
			{
				const uint64 Duration = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);

				const bool bSuccessful = StatusCode > 199 && StatusCode < 300;
				if (const FIoBuffer& Content = Status.GetContent(); bSuccessful && Content.GetSize() > 0)
				{
					UE_LOG(LogIas, VeryVerbose, TEXT("%s"),
						*WriteToString<256>(TEXT("HTTP GET - "), Url, TEXT(" ("), StatusCode, TEXT(" "), Duration, TEXT("ms "), Content.GetSize(), TEXT(" Bytes)")));

					Callback(Content);
				}
				else
				{
					UE_LOG(LogIas, VeryVerbose, TEXT("%s"),
						*WriteToString<256>(TEXT("HTTP GET - "), Url, TEXT(" ("), StatusCode, TEXT(" "), Duration, TEXT("ms)")));
					
					Callback(FIoStatus(EIoErrorCode::NotFound, TEXTVIEW("Invalid Content")));
				}
			}
			else if (FTicketStatus::EId::Error == Status.GetId())
			{
				const uint64 Duration = (uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartTime);
				UE_LOG(LogIas, VeryVerbose, TEXT("%s"),
					*WriteToString<256>(TEXT("HTTP GET - "), Url, TEXT(" ("), StatusCode, TEXT(" "), Duration, TEXTVIEW("ms) : "), Status.GetErrorReason()));

				Callback(FIoStatus(EIoErrorCode::ReadError, TEXTVIEW("HTTP Error")));
			}
		};

	EventLoop.Send(MoveTemp(Request), MoveTemp(Sink));
}

bool FHttpClient::Tick()
{
	return EventLoop.Tick(GIoDispatcherHttpPollTimeoutMs) != 0;
}

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoStore
{
public:
	struct FTocEntry
	{
		FIoHash Hash;
		uint64 RawSize = 0;
		uint64 EncodedSize = 0;
		uint32 BlockOffset = ~uint32(0);
		uint32 BlockCount = 0; 
	};

	struct FToc;

	struct FContainer
	{
		const FToc* Toc = nullptr;
		FAES::FAESKey EncryptionKey;
		FString Name;
		FString EncryptionKeyGuid;
		FString ChunksDirectory;
		FName CompressionFormat;
		uint32 BlockSize = 0;

		TMap<FIoChunkId, FTocEntry> TocEntries;
		TArray<uint32> BlockSizes;
	};

	struct FToc
	{
		FOnDemandEndpoint Endpoint;
		TArray<FContainer> Containers;
	};

	using FTocArray = TChunkedArray<FToc, sizeof(FToc) * 4>;

	struct FChunkInfo
	{
		const FContainer* Container = nullptr;
		const FTocEntry* Entry = nullptr;

		bool IsValid() const { return Container && Entry; }
		operator bool() const { return IsValid(); }

		TConstArrayView<uint32> GetBlocks() const
		{
			check(Container != nullptr && Entry != nullptr);
			return TConstArrayView<uint32>(Container->BlockSizes.GetData() + Entry->BlockOffset, Entry->BlockCount);
		}
	};

	FOnDemandIoStore();
	~FOnDemandIoStore();

	void AddToc(const FOnDemandEndpoint& Ep, FOnDemandToc&& Toc);
	TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId);
	FChunkInfo GetChunkInfo(const FIoChunkId& ChunkId);

private:
	void AddDeferredContainers();
	void OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key);

	FTocArray Tocs;
	TArray<FContainer*> RegisteredContainers;
	TArray<FContainer*> DeferredContainers;
	mutable FRWLock Lock;
};

FOnDemandIoStore::FOnDemandIoStore()
{
	FEncryptionKeyManager::Get().OnKeyAdded().AddRaw(this, &FOnDemandIoStore::OnEncryptionKeyAdded);
}

FOnDemandIoStore::~FOnDemandIoStore()
{
	FEncryptionKeyManager::Get().OnKeyAdded().RemoveAll(this);
}

void FOnDemandIoStore::AddToc(const FOnDemandEndpoint& Ep, FOnDemandToc&& Toc)
{
	check(Ep.IsValid());
	UE_LOG(LogIas, Log, TEXT("Adding TOC '%s/%s'"), *Ep.ServiceUrl, *Ep.TocPath);

	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::AddToc);

	FString Prefix;
	{
		int32 Idx = INDEX_NONE;
		if (Ep.TocPath.FindLastChar(TCHAR('/'), Idx))
		{
			Prefix = Ep.TocPath.Left(Idx);
		}
	}

	{
		FWriteScopeLock _(Lock);

		const FOnDemandTocHeader& Header = Toc.Header;
		FToc* NewToc = new(Tocs) FToc{Ep};
		NewToc->Containers.SetNum(Toc.Containers.Num()); // List of containers can never change

		int32 ContainerIndex = 0;
		for (FOnDemandTocContainerEntry& Container : Toc.Containers)
		{
			FContainer* NewContainer = &NewToc->Containers[ContainerIndex++];
			NewContainer->Toc = NewToc;
			NewContainer->Name = Container.ContainerName;
			NewContainer->ChunksDirectory = (Prefix.IsEmpty() ? Header.ChunksDirectory : Prefix / Header.ChunksDirectory).ToLower();
			NewContainer->CompressionFormat = FName(Header.CompressionFormat);
			NewContainer->BlockSize = Header.BlockSize;
			NewContainer->EncryptionKeyGuid = Container.EncryptionKeyGuid;
			
			NewContainer->TocEntries.Reserve(Container.Entries.Num());
			for (const FOnDemandTocEntry& TocEntry : Container.Entries)
			{
				NewContainer->TocEntries.Add(TocEntry.ChunkId, FTocEntry
				{
					TocEntry.Hash,
					TocEntry.RawSize,
					TocEntry.EncodedSize,
					TocEntry.BlockOffset,
					TocEntry.BlockCount
				});
			}

			NewContainer->BlockSizes = MoveTemp(Container.BlockSizes);

			DeferredContainers.Add(NewContainer);
		}
	}

	AddDeferredContainers();
}

TIoStatusOr<uint64> FOnDemandIoStore::GetChunkSize(const FIoChunkId& ChunkId)
{
	if (FChunkInfo Info = GetChunkInfo(ChunkId))
	{
		return Info.Entry->RawSize;
	}

	return FIoStatus(EIoErrorCode::UnknownChunkID);
}

FOnDemandIoStore::FChunkInfo FOnDemandIoStore::GetChunkInfo(const FIoChunkId& ChunkId)
{
	FReadScopeLock _(Lock);

	for (const FContainer* Container : RegisteredContainers)
	{
		if (const FTocEntry* Entry = Container->TocEntries.Find(ChunkId))
		{
			return FChunkInfo{Container, Entry};
		}
	}

	return {};
}

void FOnDemandIoStore::AddDeferredContainers()
{
	FWriteScopeLock _(Lock);

	for (auto It = DeferredContainers.CreateIterator(); It; ++It)
	{
		FContainer* Container = *It;
		if (Container->EncryptionKeyGuid.IsEmpty())
		{
			UE_LOG(LogIas, Log, TEXT("Mounting container '%s'"), *Container->Name);
			RegisteredContainers.Add(Container);
			It.RemoveCurrent();
		}
		else
		{
			FGuid KeyGuid;
			ensure(FGuid::Parse(Container->EncryptionKeyGuid, KeyGuid));
			if (const FAES::FAESKey* Key = FEncryptionKeyManager::Get().GetKey(KeyGuid))
			{
				UE_LOG(LogIas, Log, TEXT("Mounting container '%s'"), *Container->Name);
				Container->EncryptionKey = *Key;
				RegisteredContainers.Add(Container);
				It.RemoveCurrent();
			}
			else
			{
				UE_LOG(LogIas, Log, TEXT("Defeering container '%s', encryption key '%s' not available"), *Container->Name, *Container->EncryptionKeyGuid);
			}
		}
	}
}

void FOnDemandIoStore::OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key)
{
	AddDeferredContainers();
}

///////////////////////////////////////////////////////////////////////////////
template<typename T>
class TThreadSafeIntrusiveQueue
{
public:
	void Enqueue(T* Request)
	{
		check(Request->NextRequest == nullptr);
		FScopeLock _(&CriticalSection);

		if (Tail)
		{
			Tail->NextRequest = Request;
		}
		else
		{
			check(Head == nullptr);
			Head = Request;	
		}

		Tail = Request;
	}

	T* Dequeue()
	{
		FScopeLock _(&CriticalSection);

		T* Requests = Head;
		Head = Tail = nullptr;

		return Requests;
	}

private:
	FCriticalSection CriticalSection;
	T* Head = nullptr;
	T* Tail = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
struct FChunkRequestParams
{
	static FChunkRequestParams Create(FIoRequestImpl* Request, FOnDemandIoStore::FChunkInfo ChunkInfo)
	{
		check(Request);
		check(Request->NextRequest == nullptr);
		const uint64 RawSize = FMath::Min(Request->Options.GetSize(), ChunkInfo.Entry->RawSize);
		
		const FIoOffsetAndLength ChunkRange = FIoChunkEncoding::GetChunkRange(
			ChunkInfo.Entry->RawSize,
			ChunkInfo.Container->BlockSize,
			ChunkInfo.GetBlocks(),
			Request->Options.GetOffset(),
			RawSize).ConsumeValueOrDie();

		return FChunkRequestParams{GetChunkKey(ChunkInfo.Entry->Hash, ChunkRange), ChunkRange, ChunkInfo};
	}

	void GetUrl(FAnsiStringBuilderBase& Url) const
	{
		const FString HashString = LexToString(ChunkInfo.Entry->Hash);
		Url << "/" << ChunkInfo.Container->ChunksDirectory
			<< "/" << HashString.Left(2)
			<< "/" << HashString << ANSITEXTVIEW(".iochunk");
	}

	FIoChunkDecodingParams GetDecodingParams() const
	{
		FIoChunkDecodingParams Params;
		Params.EncryptionKey = MakeMemoryView(ChunkInfo.Container->EncryptionKey.Key, FAES::FAESKey::KeySize);
		Params.CompressionFormat = ChunkInfo.Container->CompressionFormat;
		Params.BlockSize = ChunkInfo.Container->BlockSize;
		Params.TotalRawSize = ChunkInfo.Entry->RawSize;
		Params.EncodedBlockSize = ChunkInfo.GetBlocks(); 
		Params.EncodedOffset = ChunkRange.GetOffset();

		return Params;
	}

	FIoHash ChunkKey;
	FIoOffsetAndLength ChunkRange;
	FOnDemandIoStore::FChunkInfo ChunkInfo;
};

///////////////////////////////////////////////////////////////////////////////
struct FChunkRequest
{
	explicit FChunkRequest(FIoRequestImpl* Request, const FChunkRequestParams& RequestParams)
		: NextRequest()
		, Params(RequestParams)
		, RequestHead(Request)
		, RequestTail(Request)
		, StartTime(FPlatformTime::Cycles64())
		, RequestCount(1)
		, HttpRetryCount(0)
		, bCached(false)
	{
		check(Request && NextRequest == nullptr);
	}

	void AddDispatcherRequest(FIoRequestImpl* Request)
	{
		check(RequestHead && RequestTail);
		check(Request && !Request->NextRequest);
		RequestTail->NextRequest = Request;
		RequestTail = Request;
		RequestCount++;
	}

	uint32 RemoveDispatcherRequest(FIoRequestImpl* Request)
	{
		check(Request != nullptr);
		check(RequestCount > 0);

		if (RequestHead == Request)
		{
			RequestHead = Request->NextRequest; 
			if (RequestTail == Request)
			{
				check(RequestHead == nullptr);
				RequestTail = nullptr;
			}
		}
		else
		{
			FIoRequestImpl* It = RequestHead;
			while (It->NextRequest != Request)
			{
				It = It->NextRequest;
			}
			check(It->NextRequest == Request);
			It->NextRequest = It->NextRequest->NextRequest;
		}

		Request->NextRequest = nullptr;
		RequestCount--;

		return RequestCount;
	}

	FIoRequestImpl* DeqeueDispatcherRequests()
	{
		FIoRequestImpl* Head = RequestHead;
		RequestHead = RequestTail = nullptr;
		RequestCount = 0;

		return Head;
	}

	double DurationInSeconds() const
	{
		return FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - StartTime);
	}

	FChunkRequest* NextRequest;
	FChunkRequestParams Params;
	FIoRequestImpl* RequestHead;
	FIoRequestImpl* RequestTail;
	FIoBuffer Chunk;
	TTask<TIoStatusOr<FIoBuffer>> CacheTask;
	FTask DecodeTask;
	FIoCancellationToken CancellationToken;
	uint64 StartTime;
	uint16 RequestCount;
	uint16 HttpRetryCount;
	bool bCached;
};

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoBackend final
	: public FRunnable
	, public UE::IOnDemandIoDispatcherBackend
{
	using FIoRequestQueue = TThreadSafeIntrusiveQueue<FIoRequestImpl>;
	using FChunkRequestQueue = TThreadSafeIntrusiveQueue<FChunkRequest>;

	struct FBackendData
	{
		static void Attach(FIoRequestImpl* Request, const FIoHash& ChunkKey)
		{
			check(Request->BackendData == nullptr);
			Request->BackendData = new FBackendData{ChunkKey};
		}

		static TUniquePtr<FBackendData> Detach(FIoRequestImpl* Request)
		{
			check(Request->BackendData != nullptr);
			void* BackendData = Request->BackendData;
			Request->BackendData = nullptr;
			return TUniquePtr<FBackendData>(static_cast<FBackendData*>(BackendData));
		}
		
		static FBackendData& Get(FIoRequestImpl* Request)
		{
			check(Request->BackendData != nullptr);
			return *static_cast<FBackendData*>(Request->BackendData);
		}

		FIoHash ChunkKey;
	};

	struct FChunkRequests
	{
		FChunkRequest* Create(FIoRequestImpl* Request, const FChunkRequestParams& Params)
		{
			FScopeLock _(&Mutex);
			
			FBackendData::Attach(Request, Params.ChunkKey);

			if (FChunkRequest** InflightRequest = Inflight.Find(Params.ChunkKey))
			{
				FChunkRequest& ChunkRequest = **InflightRequest;
				check(!ChunkRequest.CancellationToken.IsCancelled());
				ChunkRequest.AddDispatcherRequest(Request);

				return nullptr;
			}

			FChunkRequest* ChunkRequest = Allocator.Construct(Request, Params);
			Inflight.Add(Params.ChunkKey, ChunkRequest);

			return ChunkRequest;
		}

		bool Cancel(FIoRequestImpl* Request)
		{
			FScopeLock _(&Mutex);

			FBackendData& BackendData = FBackendData::Get(Request);
			UE_LOG(LogIas, VeryVerbose, TEXT("%s"),
				*WriteToString<256>(TEXT("Cancelling I/O request ChunkId='"), LexToString(Request->ChunkId), TEXT("' ChunkKey='"), BackendData.ChunkKey, TEXT("'")));

			if (FChunkRequest** InflightRequest = Inflight.Find(BackendData.ChunkKey))
			{
				FChunkRequest& ChunkRequest = **InflightRequest;
				const uint32 RemainingCount = ChunkRequest.RemoveDispatcherRequest(Request);
				check(Request->NextRequest == nullptr);

				if (RemainingCount == 0)
				{
					ChunkRequest.CancellationToken.Cancel();
					Inflight.Remove(BackendData.ChunkKey);
				}

				return true;
			}

			return false;
		}

		void Remove(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Inflight.Remove(Request->Params.ChunkKey);
		}

		void Release(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			check(!Inflight.Contains(Request->Params.ChunkKey));
			Allocator.Destroy(Request);
		}
		
		void RemoveAndRelease(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Inflight.Remove(Request->Params.ChunkKey);
			Allocator.Destroy(Request);
		}

		TSingleThreadedSlabAllocator<FChunkRequest, 128> Allocator;
		TMap<FIoHash, FChunkRequest*> Inflight;
		FCriticalSection Mutex;
	};
public:

	FOnDemandIoBackend(TSharedPtr<IIoCache> Cache);
	virtual ~FOnDemandIoBackend();

	// I/O dispatcher backend
	virtual void Initialize(TSharedRef<const FIoDispatcherBackendContext> Context) override;
	virtual void Shutdown() override;
	virtual bool Resolve(FIoRequestImpl* Request) override;
	virtual void CancelIoRequest(FIoRequestImpl* Request) override;
	virtual void UpdatePriorityForIoRequest(FIoRequestImpl* Request) override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	virtual FIoRequestImpl* GetCompletedRequests() override;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;

	// I/O Http backend
	virtual void Mount(const FOnDemandEndpoint& Endpoint) override;

	// Runnable
	virtual bool Init() override { return true; }
	virtual void Stop() override { bStopRequested = true; }
	virtual uint32 Run() override;

private:
	void CompleteRequest(FChunkRequest* ChunkRequest);
	FIoStatus MountDeferredEndpoints(const FString& DistributionUrl, const TConstArrayView<FString>& ServiceUrls);
	static TIoStatusOr<FOnDemandToc> GetToc(FHttpClient& HttpClient, const FString& TocPath);
	FIoStatus AddToc(const FOnDemandEndpoint& Endpoint);

	TSharedPtr<IIoCache> Cache;
	TUniquePtr<FOnDemandIoStore> IoStore;
	TSharedPtr<const FIoDispatcherBackendContext> BackendContext;
	TUniquePtr<FRunnableThread> BackendThread;
	FEventRef TickBackendEvent;
	FDistributionEndpoints DistributionEndpoints;
	TArray<FOnDemandEndpoint> DeferredEndpoints;
	FChunkRequests ChunkRequests;
	FIoRequestQueue CompletedRequests;
	FChunkRequestQueue HttpRequests;
	TUniquePtr<FHttpClient> HttpClient;
	FOnDemandIoBackendStats Stats;
	FRWLock Lock;
	std::atomic_bool bStopRequested{false};
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandIoBackend::FOnDemandIoBackend(TSharedPtr<IIoCache> InCache)
	: Cache(InCache)
{
	IoStore = MakeUnique<FOnDemandIoStore>();
}

FOnDemandIoBackend::~FOnDemandIoBackend()
{
	Shutdown();
}

void FOnDemandIoBackend::Initialize(TSharedRef<const FIoDispatcherBackendContext> Context)
{
	UE_LOG(LogIas, Log, TEXT("Initializing on demand I/O dispatcher backend"));
	BackendContext = Context;
	DistributionEndpoints.ResolveDeferredEndpoints();
}

void FOnDemandIoBackend::Shutdown()
{
	if (bStopRequested)
	{
		return;
	}

	UE_LOG(LogIas, Log, TEXT("Shutting down on demand I/O dispatcher backend"));

	bStopRequested = true;
	TickBackendEvent->Trigger();
	BackendThread.Reset();
	BackendContext.Reset();
}

void FOnDemandIoBackend::CompleteRequest(FChunkRequest* ChunkRequest)
{
	check(ChunkRequest != nullptr);
	const bool bCancelled = ChunkRequest->CancellationToken.IsCancelled();

	if (bCancelled)
	{
		check(ChunkRequest->RequestHead == nullptr);
		check(ChunkRequest->RequestTail == nullptr);
		return ChunkRequests.RemoveAndRelease(ChunkRequest);
	}

	ChunkRequests.Remove(ChunkRequest);
	
	FIoBuffer Chunk = MoveTemp(ChunkRequest->Chunk);
	FIoChunkDecodingParams DecodingParams = ChunkRequest->Params.GetDecodingParams();

	bool bCanCache = Cache.IsValid();
	FIoRequestImpl* NextRequest = ChunkRequest->DeqeueDispatcherRequests();
	while (NextRequest)
	{
		FIoRequestImpl* Request = NextRequest;
		NextRequest = Request->NextRequest;
		Request->NextRequest = nullptr;

		bool bDecoded = false;
		if (Chunk.GetSize() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::DecodeBlocks);
			const uint64 RawSize = FMath::Min(Request->Options.GetSize(), ChunkRequest->Params.ChunkInfo.Entry->RawSize);
			Request->CreateBuffer(RawSize);
			DecodingParams.RawOffset = Request->Options.GetOffset(); 
			bDecoded = FIoChunkEncoding::Decode(DecodingParams, Chunk.GetView(), Request->GetBuffer().GetMutableView());
		}
		
		if (!bDecoded)
		{
			Stats.OnIoRequestFail();
			bCanCache = false;
			Request->SetFailed();
		}

		Stats.OnIoRequestComplete(Request->GetBuffer().GetSize());
		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}

	if (bCanCache && !ChunkRequest->bCached && Chunk.GetSize() > 0)
	{
		Cache->Put(ChunkRequest->Params.ChunkKey, Chunk);
	}

	Stats.OnChunkRequestRelease();
	ChunkRequests.Release(ChunkRequest);
}

bool FOnDemandIoBackend::Resolve(FIoRequestImpl* Request)
{
	using namespace UE::Tasks;
	
	FOnDemandIoStore::FChunkInfo ChunkInfo = IoStore->GetChunkInfo(Request->ChunkId);
	if (!ChunkInfo.IsValid())
	{
		return false;
	}

	Stats.OnIoRequestEnqueue();
	FChunkRequestParams RequestParams = FChunkRequestParams::Create(Request, ChunkInfo);
	FChunkRequest* ChunkRequest = ChunkRequests.Create(Request, RequestParams);

	if (ChunkRequest == nullptr)
	{
		// The chunk for the request is already inflight 
		return true;
	}

	Stats.OnChunkRequestCreate();
		
	auto CompleteOrEnqueueHttpRequest = [this, ChunkRequest]()
	{
		if (ChunkRequest->CacheTask.IsValid())
		{
			if (TIoStatusOr<FIoBuffer> Status = ChunkRequest->CacheTask.GetResult(); Status.IsOk())
			{
				ChunkRequest->Chunk = Status.ConsumeValueOrDie();
				ChunkRequest->bCached = true;
				return CompleteRequest(ChunkRequest);
			}
		}

		if (ChunkRequest->CancellationToken.IsCancelled())
		{
			return CompleteRequest(ChunkRequest);
		}

		Stats.OnHttpRequestEnqueue();
		HttpRequests.Enqueue(ChunkRequest);
		TickBackendEvent->Trigger();
	};

	//TODO: Remove if else once 25124120 is merged to FN/Main
	if (Cache.IsValid())
	{
		ChunkRequest->CacheTask = Cache->Get(ChunkRequest->Params.ChunkKey, FIoReadOptions(), &ChunkRequest->CancellationToken);
		Launch(UE_SOURCE_LOCATION, MoveTemp(CompleteOrEnqueueHttpRequest), ChunkRequest->CacheTask);
	}
	else
	{
		Launch(UE_SOURCE_LOCATION, MoveTemp(CompleteOrEnqueueHttpRequest));
	}

	return true;
}

void FOnDemandIoBackend::CancelIoRequest(FIoRequestImpl* Request)
{
	if (ChunkRequests.Cancel(Request))
	{
		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}
}

void FOnDemandIoBackend::UpdatePriorityForIoRequest(FIoRequestImpl* Request)
{
	//TODO: Implement
}

bool FOnDemandIoBackend::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId);
	return ChunkSize.IsOk();
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	if (IoStore.IsValid())
	{
		return IoStore->GetChunkSize(ChunkId);
	}

	return FIoStatus(EIoErrorCode::UnknownChunkID);
}

FIoRequestImpl* FOnDemandIoBackend::GetCompletedRequests()
{
	FIoRequestImpl* Requests = CompletedRequests.Dequeue();

	for (FIoRequestImpl* It = Requests; It != nullptr; It = It->NextRequest)
	{
		TUniquePtr<FBackendData> BackendData = FBackendData::Detach(It);
		check(It->BackendData == nullptr);
	}

	return Requests;
}

TIoStatusOr<FIoMappedRegion> FOnDemandIoBackend::OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
{
	return FIoStatus::Unknown;
}

FIoStatus FOnDemandIoBackend::MountDeferredEndpoints(const FString& DistributionUrl, const TConstArrayView<FString>& ServiceUrls)
{
	TArray<FOnDemandEndpoint, TInlineAllocator<4>> EndpointsToAdd;
	{
		FWriteScopeLock _(Lock);
		for (auto It = DeferredEndpoints.CreateIterator(); It; ++It)
		{
			if (It->DistributionUrl.Compare(DistributionUrl, ESearchCase::IgnoreCase) == 0)
			{
				EndpointsToAdd.Add(*It);
				It.RemoveCurrent();
			}
		}
	}

	for (const FOnDemandEndpoint& Ep : EndpointsToAdd)
	{
		FIoStatus Status;
		for (const FString& SerivceUrl : ServiceUrls)
		{
			// Currenlty we don't need use secure sockets to fetch on demand content
			FString UnsecureUrl = SerivceUrl.Replace(TEXT("https"), TEXT("http"));
			Status = AddToc(FOnDemandEndpoint{Ep.EndpointType, DistributionUrl, UnsecureUrl, Ep.TocPath});
			if (Status.IsOk())
			{
				break;
			}
		}

		if (!Status.IsOk())
		{
			return Status;
		}
	}

	return FIoStatus::Ok;
}

void FOnDemandIoBackend::Mount(const FOnDemandEndpoint& Endpoint)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::Mount);

	if ((Endpoint.DistributionUrl.IsEmpty() && Endpoint.ServiceUrl.IsEmpty()) || Endpoint.TocPath.IsEmpty())
	{
		UE_LOG(LogIas, Error, TEXT("Trying to mount an invalid on demand endpoint"));
		return;
	}

	if (EnumHasAnyFlags(Endpoint.EndpointType, EOnDemandEndpointType::CDN))
	{
		if (Endpoint.ServiceUrl.IsEmpty())
		{
			{
				FWriteScopeLock _(Lock);
				DeferredEndpoints.Add(Endpoint);
			}

			DistributionEndpoints.Resolve(Endpoint.DistributionUrl, [this](const FString& DistributionUrl, TConstArrayView<FString> SerivceUrls)
			{
				if (FIoStatus Status = MountDeferredEndpoints(DistributionUrl, SerivceUrls); !Status.IsOk())
				{
					UE_LOG(LogIas, Error, TEXT("Failed to add endpoint(s), reason '%s'"), *Status.ToString());
				}
			});
		}
		else if (FIoStatus Status = AddToc(Endpoint); !Status.IsOk())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to add TOC '%s/%s', reason '%s'"),
				*Endpoint.ServiceUrl, *Endpoint.TocPath, *Status.ToString());
		}
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Mounting ZEN endpoint, Url='%s'"), *Endpoint.ServiceUrl);
	}
}

TIoStatusOr<FOnDemandToc> FOnDemandIoBackend::GetToc(FHttpClient& HttpClient, const FString& TocPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::GetToc);

	TAnsiStringBuilder<256> Url;
	Url << "/" << TocPath;

	for (int32 Attempt = 0, MaxAttempts = GIoDispatcherMaxHttpRetryCount; Attempt <= MaxAttempts; ++Attempt)
	{
		UE_LOG(LogIas, Log, TEXT("Fetching TOC '%s/%s' (#%d/%d)"), *HttpClient.ServiceUrl(), *TocPath, Attempt + 1, MaxAttempts);
		
		TIoStatusOr<FOnDemandToc> Toc;
		HttpClient.Get(Url.ToView(), [&Toc](TIoStatusOr<FIoBuffer> Response)
		{
			if (Response.IsOk())
			{
				FIoBuffer Buffer = Response.ConsumeValueOrDie();
				FOnDemandToc NewToc;	
				if (LoadFromCompactBinary(FCbFieldView(Buffer.GetData()), NewToc))
				{
					Toc = TIoStatusOr<FOnDemandToc>(MoveTemp(NewToc));
				}
				else
				{
					UE_LOG(LogIas, Error, TEXT("Failed loading on demand TOC from compact binary"));
				}
			}
			else
			{
				UE_LOG(LogIas, Error, TEXT("Failed fetching TOC, reason '%s'"), *Response.Status().ToString());
			}
		});

		while (HttpClient.Tick());

		if (Toc.IsOk())
		{
			return Toc;
		}
	}

	return TIoStatusOr<FOnDemandToc>(FIoStatus(EIoErrorCode::NotFound));
}

FIoStatus FOnDemandIoBackend::AddToc(const FOnDemandEndpoint& Endpoint)
{
	TUniquePtr<FHttpClient> Client = MakeUnique<FHttpClient>(Endpoint.ServiceUrl, GIoDispatcherMaxHttpConnectionCount);

	TIoStatusOr<FOnDemandToc> Toc = GetToc(*Client, Endpoint.TocPath);
	if (!Toc.IsOk())
	{
		return FIoStatus(Toc.Status());
	}

	{
		FWriteScopeLock _(Lock);
		if (!HttpClient.IsValid())
		{
			HttpClient = MoveTemp(Client);
			BackendThread.Reset(FRunnableThread::Create(this, TEXT("IoStoreOnDemand"), 0, TPri_AboveNormal));
		}
	}

	IoStore->AddToc(Endpoint, Toc.ConsumeValueOrDie());

	UE_CLOG(Endpoint.ServiceUrl !=  HttpClient->ServiceUrl(),
		LogIas, Fatal, TEXT("Fetching on demand content from multiple endpoints are currently not supported"));

	return FIoStatus::Ok;
}

uint32 FOnDemandIoBackend::Run()
{
	const int32 MaxConcurrentRequests = HttpClient->MaxConnectionCount();

	while (!bStopRequested)
	{
		for (;;)
		{
			FChunkRequest* NextChunkRequest = HttpRequests.Dequeue();
			if (NextChunkRequest == nullptr)
			{
				break;
			}

			int32 NumConcurrentRequests = 0;
			while (NextChunkRequest)
			{
				FChunkRequest* ChunkRequest = NextChunkRequest;
				NextChunkRequest = ChunkRequest->NextRequest;
				ChunkRequest->NextRequest = nullptr;

				Stats.OnHttpRequestDequeue();

				TAnsiStringBuilder<256> Url;
				ChunkRequest->Params.GetUrl(Url);

				NumConcurrentRequests++;
				HttpClient->Get(Url.ToView(), ChunkRequest->Params.ChunkRange,
					[this, ChunkRequest, &NumConcurrentRequests](TIoStatusOr<FIoBuffer> Status)
					{
						NumConcurrentRequests--;

						if (Status.Status().GetErrorCode() == EIoErrorCode::ReadError)
						{
							if (++ChunkRequest->HttpRetryCount <= GIoDispatcherMaxHttpRetryCount)
							{
								Stats.OnHttpRequestFail();
								Stats.OnHttpRequestEnqueue();
								return HttpRequests.Enqueue(ChunkRequest); 
							}
						}

						if (Status.IsOk())
						{
							ChunkRequest->Chunk = Status.ConsumeValueOrDie();
							Stats.OnHttpRequestComplete(ChunkRequest->Chunk.DataSize());
						}
						else
						{
							Stats.OnHttpRequestFail();
						}

						Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
						{
							CompleteRequest(ChunkRequest);
						});
					});

				while (NumConcurrentRequests >= MaxConcurrentRequests)
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::TickHttpEventLoop);
					HttpClient->Tick();
				}
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FOnDemandIoBackend::TickHttpEventLoop);
				while (HttpClient->Tick());
			}
		}

		if (!bStopRequested)
		{
			TickBackendEvent->Wait();
		}
	}

	return 0;
}

} // namespace UE::IO::Private

namespace UE
{

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TSharedPtr<IIoCache> Cache)
{
	return MakeShareable<IOnDemandIoDispatcherBackend>(new UE::IO::Private::FOnDemandIoBackend(Cache));
}

} // namespace UE
