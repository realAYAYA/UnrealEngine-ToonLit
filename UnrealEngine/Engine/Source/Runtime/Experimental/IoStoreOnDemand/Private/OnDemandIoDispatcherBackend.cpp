// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandIoDispatcherBackend.h"

#include "Algo/Transform.h"
#include "AnalyticsEventAttribute.h"
#include "Containers/BitArray.h"
#include "Containers/StringView.h"
#include "CoreHttp/LatencyTesting.h"
#include "DistributionEndpoints.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "HAL/Event.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "HAL/PreprocessorHelpers.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HttpManager.h"
#include "IO/IoAllocators.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoDispatcher.h"
#include "IO/IoOffsetLength.h"
#include "IO/IoStatus.h"
#include "IO/IoStore.h"
#include "IO/IoStoreOnDemand.h"
#include "IasCache.h"
#include "Logging/StructuredLog.h"
#include "Math/NumericLimits.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegatesInternal.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/PathViews.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "OnDemandHttpClient.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/MemoryReader.h"
#include "Statistics.h"
#include "Tasks/Task.h"

#include <atomic>

#if !UE_BUILD_SHIPPING
#include "Modules/ModuleManager.h"
#endif 

/** When enabled the IAS system can add additional debug console commands for development use */
#define UE_IAS_DEBUG_CONSOLE_CMDS (1 && !NO_CVARS && !UE_BUILD_SHIPPING)

namespace UE::IO::IAS
{

///////////////////////////////////////////////////////////////////////////////

extern FString GIasOnDemandTocExt;

///////////////////////////////////////////////////////////////////////////////
int32 GIasHttpPrimaryEndpoint = 0;
static FAutoConsoleVariableRef CVar_IasHttpPrimaryEndpoint(
	TEXT("ias.HttpPrimaryEndpoint"),
	GIasHttpPrimaryEndpoint,
	TEXT("Primary endpoint to use returned from the distribution endpoint")
);

bool GIasHttpChangeEndpointAfterSuccessfulRetry = true;
static FAutoConsoleVariableRef CVar_IasHttpChangeEndpointAfterSuccessfulRetry(
	TEXT("ias.HttpChangeEndpointAfterSuccessfulRetry"),
	GIasHttpChangeEndpointAfterSuccessfulRetry,
	TEXT("Whether to change the current endpoint after a sucessful retry")
);

int32 GIasHttpPollTimeoutMs = 17;
static FAutoConsoleVariableRef CVar_GIasHttpPollTimeoutMs(
	TEXT("ias.HttpPollTimeoutMs"),
	GIasHttpPollTimeoutMs,
	TEXT("Http tick poll timeout in milliseconds")
);

int32 GIasHttpRateLimitKiBPerSecond = 0;
static FAutoConsoleVariableRef CVar_GIasHttpRateLimitKiBPerSecond(
	TEXT("ias.HttpRateLimitKiBPerSecond"),
	GIasHttpRateLimitKiBPerSecond,
	TEXT("Http throttle limit in KiBPerSecond")
);

static int32 GIasHttpRecvBufKiB = -1;
static FAutoConsoleVariableRef CVar_GIasHttpRecvBufKiB(
	TEXT("ias.HttpRecvBufKiB"),
	GIasHttpRecvBufKiB,
	TEXT("Recv buffer size")
);

static int32 GIasHttpConcurrentRequests = 8;
static FAutoConsoleVariableRef CVar_IasHttpConcurrentRequests(
	TEXT("ias.HttpConcurrentRequests"),
	GIasHttpConcurrentRequests,
	TEXT("Number of concurrent requests in the http client.")
);

static int32 GIasHttpConnectionCount = 4;
static FAutoConsoleVariableRef CVar_IasHttpConnectionCount(
	TEXT("ias.HttpConnectionCount"),
	GIasHttpConnectionCount,
	TEXT("Number of open HTTP connections to the on demand endpoint(s).")
);

static int32 GIasHttpPipelineLength = 2;
static FAutoConsoleVariableRef CVar_GIasHttpPipelineLength(
	TEXT("ias.HttpPipelineLength"),
	GIasHttpPipelineLength,
	TEXT("Number of concurrent requests on one connection")
);

/**
 *This is only applied when the connection was made to a single ServiceUrl rather than a DistributedUrl.
 * In the latter case we will make two attempts on the primary CDN followed by a single attempt for the
 * remaining CDN's to be tried in the order provided by the distributed endpoint.
 */
static int32 GIasHttpRetryCount = 2;
static FAutoConsoleVariableRef CVar_IasHttpRetryCount(
	TEXT("ias.HttpRetryCount"),
	GIasHttpRetryCount,
	TEXT("Number of HTTP request retries before failing the request (if connected to a service url rather than distributed endpoints).")
);

int32 GIasHttpTimeOutMs = 10 * 1000;
static FAutoConsoleVariableRef CVar_IasHttpTimeOutMs(
	TEXT("ias.HttpTimeOutMs"),
	GIasHttpTimeOutMs,
	TEXT("Time out value for HTTP requests in milliseconds")
);

int32 GIasHttpHealthCheckWaitTime = 3000;
static FAutoConsoleVariableRef CVar_IasHttpHealthCheckWaitTime(
	TEXT("ias.HttpHealthCheckWaitTime"),
	GIasHttpHealthCheckWaitTime,
	TEXT("Number of milliseconds to wait before reconnecting to avaiable endpoint(s)")
);

int32 GIasMaxEndpointTestCountAtStartup = 1;
static FAutoConsoleVariableRef CVar_IasMaxEndpointTestCountAtStartup(
	TEXT("ias.MaxEndpointTestCountAtStartup"),
	GIasMaxEndpointTestCountAtStartup,
	TEXT("Number of endpoint(s) to test at startup")
);

int32 GIasHttpErrorSampleCount = 8;
static FAutoConsoleVariableRef CVar_IasHttpErrorSampleCount(
	TEXT("ias.HttpErrorSampleCount"),
	GIasHttpErrorSampleCount,
	TEXT("Number of samples for computing the moving average of failed HTTP requests")
);

float GIasHttpErrorHighWater = 0.5f;
static FAutoConsoleVariableRef CVar_IasHttpErrorHighWater(
	TEXT("ias.HttpErrorHighWater"),
	GIasHttpErrorHighWater,
	TEXT("High water mark when HTTP streaming will be disabled")
);

bool GIasHttpEnabled = true;
static FAutoConsoleVariableRef CVar_IasHttpEnabled(
	TEXT("ias.HttpEnabled"),
	GIasHttpEnabled,
	TEXT("Enables individual asset streaming via HTTP")
);

bool GIasHttpOptionalBulkDataEnabled = true;
static FAutoConsoleVariableRef CVar_IasHttpOptionalBulkDataEnabled(
	TEXT("ias.HttpOptionalBulkDataEnabled"),
	GIasHttpOptionalBulkDataEnabled,
	TEXT("Enables optional bulk data via HTTP")
);

bool GIasReportAnalyticsEnabled = true;
static FAutoConsoleVariableRef CVar_IoReportAnalytics(
	TEXT("ias.ReportAnalytics"),
	GIasReportAnalyticsEnabled,
	TEXT("Enables reporting statics to the analytics system")
);

int32 GIasTocMode = 0;
static FAutoConsoleVariableRef CVar_IasTocMode(
	TEXT("ias.TocMode"),
	GIasTocMode,
	TEXT("How should the IAS system load it's toc (see ETocMode).\n")
	TEXT("0 = Load a single .iochunktoc\n")
	TEXT("1 = Try to find a .uondemandtoc each time a pak file is mounted\n")
	TEXT("2 = Download the toc from the target CDN"),
	ECVF_ReadOnly
);

static int32 GIasHttpRangeRequestMinSizeKiB = 128;
static FAutoConsoleVariableRef CVar_IasHttpRangeRequestMinSizeKiB(
	TEXT("ias.HttpRangeRequestMinSizeKiB"),
	GIasHttpRangeRequestMinSizeKiB,
	TEXT("Minimum chunk size for partial chunk request(s)")
);

static int32 GDistributedEndpointRetryWaitTime = 15;
static FAutoConsoleVariableRef CVar_DistributedEndpointRetryWaitTime(
	TEXT("ias.DistributedEndpointRetryWaitTime"),
	GDistributedEndpointRetryWaitTime,
	TEXT("How long to wait (in seconds) after failing to resolve a distributed endpoint before retrying")
);

static int32 GDistributedEndpointAttemptCount = 5;
static FAutoConsoleVariableRef CVar_DistributedEndpointAttemptCount(
	TEXT("ias.DistributedEndpointAttemptCount"),
	GDistributedEndpointAttemptCount,
	TEXT("Number of times we should try to resolve a distributed endpoint befor eusing the fallback url (if there is one)")
);

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommand CVar_IasAbandonCache(
	TEXT("Ias.AbandonCache"),
	TEXT("Abandon the local file cache"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		FIoStoreOnDemandModule& Module = FModuleManager::Get().GetModuleChecked<FIoStoreOnDemandModule>("IoStoreOnDemand");
		Module.AbandonCache();
	})
);
#endif //!UE_BUILD_SHIPPING
///////////////////////////////////////////////////////////////////////////////
#if !UE_BUILD_SHIPPING
static void LatencyTest(FStringView Url, FStringView Path)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::LatencyTest);

	int32 Results[4] = {};
	UE::IO::IAS::HTTP::LatencyTest(Url, Path, GIasHttpTimeOutMs, MakeArrayView(Results));
	UE_LOG(LogIas, Log, TEXT("Endpoint '%s' latency test (ms): %d %d %d %d"),
		Url.GetData(), Results[0], Results[1], Results[2], Results[3]);
}
#endif // !UE_BUILD_SHIPPING

///////////////////////////////////////////////////////////////////////////////
enum class ETocMode : int32
{
	LoadTocFromDisk = 0,	// <- Current Default
	LoadTocFromMountedPaks,
	LoadTocFromNetwork
};

static void ForceTocMode(ETocMode Mode)
{
	GIasTocMode = static_cast<int32>(Mode);
}

static ETocMode GetTocMode()
{
	if (GIasTocMode < 0 || GIasTocMode > static_cast<int32>(ETocMode::LoadTocFromNetwork))
	{
		UE_LOG(LogIas, Log, TEXT("ias.TocMode set to invalid value, defaulting to ETocMode::LoadTocFromDisk"));
		return ETocMode::LoadTocFromDisk;
	}

	return static_cast<ETocMode>(GIasTocMode);
}
///////////////////////////////////////////////////////////////////////////////
static int32 LatencyTest(TConstArrayView<FString> Urls, FStringView Path, std::atomic_bool& bCancel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::LatencyTest);

	for (int32 Idx = 0; Idx < Urls.Num() && !bCancel.load(std::memory_order_relaxed); ++Idx)
	{
		int32 LatencyMs = -1;
		UE::IO::IAS::HTTP::LatencyTest(Urls[Idx], Path, GIasHttpTimeOutMs, MakeArrayView(&LatencyMs, 1));
		if (LatencyMs > 0)
		{
			return Idx;
		}
	}

	return INDEX_NONE;
}
///////////////////////////////////////////////////////////////////////////////
struct FBitWindow
{
	void Reset(uint32 Count)
	{
		Count = FMath::RoundUpToPowerOfTwo(Count);
		Bits.SetNum(int32(Count), false);
		Counter = 0;
		Mask = Count - 1;
	}

	void Add(bool bValue)
	{
		const uint32 Idx = Counter++ & Mask;
		Bits[Idx] = bValue;
	}

	float AvgSetBits() const
	{
		return float(Bits.CountSetBits()) / float(Bits.Num());
	}

private:
	TBitArray<> Bits;
	uint32 Counter = 0;
	uint32 Mask = 0;
};
///////////////////////////////////////////////////////////////////////////////
FIoHash GetChunkKey(const FIoHash& ChunkHash, const FIoOffsetAndLength& Range)
{
	FIoHashBuilder HashBuilder;
	HashBuilder.Update(ChunkHash.GetBytes(), sizeof(FIoHash::ByteArray));
	HashBuilder.Update(&Range, sizeof(FIoOffsetAndLength));

	return HashBuilder.Finalize();
}

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoStore
{
public:
	struct FTocEntry
	{
		uint32 RawSize = 0;
		uint32 EncodedSize = 0;
		uint32 BlockOffset = ~uint32(0);
		uint32 BlockCount = 0; 
		FIoHash Hash;
	};

	struct FToc;

	struct FContainer
	{
		const FToc* Toc = nullptr;
		FAES::FAESKey EncryptionKey;
		FString Name; // TODO: Consider removing when NO_LOGGING == 1 (only used for logging at the moment)
		FString EncryptionKeyGuid;
		FString ChunksDirectory;
		FName CompressionFormat;
		uint32 BlockSize = 0;

		TMap<FIoChunkId, FTocEntry> TocEntries;
		TArray<uint32> BlockSizes;
		TArray<FIoBlockHash> BlockHashes;
	};

	struct FToc
	{
		FString TocPath;
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
		
		TConstArrayView<FIoBlockHash> GetBlockHashes() const
		{
			check(Container != nullptr && Entry != nullptr);
			return Container->BlockHashes.IsEmpty()
				? TConstArrayView<FIoBlockHash>()
				: TConstArrayView<FIoBlockHash>(Container->BlockHashes.GetData() + Entry->BlockOffset, Entry->BlockCount);
		}
	};

	FOnDemandIoStore();
	~FOnDemandIoStore();

	void AddToc(FStringView TocPath, FOnDemandToc&& Toc);
	void RemoveToc(FStringView TocPath);

	TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId);
	FChunkInfo GetChunkInfo(const FIoChunkId& ChunkId);
	FString GetFirstTocPath() const;

	TArray<FIoChunkId> GetAllChunkIds(bool bIncludeOptionalChunks);

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

void FOnDemandIoStore::AddToc(FStringView TocPath, FOnDemandToc&& Toc)
{
	UE_LOGFMT(LogIas, Log, "Adding TOC '{FileName}'", TocPath);

	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::AddToc);

	TStringBuilder<128> ChunksDirectory;

	{
		// First attempt to parse the prefix from the given path (legacy)
		int32 Idx = INDEX_NONE;
		if (TocPath.FindLastChar(TEXT('/'), Idx))
		{
			Algo::Transform(TocPath.Left(Idx), AppendChars(ChunksDirectory), FChar::ToLower);
		}

		// If there was no prefix in the path we should just use the ChunksDirectory from the toc itself
		if (ChunksDirectory.Len() == 0)
		{
			Algo::Transform(Toc.Header.ChunksDirectory, AppendChars(ChunksDirectory), FChar::ToLower);
			
		}

		FPathViews::Append(ChunksDirectory, TEXT("chunks"));
	}

	{
		FWriteScopeLock _(Lock);

		const FOnDemandTocHeader& Header = Toc.Header;
		FToc* NewToc = new(Tocs) FToc{FString(TocPath)};
		NewToc->Containers.SetNum(Toc.Containers.Num()); // List of containers can never change

		const FName CompressionFormat(Header.CompressionFormat);
		int32 ContainerIndex = 0;

		for (FOnDemandTocContainerEntry& Container : Toc.Containers)
		{
			FContainer* NewContainer = &NewToc->Containers[ContainerIndex++];
			NewContainer->Toc = NewToc;
			NewContainer->Name = MoveTemp(Container.ContainerName);
			NewContainer->ChunksDirectory = ChunksDirectory;
			NewContainer->CompressionFormat = CompressionFormat;
			NewContainer->BlockSize = Header.BlockSize;
			NewContainer->EncryptionKeyGuid = Container.EncryptionKeyGuid;
			
			NewContainer->TocEntries.Reserve(Container.Entries.Num());
			for (const FOnDemandTocEntry& TocEntry : Container.Entries)
			{
				check(TocEntry.RawSize <= 0xffff'ffffull);
				check(TocEntry.EncodedSize <= 0xffff'ffffull);
				NewContainer->TocEntries.Add(TocEntry.ChunkId, FTocEntry
				{
					uint32(TocEntry.RawSize),
					uint32(TocEntry.EncodedSize),
					TocEntry.BlockOffset,
					TocEntry.BlockCount,
					TocEntry.Hash,
				});
			}

			NewContainer->BlockSizes = MoveTemp(Container.BlockSizes);
			NewContainer->BlockHashes = MoveTemp(Container.BlockHashes);

			DeferredContainers.Add(NewContainer);
		}
	}

	AddDeferredContainers();
}

void FOnDemandIoStore::RemoveToc(FStringView TocPath)
{
	// TODO: Need to test this with content bundles!
	checkNoEntry();
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

TArray<FIoChunkId> FOnDemandIoStore::GetAllChunkIds(bool bIncludeOptionalChunks)
{
	FReadScopeLock _(Lock);

	TArray<FIoChunkId> ChunkIds;
	int32 NumChunks = 0;
	for (const FContainer* Container : RegisteredContainers)
	{
		NumChunks += Container->TocEntries.Num();
	}

	ChunkIds.Reserve(NumChunks);

	for (const FContainer* Container : RegisteredContainers)
	{
		for (const TPair<FIoChunkId, FTocEntry>& Entry : Container->TocEntries)
		{
			if (bIncludeOptionalChunks || Entry.Key.GetChunkType() != EIoChunkType::OptionalBulkData)
			{
				ChunkIds.Add(Entry.Key);
			}
		}
	}

	return ChunkIds;
}

void FOnDemandIoStore::AddDeferredContainers()
{
	FWriteScopeLock _(Lock);

	for (auto It = DeferredContainers.CreateIterator(); It; ++It)
	{
		FContainer* Container = *It;
		if (Container->EncryptionKeyGuid.IsEmpty())
		{
			check(Container->EncryptionKey.IsValid() == false);
			UE_LOG(LogIas, Log, TEXT("Mounting container '%s' (%d entries)"), *Container->Name, Container->TocEntries.Num());
			RegisteredContainers.Add(Container);
			It.RemoveCurrent();
		}
		else
		{
			FGuid KeyGuid;
			ensure(FGuid::Parse(Container->EncryptionKeyGuid, KeyGuid));
			if (FEncryptionKeyManager::Get().TryGetKey(KeyGuid, Container->EncryptionKey))
			{
				UE_LOG(LogIas, Log, TEXT("Mounting container '%s' (%d entries)"), *Container->Name, Container->TocEntries.Num());
				RegisteredContainers.Add(Container);
				It.RemoveCurrent();
			}
			else
			{
				UE_LOG(LogIas, Log, TEXT("Deferring container '%s', encryption key '%s' not available"), *Container->Name, *Container->EncryptionKeyGuid);
			}
		}
	}
}

void FOnDemandIoStore::OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key)
{
	LLM_SCOPE_BYTAG(Ias);
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::OnEncryptionKeyAdded);
	AddDeferredContainers();
}

FString FOnDemandIoStore::GetFirstTocPath() const
{
	FReadScopeLock _(Lock);
	return Tocs.Num() > 0 ? Tocs[0].TocPath : FString();
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

	void EnqueueByPriority(T* Request)
	{
		FScopeLock _(&CriticalSection);
		EnqueueByPriorityInternal(Request);
	}

	T* Dequeue()
	{
		FScopeLock _(&CriticalSection);

		T* Requests = Head;
		Head = Tail = nullptr;

		return Requests;
	}

	void Reprioritize(T* Request)
	{
		// Switch to double linked list/array if this gets too expensive
		FScopeLock _(&CriticalSection);
		if (RemoveInternal(Request))
		{
			EnqueueByPriorityInternal(Request);
		}
	}

private:
	void EnqueueByPriorityInternal(T* Request)
	{
		check(Request->NextRequest == nullptr);

		if (Head == nullptr || Request->Priority > Head->Priority)
		{
			if (Head == nullptr)
			{
				check(Tail == nullptr);
				Tail = Request;
			}

			Request->NextRequest = Head;
			Head = Request;
		}
		else if (Request->Priority <= Tail->Priority)
		{
			check(Tail != nullptr);
			Tail->NextRequest = Request;
			Tail = Request;
		}
		else
		{
			// NOTE: This can get expensive if the queue gets too long, might be better to have x number of bucket(s)
			TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::EnqueueByPriority);
			T* It = Head;
			while (It->NextRequest != nullptr && Request->Priority <= It->NextRequest->Priority)
			{
				It = It->NextRequest;
			}

			Request->NextRequest = It->NextRequest;
			It->NextRequest = Request;
		}
	}

	bool RemoveInternal(T* Request)
	{
		check(Request != nullptr);
		if (Head == nullptr)
		{
			check(Tail == nullptr);
			return false;
		}

		if (Head == Request)
		{
			Head = Request->NextRequest; 
			if (Tail == Request)
			{
				check(Head == nullptr);
				Tail = nullptr;
			}

			Request->NextRequest = nullptr;
			return true;
		}
		else
		{
			T* It = Head;
			while (It->NextRequest && It->NextRequest != Request)
			{
				It = It->NextRequest;
			}

			if (It->NextRequest == Request)
			{
				It->NextRequest = It->NextRequest->NextRequest;
				Request->NextRequest = nullptr;
				if (Tail == Request)
				{
					Tail = It;
				}
				return true;
			}
		}

		return false;
	}

	FCriticalSection CriticalSection;
	T* Head = nullptr;
	T* Tail = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
struct FChunkRequestParams
{
	static FChunkRequestParams Create(const FIoOffsetAndLength& OffsetLength, FOnDemandIoStore::FChunkInfo ChunkInfo)
	{
		FIoOffsetAndLength ChunkRange;
		if (ChunkInfo.Entry->EncodedSize <= (uint64(GIasHttpRangeRequestMinSizeKiB) << 10))
		{
			ChunkRange = FIoOffsetAndLength(0, ChunkInfo.Entry->EncodedSize);
		}
		else
		{
			const uint64 RawSize = FMath::Min<uint64>(OffsetLength.GetLength(), ChunkInfo.Entry->RawSize);

			ChunkRange = FIoChunkEncoding::GetChunkRange(
				ChunkInfo.Entry->RawSize,
				ChunkInfo.Container->BlockSize,
				ChunkInfo.GetBlocks(),
				OffsetLength.GetOffset(),
				RawSize).ConsumeValueOrDie();
		}

		return FChunkRequestParams{GetChunkKey(ChunkInfo.Entry->Hash, ChunkRange), ChunkRange, ChunkInfo};
	}

	static FChunkRequestParams Create(FIoRequestImpl* Request, FOnDemandIoStore::FChunkInfo ChunkInfo)
	{
		check(Request);
		check(Request->NextRequest == nullptr);
		return Create(FIoOffsetAndLength(Request->Options.GetOffset(), Request->Options.GetSize()), ChunkInfo);
	}

	const FIoHash& GetUrlHash() const
	{
		return ChunkInfo.Entry->Hash;
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
		const FAES::FAESKey& EncryptionKey = ChunkInfo.Container->EncryptionKey;

		FIoChunkDecodingParams Params;
		Params.EncryptionKey = EncryptionKey.IsValid() ? MakeMemoryView(EncryptionKey.Key, FAES::FAESKey::KeySize) : FMemoryView();
		Params.CompressionFormat = ChunkInfo.Container->CompressionFormat;
		Params.BlockSize = ChunkInfo.Container->BlockSize;
		Params.TotalRawSize = ChunkInfo.Entry->RawSize;
		Params.EncodedBlockSize = ChunkInfo.GetBlocks(); 
		Params.BlockHash = ChunkInfo.GetBlockHashes(); 
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
		, Priority(Request->Priority)
		, RequestCount(1)
		, bCached(false)
	{
		check(Request && NextRequest == nullptr);
	}

	bool AddDispatcherRequest(FIoRequestImpl* Request)
	{
		check(RequestHead && RequestTail);
		check(Request && !Request->NextRequest);

		const bool bPriorityChanged = Request->Priority > RequestHead->Priority;
		if (bPriorityChanged)
		{
			Priority = Request->Priority;
			Request->NextRequest = RequestHead;
			RequestHead = Request;
		}
		else
		{
			FIoRequestImpl* It = RequestHead;
			while (It->NextRequest != nullptr && Request->Priority <= It->NextRequest->Priority)
			{
				It = It->NextRequest;
			}

			if (RequestTail == It)
			{
				check(It->NextRequest == nullptr);
				RequestTail = Request;
			}

			Request->NextRequest = It->NextRequest;
			It->NextRequest = Request;
		}

		RequestCount++;
		return bPriorityChanged;
	}

	int32 RemoveDispatcherRequest(FIoRequestImpl* Request)
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
				if (It == nullptr)
				{
					return INDEX_NONE; // Not found
				}
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

	FChunkRequest* NextRequest;
	FChunkRequestParams Params;
	FIoRequestImpl* RequestHead;
	FIoRequestImpl* RequestTail;
	FIoBuffer Chunk;
	uint64 StartTime;
	int32 Priority;
	uint16 RequestCount;
	bool bCached;
	bool bCancelled = false;
	EIoErrorCode CacheGetStatus;
};

///////////////////////////////////////////////////////////////////////////////

static void LogIoResult(
	const FIoChunkId& ChunkId,
	const FIoHash& UrlHash,
	uint64 DurationMs,
	uint64 UncompressedSize,
	uint64 UncompressedOffset,
	const FIoOffsetAndLength& ChunkRange,
	uint64 ChunkSize,
	int32 Priority,
	bool bCached)
{
	const TCHAR* Prefix = [bCached, UncompressedSize]() -> const TCHAR*
	{
		if (UncompressedSize == 0)
		{
			return bCached ? TEXT("io-cache-error") : TEXT("io-http-error ");
		}
		return bCached ? TEXT("io-cache") : TEXT("io-http ");
	}();

	auto PrioToString = [](int32 Prio) -> const TCHAR*
	{
		if (Prio < IoDispatcherPriority_Low)
		{
			return TEXT("Min");
		}
		if (Prio < IoDispatcherPriority_Medium)
		{
			return TEXT("Low");
		}
		if (Prio < IoDispatcherPriority_High)
		{
			return TEXT("Medium");
		}
		if (Prio < IoDispatcherPriority_Max)
		{
			return TEXT("High");
		}

		return TEXT("Max");
	};

	UE_LOG(LogIas, VeryVerbose, TEXT("%s: %5" UINT64_FMT "ms %5" UINT64_FMT "KiB[%7" UINT64_FMT "] % s: % s | Range: %" UINT64_FMT "-%" UINT64_FMT "/%" UINT64_FMT " (%.2f%%) | Prio: %s"),
		Prefix,
		DurationMs,
		UncompressedSize >> 10,
		UncompressedOffset,
		*LexToString(ChunkId),
		*LexToString(UrlHash),
		ChunkRange.GetOffset(), (ChunkRange.GetOffset() + ChunkRange.GetLength() - 1), ChunkSize,
		100.0f * (float(ChunkRange.GetLength()) / float(ChunkSize)),
		PrioToString(Priority));
};

/**
 * Utility to create a FArchive capable of reading from disk using the exact same pathing
 * rules as FPlatformMisc::LoadTextFileFromPlatformPackage but without forcing the entire
 * file to be loaded at once.
 */
static TUniquePtr<FArchive> CreateReaderFromPlatformPackage(const FString& RelPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::CreateReaderFromPlatformPackage);

	const FString AbsPath = FPaths::Combine(FGenericPlatformMisc::RootDir(), RelPath);

	IFileHandle* File = IPlatformFile::GetPlatformPhysical().OpenRead(*AbsPath);
	if (File)
	{
#if PLATFORM_ANDROID
		// This is a handle to an asset so we need to call Seek(0) to move the internal
		// offset to the start of the asset file.
		File->Seek(0);
#endif //PLATFORM_ANDROID
		const uint32 ReadBufferSize = 256 * 1024;
		return MakeUnique<FArchiveFileReaderGeneric>(File, *AbsPath, File->Size(), ReadBufferSize);
	}
	else
	{
		return TUniquePtr<FArchive>();
	}	
}

/** Loads a single .iochunktoc (containing all of our container tocs) from disk */
static TIoStatusOr<FOnDemandToc> LoadOnDemandTocFromDisk(FStringView TocHash)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::LoadOnDemandTocFromDisk);

	FOnDemandToc OutToc;

	const ETocMode TocMode = GetTocMode();
	if (TocMode == ETocMode::LoadTocFromDisk)
	{
		FString TocFileName = FString(TocHash);
		TocFileName .Append(TEXT(".iochunktoc"));

		const FString TocPath = FPaths::Combine(TEXT("Cloud"), TocFileName);

		if (FPlatformMisc::FileExistsInPlatformPackage(TocPath))
		{	
			TUniquePtr<FArchive> Ar = CreateReaderFromPlatformPackage(TocPath);
			if (!Ar.IsValid())
			{
				return FIoStatus(EIoErrorCode::FileOpenFailed, WriteToString<256>(TEXT("Failed to open '"), TocPath, TEXT("' from disk")));
			}

			*Ar << OutToc;

			Ar->Close();

			if (!Ar->IsError() && !Ar->IsCriticalError())
			{
				UE_LOG(LogIas, Log, TEXT("Loaded '%s' from disk"), *TocPath);
				return OutToc;
			}
			else
			{
				return FIoStatus(EIoErrorCode::ReadError, WriteToString<256>(TEXT("Failed to serialize '"), TocPath, TEXT("' from disk")));
			}
		}
		else
		{
			return FIoStatus(EIoErrorCode::NotFound, WriteToString<256>(TEXT("Unable to find '"), TocPath, TEXT("' on disk")));
		}
	}
	else
	{
		checkNoEntry();
	}

	return OutToc;
}

///////////////////////////////////////////////////////////////////////////////
struct FBackendStatus
{
	enum class EFlags : uint8
	{
		None						= 0,
		CacheEnabled				= (1 << 0),
		HttpEnabled					= (1 << 1),
		HttpError					= (1 << 2),
		HttpBulkOptionalDisabled	= (1 << 3),
		AbandonCache				= (1 << 4),

		// When adding new values here, remember to update operator<<(FStringBuilderBase& Sb, EFlags StatusFlags) below!
	};

	bool IsHttpEnabled() const
	{
		return IsHttpEnabled(Flags.load(std::memory_order_relaxed));
	}

	bool IsHttpEnabled(EIoChunkType ChunkType) const
	{
		const uint8 CurrentFlags = Flags.load(std::memory_order_relaxed);
		return IsHttpEnabled(CurrentFlags) &&
			(ChunkType != EIoChunkType::OptionalBulkData ||
				((CurrentFlags & uint8(EFlags::HttpBulkOptionalDisabled)) == 0 && GIasHttpOptionalBulkDataEnabled));
	}

	bool IsHttpError() const
	{
		return HasAnyFlags(EFlags::HttpError);
	}

	bool IsCacheEnabled() const
	{
		return HasAnyFlags(EFlags::CacheEnabled);
	}

	bool IsCacheWriteable() const
	{
		const uint8 CurrentFlags = Flags.load(std::memory_order_relaxed);
		return (CurrentFlags & uint8(EFlags::CacheEnabled)) && IsHttpEnabled(CurrentFlags); 
	}

	bool IsCacheReadOnly() const
	{
		const uint8 CurrentFlags = Flags.load(std::memory_order_relaxed);
		return (CurrentFlags & uint8(EFlags::CacheEnabled)) && !IsHttpEnabled(CurrentFlags);
	}

	bool ShouldAbandonCache() const
	{
		return HasAnyFlags(EFlags::AbandonCache);
	}

	void SetHttpEnabled(bool bEnabled)
	{
		AddOrRemoveFlags(EFlags::HttpEnabled, bEnabled, TEXT("HTTP streaming enabled"));
		FGenericCrashContext::SetEngineData(TEXT("IAS.Enabled"), bEnabled ? TEXT("true") : TEXT("false"));
	}

	void SetHttpOptionalBulkEnabled(bool bEnabled)
	{
		AddOrRemoveFlags(EFlags::HttpBulkOptionalDisabled, bEnabled == false, TEXT("HTTP streaming of optional bulk data disabled"));
	}

	void SetCacheEnabled(bool bEnabled)
	{
		AddOrRemoveFlags(EFlags::CacheEnabled, bEnabled, TEXT("Cache enabled"));
	}

	void SetHttpError(bool bError)
	{
		AddOrRemoveFlags(EFlags::HttpError, bError, TEXT("HTTP streaming error"));
	}

	void SetAbandonCache(bool bAbandon)
	{
		AddOrRemoveFlags(EFlags::AbandonCache, bAbandon, TEXT("Abandon cache"));
	}

private:
	static bool IsHttpEnabled(uint8 FlagsToTest)
	{
		constexpr uint8 HttpFlags = uint8(EFlags::HttpEnabled) | uint8(EFlags::HttpError);
		return ((FlagsToTest & HttpFlags) == uint8(EFlags::HttpEnabled)) && GIasHttpEnabled;
	}

	bool HasAnyFlags(uint8 Contains) const
	{
		return (Flags.load(std::memory_order_relaxed) & Contains) != 0;
	}
	
	bool HasAnyFlags(EFlags Contains) const
	{
		return HasAnyFlags(uint8(Contains));
	}

	uint8 AddFlags(EFlags FlagsToAdd)
	{
		return Flags.fetch_or(uint8(FlagsToAdd));
	}

	uint8 RemoveFlags(EFlags FlagsToRemove)
	{
		return Flags.fetch_and(~uint8(FlagsToRemove));
	}

	uint8 AddOrRemoveFlags(EFlags FlagsToAddOrRemove, bool bValue)
	{
		return bValue ? AddFlags(FlagsToAddOrRemove) : RemoveFlags(FlagsToAddOrRemove);
	}

	void AddOrRemoveFlags(EFlags FlagsToAddOrRemove, bool bValue, const TCHAR* DebugText)
	{
		const uint8 PrevFlags = AddOrRemoveFlags(FlagsToAddOrRemove, bValue);
		TStringBuilder<128> Sb;
		Sb.Append(DebugText)
			<< TEXT(" '");
		Sb.Append(bValue ? TEXT("true") : TEXT("false"))
			<< TEXT("', backend status '(")
			<< EFlags(PrevFlags)
			<< TEXT(") -> (")
			<< EFlags(Flags.load(std::memory_order_relaxed))
			<< TEXT(")'");
		UE_LOG(LogIas, Log, TEXT("%s"), Sb.ToString());
	}

	friend FStringBuilderBase& operator<<(FStringBuilderBase& Sb, EFlags StatusFlags)
	{
		if (StatusFlags == EFlags::None)
		{
			Sb.Append(TEXT("None"));
			return Sb;
		}

		bool bFirst = true;
		auto AppendIf = [StatusFlags, &Sb, &bFirst](EFlags Contains, const TCHAR* Str)
		{
			if (uint8(StatusFlags) & uint8(Contains))
			{
				if (!bFirst)
				{
					Sb.AppendChar(TEXT('|'));
				}
				Sb.Append(Str);
				bFirst = false;
			}
		};

		AppendIf(EFlags::CacheEnabled, TEXT("CacheEnabled"));
		AppendIf(EFlags::HttpEnabled, TEXT("HttpEnabled"));
		AppendIf(EFlags::HttpError, TEXT("HttpError"));
		AppendIf(EFlags::HttpBulkOptionalDisabled, TEXT("HttpBulkOptionalDisabled"));
		AppendIf(EFlags::AbandonCache, TEXT("AbandonCache"));

		return Sb;
	}

	std::atomic<uint8> Flags{0};
};
///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoBackend final
	: public FRunnable
	, public IOnDemandIoDispatcherBackend
{
	using FIoRequestQueue = TThreadSafeIntrusiveQueue<FIoRequestImpl>;
	using FChunkRequestQueue = TThreadSafeIntrusiveQueue<FChunkRequest>;

	struct FAvailableEps
	{
		bool HasCurrent() const { return Current != INDEX_NONE; }
		const FString& GetCurrent() const { return Urls[Current]; }

		int32 Current = INDEX_NONE;
		TArray<FString> Urls;
	};

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
		
		static FBackendData* Get(FIoRequestImpl* Request)
		{
			return static_cast<FBackendData*>(Request->BackendData);
		}

		FIoHash ChunkKey;
	};

	struct FChunkRequests
	{
		FChunkRequest* TryUpdatePriority(FIoRequestImpl* Request)
		{
			FScopeLock _(&Mutex);

			const FBackendData* BackendData = FBackendData::Get(Request);
			if (BackendData == nullptr)
			{
				return nullptr;
			}

			if (FChunkRequest** InflightRequest = Inflight.Find(BackendData->ChunkKey))
			{
				FChunkRequest* ChunkRequest = *InflightRequest;
				if (Request->Priority > ChunkRequest->Priority)
				{
					ChunkRequest->Priority = Request->Priority;
					return ChunkRequest;
				}
			}

			return nullptr;
		}

		FChunkRequest* Create(FIoRequestImpl* Request, const FChunkRequestParams& Params, bool& bOutPending, bool& bOutUpdatePriority)
		{
			FScopeLock _(&Mutex);
			
			FBackendData::Attach(Request, Params.ChunkKey);

			if (FChunkRequest** InflightRequest = Inflight.Find(Params.ChunkKey))
			{
				FChunkRequest* ChunkRequest = *InflightRequest;
				check(!ChunkRequest->bCancelled);
				bOutPending = true;
				bOutUpdatePriority = ChunkRequest->AddDispatcherRequest(Request);

				return ChunkRequest;
			}

			bOutPending = bOutUpdatePriority = false;
			FChunkRequest* ChunkRequest = Allocator.Construct(Request, Params);
			ChunkRequestCount++;
			Inflight.Add(Params.ChunkKey, ChunkRequest);

			return ChunkRequest;
		}

		bool Cancel(FIoRequestImpl* Request, IIasCache* TheCache)
		{
			FScopeLock _(&Mutex);

			const FBackendData* BackendData = FBackendData::Get(Request);
			if (BackendData == nullptr)
			{
				return false;
			}

			UE_LOG(LogIas, VeryVerbose, TEXT("%s"),
				*WriteToString<256>(TEXT("Cancelling I/O request ChunkId='"), Request->ChunkId, TEXT("' ChunkKey='"), BackendData->ChunkKey, TEXT("'")));

			if (FChunkRequest** InflightRequest = Inflight.Find(BackendData->ChunkKey))
			{
				FChunkRequest& ChunkRequest = **InflightRequest;
				const int32 RemainingCount = ChunkRequest.RemoveDispatcherRequest(Request);
				if (RemainingCount == INDEX_NONE)
				{
					// Not found
					// When a request A with ChunkKey X enters CompleteRequest its Inflight entry X->A is removed.
					// If a new request B with the same ChunkKey X is made, then Resolve will add a new Infligt entry X->B.
					// If we at this point cancel A, we will find the Inflight entry for B, which will not contain A, which is fine.
					return false;
				}

				check(Request->NextRequest == nullptr);

				if (RemainingCount == 0)
				{
					ChunkRequest.bCancelled = true;
					if (TheCache != nullptr)
					{
						TheCache->Cancel(ChunkRequest.Chunk);
					}
					Inflight.Remove(BackendData->ChunkKey);
				}

				return true;
			}

			return false;
		}

		FIoChunkId GetChunkId(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			return Request->RequestHead ? Request->RequestHead->ChunkId : FIoChunkId::InvalidChunkId;
		}

		void Remove(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Inflight.Remove(Request->Params.ChunkKey);
		}

		void Release(FChunkRequest* Request)
		{
			FScopeLock _(&Mutex);
			Destroy(Request);
		}
		
		int32 Num()
		{
			FScopeLock _(&Mutex);
			return ChunkRequestCount; 
		}

	private:
		void Destroy(FChunkRequest* Request)
		{
			Allocator.Destroy(Request);
			ChunkRequestCount--;
			check(ChunkRequestCount >= 0);
		}

		TSingleThreadedSlabAllocator<FChunkRequest, 128> Allocator;
		TMap<FIoHash, FChunkRequest*> Inflight;
		FCriticalSection Mutex;
		int32 ChunkRequestCount = 0;
	};
public:

	FOnDemandIoBackend(TUniquePtr<IIasCache>&& InCache);
	virtual ~FOnDemandIoBackend();

	// I/O dispatcher backend
	virtual void Initialize(TSharedRef<const FIoDispatcherBackendContext> Context) override;
	virtual void Shutdown() override;
	virtual bool Resolve(FIoRequestImpl* Request) override;
	virtual void CancelIoRequest(FIoRequestImpl* Request) override;
	virtual void UpdatePriorityForIoRequest(FIoRequestImpl* Request) override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId) const override;
	virtual bool DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<uint64> GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const;
	virtual FIoRequestImpl* GetCompletedRequests() override;
	virtual TIoStatusOr<FIoMappedRegion> OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;

	// I/O Http backend
	virtual void Mount(const FOnDemandEndpoint& Endpoint) override;
	virtual void SetBulkOptionalEnabled(bool bEnabled) override;
	virtual void SetEnabled(bool bEnabled) override;
	virtual bool IsEnabled() const override;
	virtual void AbandonCache() override;
	virtual void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const override;

	// Runnable
	virtual bool Init() override { return true; }
	virtual uint32 Run() override;
	virtual void Stop() override;

private:

	void MountContainer(FStringView ContainerPath);
	void UnmountContainer(FStringView ContainerPath);

	FString GetEndpointTestPath() const;
	void ConditionallyStartBackendThread();
	void CompleteRequest(FChunkRequest* ChunkRequest);
	void CompleteMaterialize(FChunkRequest* ChunkRequest);

	FIoStatus ApplyLoadedOnDemandToc(const FString& TocPath);
	FIoStatus DownloadoadOnDemandToc(const FString& CdnUrl, const FString& TocPath);

	bool ResolveDistributedEndpoint(const FDistributedEndpointUrl& Url);
	void InitializePrimaryEndpoint();

	/** Mode to control which types of .iochunktoc are flush */
	enum class EFlushMode : uint8
	{
		/** Flush tocs that require disk access */
		Disk	= 1 << 0,
		/** Flush tocs that require network access */
		Network = 1 << 1,

		/** Flush all types of tocs */
		All		= Disk | Network,
	};

	void FlushDeferredTocs(EFlushMode FlushMode);

	bool SetupHttpThread();
	void ProcessHttpRequests(FHttpClient& HttpClient, FBitWindow& HttpErrors, int32 MaxConcurrentRequests);
	int32 WaitForCompleteRequestTasks(float WaitTimeSeconds, float PollTimeSeconds);
	void DrainHttpRequests();

	struct FTocParams
	{
		FString Path;
		bool bForceDownload;
	};

	TUniquePtr<IIasCache> Cache;
	TUniquePtr<FOnDemandIoStore> IoStore;
	TSharedPtr<const FIoDispatcherBackendContext> BackendContext;
	TUniquePtr<FRunnableThread> BackendThread;
	FEventRef TickBackendEvent;
	TArray<FTocParams> DeferredTocs;
	FChunkRequests ChunkRequests;
	FIoRequestQueue CompletedRequests;
	FChunkRequestQueue HttpRequests;
	TArray<FChunkRequest*> PendingCacheGets;
	FOnDemandIoBackendStats Stats;
	FBackendStatus BackendStatus;
	FAvailableEps AvailableEps;
	FDistributedEndpointUrl DistributionUrl;
	FEventRef DistributedEndpointEvent;

	FString EndpointTestPath;

	FDelegateHandle PakDelegateHandle;

	mutable FRWLock Lock;
	std::atomic_uint32_t InflightCacheRequestCount{0};
	std::atomic_bool bStopRequested{false};

	bool bGeneratedOnDemandToc = false;
	UE::Tasks::TTask<TIoStatusOr<FOnDemandToc>> LoadingOnDemandTocTask;

#if UE_IAS_DEBUG_CONSOLE_CMDS
	TArray<IConsoleCommand*> DynamicConsoleCommands;
#endif // UE_IAS_DEBUG_CONSOLE_CMDS
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandIoBackend::FOnDemandIoBackend(TUniquePtr<IIasCache>&& InCache)
	: Cache(MoveTemp(InCache))
{
	IoStore = MakeUnique<FOnDemandIoStore>();
	BackendStatus.SetHttpEnabled(true);
	BackendStatus.SetCacheEnabled(Cache.IsValid());

#if UE_IAS_DEBUG_CONSOLE_CMDS
	DynamicConsoleCommands.Emplace(
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ias.InvokeHttpFailure"),
		TEXT("Marks the current ias http connection as failed forcing the system to try to reconnect"),
		FConsoleCommandDelegate::CreateLambda([this]()
			{
				UE_LOG(LogIas, Display, TEXT("User invoked http error via 'ias.InvokeHttpFailure'"));
				BackendStatus.SetHttpError(true);

				TickBackendEvent->Trigger();
			}),
		ECVF_Cheat)
	);
#endif // UE_IAS_DEBUG_CONSOLE_CMDS

	{
		bool bForceTocFromMountedPaks = false;
		GConfig->GetBool(TEXT("Ias"), TEXT("ForceTocFromMountedPaks"), bForceTocFromMountedPaks, GEngineIni);

		if (bForceTocFromMountedPaks)
		{
			ForceTocMode(ETocMode::LoadTocFromMountedPaks);
		}
	}
}

FOnDemandIoBackend::~FOnDemandIoBackend()
{
	if (PakDelegateHandle.IsValid())
	{
		FCoreInternalDelegates::GetOnPakMountOperation().Remove(PakDelegateHandle);
	}

#if UE_IAS_DEBUG_CONSOLE_CMDS
	for (IConsoleCommand* Cmd : DynamicConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
#endif // UE_IAS_DEBUG_CONSOLE_CMDS

	Shutdown();
}

void FOnDemandIoBackend::Initialize(TSharedRef<const FIoDispatcherBackendContext> Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::Initialize);
	LLM_SCOPE_BYTAG(Ias);
	UE_LOG(LogIas, Log, TEXT("Initializing on demand I/O dispatcher backend"));
	BackendContext = Context;

	ConditionallyStartBackendThread();
}

void FOnDemandIoBackend::Shutdown()
{
	if (bStopRequested)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::Shutdown);

	UE_LOG(LogIas, Log, TEXT("Shutting down on demand I/O dispatcher backend"));

	// Stop and wait for our backend thread to finish.
	// Note, the IoDispatcher typically waits for all its pending io requests before shutting down its backends.
	BackendThread.Reset();

	// Drain any reamaining (cancelled) http requests that already been completed from the IoDispatcher point of view.
	DrainHttpRequests();

	// The CompleteRequest tasks may still be executing a while after the IoDispatcher has been notified about the completed io requests.
	const int32 NumPending = WaitForCompleteRequestTasks(5.0f, 0.1f);
	UE_CLOG(NumPending > 0, LogIas, Warning, TEXT("%d request(s) still pending after shutdown"), NumPending);

	BackendContext.Reset();
}

void FOnDemandIoBackend::MountContainer(FStringView ContainerPath)
{
	// TODO: For now we just log an error if a container cannot be mounted, but for production
	// we will need to make sure that the user is informed so that they can verify their installation.
	IPlatformFile& PlatformFile = IPlatformFile::GetPlatformPhysical();

	const FString TocPath = FPathViews::ChangeExtension(ContainerPath, GIasOnDemandTocExt);

	if (PlatformFile.FileExists(*TocPath))
	{
		UE_LOG(LogIas, Log, TEXT("Mounting '%s'"), *TocPath);

		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*TocPath));

		if (!Ar.IsValid())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to open '%s' for reading"), *TocPath);
			return;
		}

		/// First we read and validate the sentinel at the end of the file to check for potential
		// file corruption that could lead to crashing during serialization.
		{
			const int64 SentinelPos = Ar->TotalSize() - FOnDemandTocSentinel::SentinelSize;

			if (SentinelPos < 0)
			{
				UE_LOG(LogIas, Error, TEXT("The file '%s' is smaller than expected and quite possible corrupted"), *TocPath);
				return;
			}

			Ar->Seek(SentinelPos);

			FOnDemandTocSentinel Sentinel;
			(*Ar) << Sentinel;

			if (!Sentinel.IsValid())
			{
				UE_LOG(LogIas, Error, TEXT("File corruption detected when serializing '%s'"), *TocPath);
				return;
			}

			Ar->Seek(0);
		}

		FOnDemandToc ContainerToc;
		(*Ar) << ContainerToc;

		if (Ar->IsError() || Ar->IsCriticalError())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to serialize '%s'"), *TocPath);
			return;
		}

		const FStringView FileName = FPathViews::GetCleanFilename(TocPath);
		IoStore->AddToc(FileName, MoveTemp(ContainerToc));
	}
}

void FOnDemandIoBackend::UnmountContainer(FStringView ContainerPath)
{
	const FString TocPath = FPathViews::ChangeExtension(ContainerPath, GIasOnDemandTocExt);
	const FStringView FileName = FPathViews::GetCleanFilename(TocPath);

	IoStore->RemoveToc(FileName);
}

FString FOnDemandIoBackend::GetEndpointTestPath() const
{
	if (!EndpointTestPath.IsEmpty())
	{
		return EndpointTestPath;
	}

	// Older fallback path, it probably shouldn't be possible to get this far and can be removed later.
	FString TestPath = IoStore.IsValid() ? IoStore->GetFirstTocPath() : FString();
	if (TestPath.IsEmpty())
	{
		FReadScopeLock _(Lock);
		if (DeferredTocs.IsEmpty() == false)
		{
			TestPath = DeferredTocs[0].Path;
		}
	}
	return TestPath;
}

void FOnDemandIoBackend::ConditionallyStartBackendThread()
{
	FWriteScopeLock _(Lock);
	if (BackendThread.IsValid() == false)
	{
		BackendThread.Reset(FRunnableThread::Create(this, TEXT("Ias.Http"), 0, TPri_AboveNormal));
	}
}

void FOnDemandIoBackend::CompleteRequest(FChunkRequest* ChunkRequest)
{
	LLM_SCOPE_BYTAG(Ias);
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::CompleteRequest);
	check(ChunkRequest != nullptr);

	if (ChunkRequest->bCancelled)
	{
		check(ChunkRequest->RequestHead == nullptr);
		check(ChunkRequest->RequestTail == nullptr);
		return ChunkRequests.Release(ChunkRequest);
	}

	ChunkRequests.Remove(ChunkRequest);
	
	FIoBuffer Chunk = MoveTemp(ChunkRequest->Chunk);
	FIoChunkDecodingParams DecodingParams = ChunkRequest->Params.GetDecodingParams();

	// Only cache chunks if HTTP streaming is enabled
	bool bCacheChunk = ChunkRequest->bCached == false && Chunk.GetSize() > 0;
	FIoRequestImpl* NextRequest = ChunkRequest->DeqeueDispatcherRequests();
	while (NextRequest)
	{
		FIoRequestImpl* Request = NextRequest;
		NextRequest = Request->NextRequest;
		Request->NextRequest = nullptr;

		bool bDecoded = false;
		if (Chunk.GetSize() > 0)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::DecodeBlocks);
			const uint64 RawSize = FMath::Min<uint64>(Request->Options.GetSize(), ChunkRequest->Params.ChunkInfo.Entry->RawSize);
			Request->CreateBuffer(RawSize);
			DecodingParams.RawOffset = Request->Options.GetOffset(); 
			bDecoded = FIoChunkEncoding::Decode(DecodingParams, Chunk.GetView(), Request->GetBuffer().GetMutableView());

			if (!bDecoded)
			{
				Stats.OnIoDecodeError();
			}
		}
		
		const uint64 DurationMs = Request->GetStartTime() > 0 ?
			(uint64)FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Request->GetStartTime()) : 0;

		if (bDecoded)
		{
			Stats.OnIoRequestComplete(Request->GetBuffer().GetSize(), DurationMs);
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), DurationMs,
				Request->GetBuffer().DataSize(), Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange, ChunkRequest->Params.ChunkInfo.Entry->EncodedSize,
				ChunkRequest->Priority, ChunkRequest->bCached);
				
		}
		else
		{
			bCacheChunk = false;
			Request->SetFailed();

			Stats.OnIoRequestError();
			LogIoResult(Request->ChunkId, ChunkRequest->Params.GetUrlHash(), DurationMs,
				0, Request->Options.GetOffset(),
				ChunkRequest->Params.ChunkRange, ChunkRequest->Params.ChunkInfo.Entry->EncodedSize,
				ChunkRequest->Priority, ChunkRequest->bCached);
		}

		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}

	if (bCacheChunk && BackendStatus.IsCacheWriteable())
	{
		Cache->Put(ChunkRequest->Params.ChunkKey, Chunk);
	}

	if (BackendStatus.ShouldAbandonCache() && InflightCacheRequestCount.load(std::memory_order_relaxed) == 0)
	{
		TickBackendEvent->Trigger();
	}

	ChunkRequests.Release(ChunkRequest);
}

void FOnDemandIoBackend::CompleteMaterialize(FChunkRequest* ChunkRequest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::CompleteMaterialize);

	bool bWasCancelled = false;
	switch (ChunkRequest->CacheGetStatus)
	{
		case EIoErrorCode::Ok:
		check(ChunkRequest->Chunk.GetData() != nullptr);
		ChunkRequest->bCached = true;
		CompleteRequest(ChunkRequest);
		return;

	case EIoErrorCode::ReadError:
		FOnDemandIoBackendStats::Get()->OnCacheError();
		break;

	case EIoErrorCode::Cancelled:
		bWasCancelled = true;
		break;

	case EIoErrorCode::NotFound:
		break;
	}

	if (bWasCancelled || BackendStatus.IsHttpEnabled() == false)
	{
		UE_CLOG(BackendStatus.IsHttpEnabled() == false, LogIas, Log, TEXT("Chunk was not found in the cache and HTTP is disabled"));
		CompleteRequest(ChunkRequest);
		return;
	}

	Stats.OnHttpEnqueue();
	HttpRequests.EnqueueByPriority(ChunkRequest);
	TickBackendEvent->Trigger();
}

bool FOnDemandIoBackend::Resolve(FIoRequestImpl* Request)
{
	using namespace UE::Tasks;

	FOnDemandIoStore::FChunkInfo ChunkInfo = IoStore->GetChunkInfo(Request->ChunkId);
	if (!ChunkInfo.IsValid())
	{
		return false;
	}

	FChunkRequestParams RequestParams = FChunkRequestParams::Create(Request, ChunkInfo);

	if (BackendStatus.IsHttpEnabled(Request->ChunkId.GetChunkType()) == false)
	{ 
		// If the cache is not readonly the chunk may get evicted before the request is completed
		if (BackendStatus.IsCacheReadOnly() == false || Cache->ContainsChunk(RequestParams.ChunkKey) == false)
		{
			return false;
		}
	}

	Stats.OnIoRequestEnqueue();
	bool bPending = false;
	bool bUpdatePriority = false;
	FChunkRequest* ChunkRequest = ChunkRequests.Create(Request, RequestParams, bPending, bUpdatePriority);

	if (bPending)
	{
		if (bUpdatePriority)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::UpdatePriorityForIoRequest);
			HttpRequests.Reprioritize(ChunkRequest);
		}
		// The chunk for the request is already inflight 
		return true;
	}

	if (Cache.IsValid())
	{
		const FIoHash& Key = ChunkRequest->Params.ChunkKey;
		FIoBuffer& Buffer = ChunkRequest->Chunk;

		//TODO: Pass priority to cache
		EIoErrorCode GetStatus = Cache->Get(Key, Buffer);

		if (GetStatus == EIoErrorCode::Ok)
		{
			check(Buffer.GetData() != nullptr);
			ChunkRequest->bCached = true;
			Launch(UE_SOURCE_LOCATION, [this, ChunkRequest] {
				CompleteRequest(ChunkRequest);
			});
			return true;
		}

		if (GetStatus == EIoErrorCode::FileNotOpen)
		{
			InflightCacheRequestCount.fetch_add(1, std::memory_order_relaxed);

			FTaskEvent OnReadyEvent(TEXT("IasCacheMaterializeDone"));

			Launch(UE_SOURCE_LOCATION, [this, ChunkRequest] {
				InflightCacheRequestCount.fetch_sub(1, std::memory_order_relaxed);
				CompleteMaterialize(ChunkRequest);
			}, OnReadyEvent);

			EIoErrorCode& OutStatus = ChunkRequest->CacheGetStatus;
			Cache->Materialize(Key, Buffer, OutStatus, MoveTemp(OnReadyEvent));
			return true;
		}

		check(GetStatus == EIoErrorCode::NotFound);
	}

	Stats.OnHttpEnqueue();
	HttpRequests.EnqueueByPriority(ChunkRequest);
	TickBackendEvent->Trigger();
	return true;
}

void FOnDemandIoBackend::CancelIoRequest(FIoRequestImpl* Request)
{
	if (ChunkRequests.Cancel(Request, Cache.Get()))
	{
		CompletedRequests.Enqueue(Request);
		BackendContext->WakeUpDispatcherThreadDelegate.Execute();
	}
}

void FOnDemandIoBackend::UpdatePriorityForIoRequest(FIoRequestImpl* Request)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::UpdatePriorityForIoRequest);
	if (FChunkRequest* ChunkRequest = ChunkRequests.TryUpdatePriority(Request))
	{
		HttpRequests.Reprioritize(ChunkRequest);
	}
}

bool FOnDemandIoBackend::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	const TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId);
	return ChunkSize.IsOk();
}

bool FOnDemandIoBackend::DoesChunkExist(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange) const
{
	uint64 Unused = 0;
	const TIoStatusOr<uint64> ChunkSize = GetSizeForChunk(ChunkId, ChunkRange, Unused);
	return ChunkSize.IsOk();
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	uint64 Unused = 0;
	const FIoOffsetAndLength ChunkRange(0, MAX_uint64);
	return GetSizeForChunk(ChunkId, ChunkRange, Unused);
}

TIoStatusOr<uint64> FOnDemandIoBackend::GetSizeForChunk(const FIoChunkId& ChunkId, const FIoOffsetAndLength& ChunkRange, uint64& OutAvailable) const
{
	OutAvailable = 0;

	const FOnDemandIoStore::FChunkInfo ChunkInfo = IoStore.IsValid() ? IoStore->GetChunkInfo(ChunkId) : FOnDemandIoStore::FChunkInfo();
	if (ChunkInfo.IsValid() == false)
	{
		return FIoStatus(EIoErrorCode::UnknownChunkID);
	}

	FIoOffsetAndLength RequestedRange(ChunkRange.GetOffset(), FMath::Min<uint64>(ChunkInfo.Entry->RawSize, ChunkRange.GetLength()));
	OutAvailable = ChunkInfo.Entry->RawSize;

	if (BackendStatus.IsHttpEnabled(ChunkId.GetChunkType()) == false)
	{
		// If the cache is not readonly the chunk may get evicted before the request is resolved
		if (BackendStatus.IsCacheReadOnly() == false)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		check(Cache.IsValid());
		const FChunkRequestParams RequestParams = FChunkRequestParams::Create(RequestedRange, ChunkInfo);
		if (Cache->ContainsChunk(RequestParams.ChunkKey) == false)
		{
			return FIoStatus(EIoErrorCode::UnknownChunkID);
		}

		// Only the specified chunk range is available 
		OutAvailable = RequestedRange.GetLength();
	}

	return TIoStatusOr<uint64>(ChunkInfo.Entry->RawSize);
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

FIoStatus FOnDemandIoBackend::ApplyLoadedOnDemandToc(const FString& TocPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::ApplyLoadedOnDemandToc);

	if (bGeneratedOnDemandToc)
	{
		return FIoStatus::Ok;
	}

	TIoStatusOr<FOnDemandToc> GeneratedTocResult;

	if (LoadingOnDemandTocTask.IsValid())
	{
		GeneratedTocResult = MoveTemp(LoadingOnDemandTocTask.GetResult());
	}
	else
	{
		GeneratedTocResult = LoadOnDemandTocFromDisk(FPathViews::GetBaseFilename(TocPath));
	}
	
	if (!GeneratedTocResult.IsOk())
	{
		return GeneratedTocResult.Status();
	}

	IoStore->AddToc(TocPath, GeneratedTocResult.ConsumeValueOrDie());

	bGeneratedOnDemandToc = true;

	return FIoStatus::Ok;
}

FIoStatus FOnDemandIoBackend::DownloadoadOnDemandToc(const FString& CdnUrl, const FString& TocPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::LoadOnDemandToc);

	UE_LOG(LogIas, Log, TEXT("Downloading OnDemandToc from CDN"));

	TIoStatusOr<FOnDemandToc> TocResult = LoadTocFromUrl(CdnUrl, TocPath, 1);
	if (TocResult.IsOk())
	{
		IoStore->AddToc(TocPath, TocResult.ConsumeValueOrDie());
		return FIoStatus::Ok;
	}
	else
	{
		return TocResult.Status();
	}
}

bool FOnDemandIoBackend::ResolveDistributedEndpoint(const FDistributedEndpointUrl& DistributedEndpointUrl)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::ResolveDistributedEndpoint);

	check(DistributedEndpointUrl.IsValid());

	// We need to resolve the end point in this method which occurs after the config system has initialized
	// rather than in ::Mount which can occur before that.
	// Without the config system initialized the http module will not work properly and we will always fail
	// to resolve and the OnDemand system will not recover.
	check(GConfig->IsReadyForUse());

	int32 NumAttempts = 0;

	while (!bStopRequested)
	{
		TArray<FString> ServiceUrls;

		FDistributionEndpoints Resolver;
		FDistributionEndpoints::EResult Result = Resolver.ResolveEndpoints(DistributedEndpointUrl.EndpointUrl, ServiceUrls, *DistributedEndpointEvent.Get());
		if (Result == FDistributionEndpoints::EResult::Success)
		{
			FWriteScopeLock _(Lock);
			for (const FString& Url : ServiceUrls)
			{
				AvailableEps.Urls.Add(Url.Replace(TEXT("https"), TEXT("http")));
			}

			return true;
		}

		if (DistributedEndpointUrl.HasFallbackUrl() && ++NumAttempts == GDistributedEndpointAttemptCount)
		{
			FString FallbackUrl = DistributedEndpointUrl.FallbackUrl.Replace(TEXT("https"), TEXT("http"));
			UE_LOG(LogIas, Warning, TEXT("Failed to resolve the distributed endpoint %d times. Fallback CDN '%s' will be used instead"), GDistributedEndpointAttemptCount , *FallbackUrl);
			
			FWriteScopeLock _(Lock);
			AvailableEps.Urls.Emplace(MoveTemp(FallbackUrl));
		
			return true;
		}

		if (!bStopRequested)
		{
			const uint32 WaitTime = GDistributedEndpointRetryWaitTime >= 0 ? (static_cast<uint32>(GDistributedEndpointRetryWaitTime) * 1000) : MAX_uint32;
			DistributedEndpointEvent->Wait(WaitTime);
		}
	}

	return false;
}

void FOnDemandIoBackend::InitializePrimaryEndpoint()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::InitializePrimaryEndpoint);

	// We only need to run this code if we do not have an currently selected, most likely because
	// a list of endpoints was polled from online rather than a url being provided by the cmdline/config system.
	if (!AvailableEps.HasCurrent())
	{
		UE_LOG(LogIas, Log, TEXT("Attempting to find the primary endpoint..."));

		TConstArrayView<FString> Urls = AvailableEps.Urls;
		const int32 MaxUrls = FMath::Min(GIasMaxEndpointTestCountAtStartup, AvailableEps.Urls.Num());
		const FString TestPath = GetEndpointTestPath();
		if (int32 Idx = LatencyTest(Urls.Left(MaxUrls), TestPath, bStopRequested); Idx != INDEX_NONE)
		{
			FOnDemandIoBackendStats::Get()->OnHttpConnected();

			AvailableEps.Current = Idx;
			UE_LOG(LogIas, Log, TEXT("Using endpoint '%s'"), *AvailableEps.GetCurrent());
		}
		else
		{
			UE_LOG(LogIas, Error, TEXT("Unable to connect to any valid endpoint"));
			BackendStatus.SetHttpError(true);
			return;
		}
	}

	// Now we have a valid endpoint, we need to flush any pending .iochunktoc download requests
	FlushDeferredTocs(EFlushMode::All);
}

void FOnDemandIoBackend::FlushDeferredTocs(EFlushMode FlushMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::FlushDeferredTocs);

	TArray<FTocParams> Tocs;
	{
		FWriteScopeLock _(Lock);
		if (DeferredTocs.IsEmpty())
		{
			return;
		}
		Tocs = MoveTemp(DeferredTocs);
	}

	const ETocMode TocMode = GetTocMode();

	for (FTocParams& TocParams : Tocs)
	{
		if (TocMode == ETocMode::LoadTocFromDisk && !TocParams.bForceDownload)
		{
			if (EnumHasAnyFlags(FlushMode, EFlushMode::Disk))
			{
				FIoStatus Result = ApplyLoadedOnDemandToc(TocParams.Path);
				if (!Result.IsOk())
				{
					UE_LOG(LogIas, Error, TEXT("Failed to add generated toc', reason '%s'"), *Result.ToString());
				}
			}
			else
			{
				FWriteScopeLock _(Lock);
				DeferredTocs.Emplace(MoveTemp(TocParams));
			}
		}
		else
		{
			if (EnumHasAnyFlags(FlushMode, EFlushMode::Network) && AvailableEps.HasCurrent())
			{
				FIoStatus Result = DownloadoadOnDemandToc(AvailableEps.GetCurrent(), TocParams.Path);
				if (!Result.IsOk())
				{
					UE_LOG(LogIas, Error, TEXT("Failed to add TOC '%s/%s', reason '%s'"), *AvailableEps.GetCurrent(), *TocParams.Path, *Result.ToString());
				}
			}
			else
			{
				FWriteScopeLock _(Lock);
				DeferredTocs.Emplace(MoveTemp(TocParams));
			}
		}
	}
}

void FOnDemandIoBackend::Mount(const FOnDemandEndpoint& Endpoint)
{
	LLM_SCOPE_BYTAG(Ias);
	TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::Mount);

	if ((Endpoint.DistributionUrl.IsEmpty() && Endpoint.ServiceUrls.IsEmpty()) || Endpoint.TocPath.IsEmpty())
	{
		UE_LOG(LogIas, Error, TEXT("Trying to mount an invalid on demand endpoint"));
		return;
	}

	EndpointTestPath = Endpoint.TocPath;

	// TODO: Should pass this info around via DeferredTocs
	if (Endpoint.bForceTocDownload)
	{
		ForceTocMode(ETocMode::LoadTocFromNetwork);
	}

	if (GetTocMode() == ETocMode::LoadTocFromDisk)
	{
		FString TocHash = FPaths::GetBaseFilename(Endpoint.TocPath);
		LoadingOnDemandTocTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [TocHash = MoveTemp(TocHash)]() -> TIoStatusOr<FOnDemandToc>
			{
				return LoadOnDemandTocFromDisk(TocHash);
			});
	}
	else if (GetTocMode() == ETocMode::LoadTocFromMountedPaks)
	{
		// First make sure that we receive notifications of any new pakfiles being mounted so that we can react to them
		PakDelegateHandle = FCoreInternalDelegates::GetOnPakMountOperation().AddLambda(
			[this](EMountOperation Operation, const TCHAR* ContainerPath, int32 Order) -> void
			{
				switch (Operation)
				{
					case EMountOperation::Mount:
						this->MountContainer(ContainerPath);
						break;
					case EMountOperation::Unmount:
						this->UnmountContainer(ContainerPath);
						break;
					default:
						checkNoEntry();
				}
			}
		);

		FCurrentlyMountedPaksDelegate& Delegate = FCoreInternalDelegates::GetCurrentlyMountedPaksDelegate();
		if (Delegate.IsBound())
		{
			TArray<FMountedPakInfo> PreExistingMountedPaks = Delegate.Execute();
			for (const FMountedPakInfo& Info : PreExistingMountedPaks)
			{
				check(Info.PakFile != nullptr);
				MountContainer(Info.PakFile->PakGetPakFilename());
			}
		}
	}

	{
		FWriteScopeLock _(Lock);
		if (Endpoint.ServiceUrls.IsEmpty())
		{
			if (AvailableEps.HasCurrent() == false)
			{
				if (!DistributionUrl.IsValid())
				{
					DistributionUrl = { Endpoint.DistributionUrl, Endpoint.FallbackUrl };
				}

				if (GetTocMode() != ETocMode::LoadTocFromMountedPaks)
				{
					DeferredTocs.Add(FTocParams{ Endpoint.TocPath, Endpoint.bForceTocDownload });
				}

				return;
			}
		}
		else if (AvailableEps.Urls.IsEmpty())
		{
			for (const FString& Url : Endpoint.ServiceUrls)
			{
				AvailableEps.Urls.Add(Url.Replace(TEXT("https"), TEXT("http")));
			}
		}
	}

	if (GetTocMode() == ETocMode::LoadTocFromNetwork)
	{
		if (AvailableEps.HasCurrent())
		{
			FIoStatus Result = DownloadoadOnDemandToc(AvailableEps.GetCurrent(), Endpoint.TocPath);
			if (!Result.IsOk())
			{
				UE_LOG(LogIas, Error, TEXT("Deferring TOC '%s/%s' due to '%s'"), *AvailableEps.GetCurrent(), *Endpoint.TocPath, *Result.ToString());
				BackendStatus.SetHttpError(true);
				FWriteScopeLock _(Lock);
				DeferredTocs.Add(FTocParams{ Endpoint.TocPath, Endpoint.bForceTocDownload });
			}
		}
		else
		{
			FWriteScopeLock _(Lock);
			DeferredTocs.Add(FTocParams{ Endpoint.TocPath, Endpoint.bForceTocDownload });
		}
	}
	else if (GetTocMode() == ETocMode::LoadTocFromDisk)
	{
		FIoStatus GeneratedResult = ApplyLoadedOnDemandToc(Endpoint.TocPath);
		if (!GeneratedResult.IsOk())
		{
			UE_LOG(LogIas, Error, TEXT("Failed to add generated toc', reason '%s'"), *GeneratedResult.ToString());
		}	
	}
}

void FOnDemandIoBackend::SetBulkOptionalEnabled(bool bEnabled)
{
	// Ignore enable/disable messages if we are mounted from the pak file system
	// TODO: Remove SetBulkOptionalEnabled entirely once LoadTocFromMountedPaks is
	// the only valid mode
	if (GetTocMode() != ETocMode::LoadTocFromMountedPaks)
	{
		BackendStatus.SetHttpOptionalBulkEnabled(bEnabled);
	}
}

void FOnDemandIoBackend::SetEnabled(bool bEnabled)
{
	// Ignore enable/disable messages if we are mounted from the pak file system
	// TODO: Remove SetEnabled entirely once LoadTocFromMountedPaks is the only
	// valid mode
	if (GetTocMode() != ETocMode::LoadTocFromMountedPaks)
	{
		BackendStatus.SetHttpEnabled(bEnabled);
	}
}

bool FOnDemandIoBackend::IsEnabled() const
{
	return BackendStatus.IsHttpEnabled();
}

void FOnDemandIoBackend::AbandonCache()
{
	BackendStatus.SetCacheEnabled(false);
	BackendStatus.SetAbandonCache(true);
}

void FOnDemandIoBackend::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	// If we got this far we know that IAS is enabled for the current process as it has a valid backend.
	// However just because IAS is enabled does not mean we have managed to make a valid connection yet.

	if (!GIasReportAnalyticsEnabled)
	{
		return;
	}

	Stats.ReportGeneralAnalytics(OutAnalyticsArray);

	if (AvailableEps.HasCurrent())
	{
		FString CdnUrl = AvailableEps.GetCurrent();

		// Strip the prefix from the url as some analytics systems may have trouble dealing with it
		if (!CdnUrl.RemoveFromStart(TEXT("http://")))
		{
			CdnUrl.RemoveFromStart(TEXT("https://"));
		}

		AppendAnalyticsEventAttributeArray(OutAnalyticsArray, TEXT("IasCdnUrl"), MoveTemp(CdnUrl));

		Stats.ReportEndPointAnalytics(OutAnalyticsArray);
	}
}

void FOnDemandIoBackend::ProcessHttpRequests(FHttpClient& HttpClient, FBitWindow& HttpErrors, int32 MaxConcurrentRequests)
{
	int32 NumConcurrentRequests = 0;
	FChunkRequest* NextChunkRequest = HttpRequests.Dequeue();

	while (NextChunkRequest)
	{
		while (NextChunkRequest)
		{
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::IssueHttpGet);
				FChunkRequest* ChunkRequest = NextChunkRequest;
				NextChunkRequest = ChunkRequest->NextRequest;
				ChunkRequest->NextRequest = nullptr;

				Stats.OnHttpDequeue();

				if (ChunkRequest->bCancelled)
				{
					CompleteRequest(ChunkRequest);
					Stats.OnHttpCancel();
				}
				else if (BackendStatus.IsHttpEnabled() == false)
				{
					CompleteRequest(ChunkRequest);
					// Technically this request is being skipped because of a pre-existing error. It is not
					// an error itself and it is not being canceled by higher level code. However we do not
					// currently have a statistic for that and we have to call one of the existing types in
					// order to correctly reduce the pending count.
					Stats.OnHttpCancel();
				}
				else
				{
					check(HttpClient.GetEndpoint() != INDEX_NONE);
					TAnsiStringBuilder<256> Url;
					ChunkRequest->Params.GetUrl(Url);

					NumConcurrentRequests++;
					HttpClient.Get(Url.ToView(), ChunkRequest->Params.ChunkRange,
						[this, &NextChunkRequest, ChunkRequest, &NumConcurrentRequests, &HttpErrors]
						(TIoStatusOr<FIoBuffer> Status, uint64 DurationMs)
						{
							NumConcurrentRequests--;
							switch (Status.Status().GetErrorCode())
							{
							case EIoErrorCode::Ok:
							{
								HttpErrors.Add(false);
								ChunkRequest->Chunk = Status.ConsumeValueOrDie();
								Stats.OnHttpGet(ChunkRequest->Chunk.DataSize(), DurationMs);
								break;
							}
							case EIoErrorCode::ReadError:
							case EIoErrorCode::NotFound:
							{
								Stats.OnHttpError();
								HttpErrors.Add(true);

								const float Average = HttpErrors.AvgSetBits();
								const bool bAboveHighWaterMark = Average > GIasHttpErrorHighWater;
								UE_LOG(LogIas, Log, TEXT("%.2f%% the last %d HTTP requests failed"), Average * 100.0f, GIasHttpErrorSampleCount);

								if (bAboveHighWaterMark && BackendStatus.IsHttpEnabled())
								{
									BackendStatus.SetHttpError(true);
									UE_LOG(LogIas, Warning, TEXT("HTTP streaming disabled due to high water mark of %.2f of the last %d requests reached"),
										GIasHttpErrorHighWater * 100.0f, GIasHttpErrorSampleCount);
								}
								break;
							}
							case EIoErrorCode::Cancelled:
							{
								const FIoChunkId ChunkId = ChunkRequests.GetChunkId(ChunkRequest);
								UE_LOG(LogIas, Log, TEXT("HTTP request for chunk '%s' cancelled"), *LexToString(ChunkId));
								break;
							}
							default:
							{
								const FIoChunkId ChunkId = ChunkRequests.GetChunkId(ChunkRequest);
								UE_LOG(LogIas, Warning, TEXT("Unhandled HTTP response '%s' for chunk '%s'"),
									GetIoErrorText(Status.Status().GetErrorCode()), *LexToString(ChunkId));
								break;
							}
							}

							UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, ChunkRequest]()
							{
								CompleteRequest(ChunkRequest);
							});
						});
				}
			}

			if (NumConcurrentRequests >= MaxConcurrentRequests)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::TickHttpSaturated);
				while (NumConcurrentRequests >= MaxConcurrentRequests) //-V654
				{
					HttpClient.Tick(MAX_uint32, GIasHttpRateLimitKiBPerSecond);
				}
			}

			if (!NextChunkRequest)
			{
				NextChunkRequest = HttpRequests.Dequeue();
			}
		}

		{
			// Keep processing pending connections until all requests are completed or a new one is issued
			TRACE_CPUPROFILER_EVENT_SCOPE(IasBackend::TickHttp);
			while (HttpClient.Tick(GIasHttpPollTimeoutMs, GIasHttpRateLimitKiBPerSecond))
			{
				if (!NextChunkRequest)
				{
					NextChunkRequest = HttpRequests.Dequeue();
				}
				if (NextChunkRequest)
				{
					break;
				}
			}
		}
	} 
}

void FOnDemandIoBackend::DrainHttpRequests()
{
	FChunkRequest* Iterator = HttpRequests.Dequeue();
	while (Iterator != nullptr)
	{
		FChunkRequest* Request = Iterator;
		Iterator = Iterator->NextRequest;

		Stats.OnHttpDequeue();
		CompleteRequest(Request);
		Stats.OnHttpCancel();
	}
}

bool FOnDemandIoBackend::SetupHttpThread()
{
	// A disk only flush should be fairly quick at this point, especially if 's.IasEnableThreadedTocGeneration' is true
	// and this will help mount .iochunktoc as soon as possible rather than waiting for the primary endpoint to be
	// chosen.
	FlushDeferredTocs(EFlushMode::Disk);

	// Note that the following method will block until the distributed endpoint (if we have one) has
	// been resolved as the thread cannot function without it.
	if (DistributionUrl.IsValid())
	{
		const bool bResult = ResolveDistributedEndpoint(DistributionUrl);
		DistributionUrl.Reset();

		if (!bResult)
		{
			return false;
		}
		
	}

	InitializePrimaryEndpoint();

	return true;
}

uint32 FOnDemandIoBackend::Run()
{
	LLM_SCOPE_BYTAG(Ias);

	// This call can take some time depending on the IAS setup and the network conditions
	if (!SetupHttpThread())
	{
		return 0;
	}

	FBitWindow HttpErrors;
	HttpErrors.Reset(GIasHttpErrorSampleCount);

	TUniquePtr<FHttpClient> HttpClient = FHttpClient::Create(FHttpClientConfig
	{
		.Endpoints = AvailableEps.Urls,
		.PrimaryEndpoint = FMath::Min(GIasHttpPrimaryEndpoint, AvailableEps.Urls.Num() -1),
		.MaxConnectionCount = GIasHttpConnectionCount,
		.PipelineLength = GIasHttpPipelineLength,
		.MaxRetryCount = FMath::Max(AvailableEps.Urls.Num() + 1, GIasHttpRetryCount),
		.ReceiveBufferSize = GIasHttpRecvBufKiB >= 0 ? GIasHttpRecvBufKiB << 10 : -1,
		.bChangeEndpointAfterSuccessfulRetry = GIasHttpChangeEndpointAfterSuccessfulRetry,
	});
	check(HttpClient.IsValid());
	HttpClient->SetEndpoint(AvailableEps.Current);
#if !UE_BUILD_SHIPPING
	if (AvailableEps.HasCurrent())
	{
		LatencyTest(AvailableEps.GetCurrent(), GetEndpointTestPath());
	}
#endif 

	while (!bStopRequested)
	{
		// Process HTTP request(s) even if the client is invalid to ensure enqueued request(s) gets completed.
		ProcessHttpRequests(*HttpClient, HttpErrors, FMath::Min(GIasHttpConcurrentRequests, 32));
		AvailableEps.Current = HttpClient->GetEndpoint();

		if (!bStopRequested)
		{
			uint32 WaitTime = MAX_uint32;
			if (BackendStatus.IsHttpError())
			{
				WaitTime = GIasHttpHealthCheckWaitTime;
				if (HttpClient->GetEndpoint() != INDEX_NONE)
				{
					FOnDemandIoBackendStats::Get()->OnHttpDisconnected();

					AvailableEps.Current = INDEX_NONE;
					HttpClient->SetEndpoint(INDEX_NONE);
					HttpErrors.Reset(GIasHttpErrorSampleCount);
				}

				UE_LOG(LogIas, Log, TEXT("Trying to reconnect to any available endpoint"));
				const FString TestPath = GetEndpointTestPath(); 
				if (int32 Idx = LatencyTest(AvailableEps.Urls, TestPath, bStopRequested); Idx != INDEX_NONE)
				{
					FOnDemandIoBackendStats::Get()->OnHttpConnected();

					AvailableEps.Current = Idx;
					HttpClient->SetEndpoint(Idx);
					BackendStatus.SetHttpError(false);
					UE_LOG(LogIas, Log, TEXT("Successfully reconnected to '%s'"), *AvailableEps.GetCurrent());
					FlushDeferredTocs(EFlushMode::All);
				}
			}
			else if (HttpClient->IsUsingPrimaryEndpoint() == false)
			{
				WaitTime = GIasHttpHealthCheckWaitTime;
				const FString TestPath = GetEndpointTestPath(); 
				TConstArrayView<FString> Urls = AvailableEps.Urls;

				if (int32 Idx = LatencyTest(Urls.Left(1), TestPath, bStopRequested); Idx != INDEX_NONE)
				{
					AvailableEps.Current = Idx;
					HttpClient->SetEndpoint(Idx);
					UE_LOG(LogIas, Log, TEXT("Reconnected to primary endpoint '%s'"), *AvailableEps.GetCurrent());
				}
			}
			if (BackendStatus.ShouldAbandonCache())
			{
				BackendStatus.SetAbandonCache(false);
				check(BackendStatus.IsCacheEnabled() == false);
				if (Cache.IsValid())
				{
					UE_LOG(LogIas, Log, TEXT("Abandoning cache, local file cache is no longer available"));
					Cache.Release()->Abandon(); // Will delete its self
				}
			}
			TickBackendEvent->Wait(WaitTime);
		}
	}

	return 0;
}

void FOnDemandIoBackend::Stop()
{
	bStopRequested = true;
	TickBackendEvent->Trigger();
	DistributedEndpointEvent->Trigger();
}

int32 FOnDemandIoBackend::WaitForCompleteRequestTasks(float WaitTimeSeconds, float PollTimeSeconds)
{
	const double StartTime = FPlatformTime::Seconds();
	while (ChunkRequests.Num() > 0 && float(FPlatformTime::Seconds() - StartTime) < WaitTimeSeconds)
	{
		FPlatformProcess::SleepNoStats(PollTimeSeconds);
	}

	return ChunkRequests.Num();
}

TSharedPtr<IOnDemandIoDispatcherBackend> MakeOnDemandIoDispatcherBackend(TUniquePtr<IIasCache>&& Cache)
{
	return MakeShareable<IOnDemandIoDispatcherBackend>(new FOnDemandIoBackend(MoveTemp(Cache)));
}

} // namespace UE::IO::IAS

#undef UE_IAS_DEBUG_CONSOLE_CMDS
