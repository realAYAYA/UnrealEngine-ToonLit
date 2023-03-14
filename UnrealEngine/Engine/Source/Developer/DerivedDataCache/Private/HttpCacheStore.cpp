// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBackendInterface.h"
#include "DerivedDataLegacyCacheStore.h"
#include "Templates/Tuple.h"

#if WITH_HTTP_DDC_BACKEND

#include "Algo/Transform.h"
#include "Compression/CompressedBuffer.h"
#include "Containers/StringView.h"
#include "Containers/Ticker.h"
#include "DerivedDataCacheKey.h"
#include "DerivedDataCacheRecord.h"
#include "DerivedDataCacheUsageStats.h"
#include "DerivedDataChunk.h"
#include "DerivedDataRequest.h"
#include "DerivedDataRequestOwner.h"
#include "DerivedDataValue.h"
#include "DesktopPlatformModule.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformProcess.h"
#include "Http/HttpClient.h"
#include "IO/IoHash.h"
#include "Memory/SharedBuffer.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinaryValidation.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "String/Find.h"

#if PLATFORM_MICROSOFT
#include "Microsoft/WindowsHWrapper.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#if WITH_SSL
#include "Ssl.h"
#endif

#define UE_HTTPDDC_GET_REQUEST_POOL_SIZE 48
#define UE_HTTPDDC_PUT_REQUEST_POOL_SIZE 16
#define UE_HTTPDDC_NONBLOCKING_REQUEST_POOL_SIZE 128
#define UE_HTTPDDC_MAX_FAILED_LOGIN_ATTEMPTS 16
#define UE_HTTPDDC_MAX_ATTEMPTS 4

namespace UE::DerivedData
{

static bool bHttpEnableAsync = true;
static FAutoConsoleVariableRef CVarHttpEnableAsync(
	TEXT("DDC.Http.EnableAsync"),
	bHttpEnableAsync,
	TEXT("If true, asynchronous operations are permitted, otherwise all operations are forced to be synchronous."),
	ECVF_Default);

TRACE_DECLARE_INT_COUNTER(HttpDDC_Get, TEXT("HttpDDC Get"));
TRACE_DECLARE_INT_COUNTER(HttpDDC_GetHit, TEXT("HttpDDC Get Hit"));
TRACE_DECLARE_INT_COUNTER(HttpDDC_Put, TEXT("HttpDDC Put"));
TRACE_DECLARE_INT_COUNTER(HttpDDC_PutHit, TEXT("HttpDDC Put Hit"));
TRACE_DECLARE_INT_COUNTER(HttpDDC_BytesReceived, TEXT("HttpDDC Bytes Received"));
TRACE_DECLARE_INT_COUNTER(HttpDDC_BytesSent, TEXT("HttpDDC Bytes Sent"));

static bool ShouldAbortForShutdown()
{
	return !GIsBuildMachine && FDerivedDataBackend::Get().IsShuttingDown();
}

static bool IsValueDataReady(FValue& Value, const ECachePolicy Policy)
{
	if (!EnumHasAnyFlags(Policy, ECachePolicy::Query))
	{
		Value = Value.RemoveData();
		return true;
	}

	if (Value.HasData())
	{
		if (EnumHasAnyFlags(Policy, ECachePolicy::SkipData))
		{
			Value = Value.RemoveData();
		}
		return true;
	}
	return false;
};

static FAnsiStringView GetDomainFromUri(const FAnsiStringView Uri)
{
	FAnsiStringView Domain = Uri;
	if (const int32 SchemeIndex = String::FindFirst(Domain, ANSITEXTVIEW("://")); SchemeIndex != INDEX_NONE)
	{
		Domain.RightChopInline(SchemeIndex + ANSITEXTVIEW("://").Len());
	}
	if (const int32 SlashIndex = String::FindFirstChar(Domain, '/'); SlashIndex != INDEX_NONE)
	{
		Domain.LeftInline(SlashIndex);
	}
	if (const int32 AtIndex = String::FindFirstChar(Domain, '@'); AtIndex != INDEX_NONE)
	{
		Domain.RightChopInline(AtIndex + 1);
	}
	const auto RemovePort = [](FAnsiStringView& Authority)
	{
		if (const int32 ColonIndex = String::FindLastChar(Authority, ':'); ColonIndex != INDEX_NONE)
		{
			Authority.LeftInline(ColonIndex);
		}
	};
	if (Domain.StartsWith('['))
	{
		if (const int32 LastBracketIndex = String::FindLastChar(Domain, ']'); LastBracketIndex != INDEX_NONE)
		{
			Domain.MidInline(1, LastBracketIndex - 1);
		}
		else
		{
			RemovePort(Domain);
		}
	}
	else
	{
		RemovePort(Domain);
	}
	return Domain;
}

static bool TryResolveCanonicalHost(const FAnsiStringView Uri, FAnsiStringBuilderBase& OutUri)
{
	// Append the URI until the end of the domain.
	const FAnsiStringView Domain = GetDomainFromUri(Uri);
	const int32 OutUriIndex = OutUri.Len();
	const int32 DomainIndex = int32(Domain.GetData() - Uri.GetData());
	const int32 DomainEndIndex = DomainIndex + Domain.Len();
	OutUri.Append(Uri.Left(DomainEndIndex));

	// Append the URI beyond the end of the domain before returning.
	ON_SCOPE_EXIT { OutUri.Append(Uri.RightChop(DomainEndIndex)); };

	// Try to resolve the host.
	::addrinfo* Result = nullptr;
	::addrinfo Hints{};
	Hints.ai_flags = AI_CANONNAME;
	Hints.ai_family = AF_UNSPEC;
	if (::getaddrinfo(*OutUri + OutUriIndex + DomainIndex, nullptr, &Hints, &Result) == 0)
	{
		ON_SCOPE_EXIT { ::freeaddrinfo(Result); };
		if (Result->ai_canonname)
		{
			OutUri.RemoveSuffix(Domain.Len());
			OutUri.Append(Result->ai_canonname);
			return true;
		}
	}
	return false;
}

/**
 * Encapsulation for access token shared by all requests.
 */
class FHttpAccessToken
{
public:
	void SetToken(FStringView Token);
	inline uint32 GetSerial() const { return Serial.load(std::memory_order_relaxed); }
	friend FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, const FHttpAccessToken& Token);

private:
	mutable FRWLock Lock;
	TArray<ANSICHAR> Header;
	std::atomic<uint32> Serial;
};

void FHttpAccessToken::SetToken(const FStringView Token)
{
	FWriteScopeLock WriteLock(Lock);
	const FAnsiStringView Prefix = ANSITEXTVIEW("Bearer ");
	const int32 TokenLen = FPlatformString::ConvertedLength<ANSICHAR>(Token.GetData(), Token.Len());
	Header.Empty(Prefix.Len() + TokenLen);
	Header.Append(Prefix.GetData(), Prefix.Len());
	const int32 TokenIndex = Header.AddUninitialized(TokenLen);
	FPlatformString::Convert(Header.GetData() + TokenIndex, TokenLen, Token.GetData(), Token.Len());
	Serial.fetch_add(1, std::memory_order_relaxed);
}

FAnsiStringBuilderBase& operator<<(FAnsiStringBuilderBase& Builder, const FHttpAccessToken& Token)
{
	FReadScopeLock ReadLock(Token.Lock);
	return Builder.Append(Token.Header);
}

struct FHttpCacheStoreParams
{
	FString Host;
	FString HostPinnedPublicKeys;
	FString Namespace;
	FString OAuthProvider;
	FString OAuthClientId;
	FString OAuthSecret;
	FString OAuthScope;
	FString OAuthProviderIdentifier;
	FString OAuthAccessToken;
	FString OAuthPinnedPublicKeys;
	bool bResolveHostCanonicalName = true;
	bool bReadOnly = false;

	void Parse(const TCHAR* NodeName, const TCHAR* Config);
};

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore
//----------------------------------------------------------------------------------------------------------

/**
 * Backend for a HTTP based caching service (Jupiter).
 */
class FHttpCacheStore final : public ILegacyCacheStore
{
public:
	
	/**
	 * Creates the cache store client, checks health status and attempts to acquire an access token.
	 */
	explicit FHttpCacheStore(const FHttpCacheStoreParams& Params);

	~FHttpCacheStore();

	/**
	 * Checks is cache service is usable (reachable and accessible).
	 * @return true if usable
	 */
	inline bool IsUsable() const { return bIsUsable; }

	void Put(
		TConstArrayView<FCachePutRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutComplete&& OnComplete) final;
	void Get(
		TConstArrayView<FCacheGetRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetComplete&& OnComplete) final;
	void PutValue(
		TConstArrayView<FCachePutValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCachePutValueComplete&& OnComplete) final;
	void GetValue(
		TConstArrayView<FCacheGetValueRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetValueComplete&& OnComplete) final;
	void GetChunks(
		TConstArrayView<FCacheGetChunkRequest> Requests,
		IRequestOwner& Owner,
		FOnCacheGetChunkComplete&& OnComplete) final;

	void LegacyStats(FDerivedDataCacheStatsNode& OutNode) final;
	bool LegacyDebugOptions(FBackendDebugOptions& Options) final;

	static FHttpCacheStore* GetAny()
	{
		return AnyInstance;
	}

	const FString& GetDomain() const { return Domain; }
	const FString& GetNamespace() const { return Namespace; }
	const FString& GetOAuthProvider() const { return OAuthProvider; }
	const FString& GetOAuthClientId() const { return OAuthClientId; }
	const FString& GetOAuthSecret() const { return OAuthSecret; }
	const FString& GetOAuthScope() const { return OAuthScope; }
	const FString& GetOAuthProviderIdentifier() const { return OAuthProviderIdentifier; }
	const FString& GetOAuthAccessToken() const { return OAuthAccessToken; }

private:
	FString Domain;
	FString Namespace;
	FString OAuthProvider;
	FString OAuthClientId;
	FString OAuthSecret;
	FString OAuthScope;
	FString OAuthProviderIdentifier;
	FString OAuthAccessToken;

	FAnsiStringBuilderBase EffectiveDomain;

	FDerivedDataCacheUsageStats UsageStats;
	FBackendDebugOptions DebugOptions;
	THttpUniquePtr<IHttpConnectionPool> ConnectionPool;
	FHttpRequestQueue GetRequestQueues[2];
	FHttpRequestQueue PutRequestQueues[2];
	FHttpRequestQueue NonBlockingRequestQueue;

	FCriticalSection AccessCs;
	TUniquePtr<FHttpAccessToken> Access;
	FTSTicker::FDelegateHandle RefreshAccessTokenHandle;
	double RefreshAccessTokenTime = 0.0;
	uint32 FailedLoginAttempts = 0;

	bool bIsUsable = false;
	bool bReadOnly = false;

	static inline FHttpCacheStore* AnyInstance = nullptr;

	FHttpClientParams GetDefaultClientParams() const;

	THttpUniquePtr<IHttpResponse> BeginIsServiceReady(IHttpClient& Client, TArray64<uint8>& Body);
	bool EndIsServiceReady(THttpUniquePtr<IHttpResponse>& Response, TArray64<uint8>& Body);
	bool AcquireAccessToken(IHttpClient* Client = nullptr);
	void SetAccessToken(FStringView Token, double RefreshDelay = 0.0);

	enum class EOperationCategory
	{
		Get,
		Put,
	};

	class FHttpOperation;

	TUniquePtr<FHttpOperation> WaitForHttpOperation(EOperationCategory Category, bool bUnboundedOverflow);

	struct FGetCacheRecordOnlyResponse
	{
		FSharedString Name;
		FCacheKey Key;
		uint64 UserData = 0;
		uint64 BytesReceived = 0;
		FOptionalCacheRecord Record;
		EStatus Status = EStatus::Error;
	};
	using FOnGetCacheRecordOnlyComplete = TUniqueFunction<void(FGetCacheRecordOnlyResponse&& Response)>;
	void GetCacheRecordOnlyAsync(
		IRequestOwner& Owner,
		const FSharedString& Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy,
		uint64 UserData,
		FOnGetCacheRecordOnlyComplete&& OnComplete);

	void GetCacheRecordAsync(
		IRequestOwner& Owner,
		const FSharedString& Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy,
		uint64 UserData,
		TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& OnComplete);

	void PutCacheRecordAsync(
		IRequestOwner& Owner,
		const FSharedString& Name,
		const FCacheRecord& Record,
		const FCacheRecordPolicy& Policy,
		uint64 UserData,
		TUniqueFunction<void(FCachePutResponse&& Response, uint64 BytesSent)>&& OnComplete);

	void PutCacheValueAsync(
		IRequestOwner& Owner,
		const FSharedString& Name,
		const FCacheKey& Key,
		const FValue& Value,
		ECachePolicy Policy,
		uint64 UserData,
		TUniqueFunction<void(FCachePutValueResponse&& Response, uint64 BytesSent)>&& OnComplete);

	void GetCacheValueAsync(
		IRequestOwner& Owner,
		FSharedString Name,
		const FCacheKey& Key,
		ECachePolicy Policy,
		uint64 UserData,
		FOnCacheGetValueComplete&& OnComplete);

	void RefCachedDataProbablyExistsBatchAsync(
		IRequestOwner& Owner,
		TConstArrayView<FCacheGetValueRequest> ValueRefs,
		FOnCacheGetValueComplete&& OnComplete);

	class FHealthCheckOp;
	class FPutPackageOp;
	class FGetRecordOp;
};

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore::FHttpOperation
//----------------------------------------------------------------------------------------------------------
class FHttpCacheStore::FHttpOperation final
{
public:
	FHttpOperation(const FHttpOperation&) = delete;
	FHttpOperation& operator=(const FHttpOperation&) = delete;

	explicit FHttpOperation(THttpUniquePtr<IHttpRequest>&& InRequest)
		: Request(MoveTemp(InRequest))
	{
	}

	// Prepare Request

	void SetUri(FAnsiStringView Uri) { Request->SetUri(Uri); }
	void SetMethod(EHttpMethod Method) { Request->SetMethod(Method); }
	void AddHeader(FAnsiStringView Name, FAnsiStringView Value) { Request->AddHeader(Name, Value); }
	void SetBody(const FCompositeBuffer& Body) { Request->SetBody(Body); }
	void SetContentType(EHttpMediaType Type) { Request->SetContentType(Type); }
	void AddAcceptType(EHttpMediaType Type) { Request->AddAcceptType(Type); }
	void SetExpectedErrorCodes(TConstArrayView<int32> Codes) { ExpectedErrorCodes = Codes; }

	// Send Request

	void Send();
	void SendAsync(IRequestOwner& Owner, TUniqueFunction<void ()>&& OnComplete);

	// Consume Response

	int32 GetStatusCode() const { return Response->GetStatusCode(); }
	EHttpErrorCode GetErrorCode() const { return Response->GetErrorCode(); }
	EHttpMediaType GetContentType() const { return Response->GetContentType(); }
	FAnsiStringView GetHeader(FAnsiStringView Name) const { return Response->GetHeader(Name); }
	FSharedBuffer GetBody() const { return ResponseBody; }
	FString GetBodyAsString() const;
	TSharedPtr<FJsonObject> GetBodyAsJson() const;
	uint64 GetBytesSent() const { return Response->GetStats().SendSize; }
	uint64 GetBytesReceived() const { return Response->GetStats().RecvSize; }

	friend FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FHttpOperation& Operation)
	{
		check(Operation.Response);
		return Builder << *Operation.Response;
	}

private:
	class FHttpOperationReceiver;
	class FAsyncHttpOperationReceiver;

	FSharedBuffer ResponseBody;
	THttpUniquePtr<IHttpRequest> Request;
	THttpUniquePtr<IHttpResponse> Response;
	TArray<int32, TInlineAllocator<4>> ExpectedErrorCodes;
	uint32 AttemptCount = 0;
};

class FHttpCacheStore::FHttpOperation::FHttpOperationReceiver final : public IHttpReceiver
{
public:
	FHttpOperationReceiver(const FHttpOperationReceiver&) = delete;
	FHttpOperationReceiver& operator=(const FHttpOperationReceiver&) = delete;

	explicit FHttpOperationReceiver(FHttpOperation* InOperation, IHttpReceiver* InNext = nullptr)
		: Operation(InOperation)
		, Next(InNext)
		, BodyReceiver(BodyArray, this)
	{
	}

	FHttpOperation* GetOperation() const { return Operation; }

private:
	IHttpReceiver* OnCreate(IHttpResponse& LocalResponse) final
	{
		++Operation->AttemptCount;
		return &BodyReceiver;
	}

	IHttpReceiver* OnComplete(IHttpResponse& LocalResponse) final
	{
		Operation->ResponseBody = MakeSharedBufferFromArray(MoveTemp(BodyArray));

		LogResponse(LocalResponse);

		if (!ShouldRetry(LocalResponse))
		{
			Operation->Request.Reset();
		}

		return Next;
	}

	bool ShouldRetry(IHttpResponse& LocalResponse) const
	{
		if (Operation->AttemptCount >= UE_HTTPDDC_MAX_ATTEMPTS || ShouldAbortForShutdown())
		{
			return false;
		}

		if (LocalResponse.GetErrorCode() == EHttpErrorCode::TimedOut)
		{
			return true;
		}

		// Too many requests, make a new attempt.
		if (LocalResponse.GetStatusCode() == 429)
		{
			return true;
		}

		return false;
	}

	void LogResponse(IHttpResponse& LocalResponse) const
	{
		if (UE_LOG_ACTIVE(LogDerivedDataCache, Display))
		{
			const int32 StatusCode = LocalResponse.GetStatusCode();
			const bool bVerbose = (StatusCode >= 200 && StatusCode < 300) || Operation->ExpectedErrorCodes.Contains(StatusCode);

			TStringBuilder<80> StatsText;
			if (!bVerbose || UE_LOG_ACTIVE(LogDerivedDataCache, Verbose))
			{
				const FHttpResponseStats& Stats = LocalResponse.GetStats();
				if (Stats.SendSize)
				{
					StatsText << TEXTVIEW("sent ") << Stats.SendSize << TEXTVIEW(" bytes, ");
				}
				if (Stats.RecvSize)
				{
					StatsText << TEXTVIEW("received ") << Stats.RecvSize << TEXTVIEW(" bytes, ");
				}
				StatsText.Appendf(TEXT("%.3f seconds"), Stats.TotalTime);
			}

			if (bVerbose)
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("HTTP: %s (%s)"), *WriteToString<256>(LocalResponse), *StatsText);
			}
			else
			{
				FString Body = Operation->GetBodyAsString();
				Body.ReplaceCharInline(TEXT('\r'), TEXT(' '));
				Body.ReplaceCharInline(TEXT('\n'), TEXT(' '));
				UE_LOG(LogDerivedDataCache, Display,
					TEXT("HTTP: %s (%s) %s"), *WriteToString<256>(LocalResponse), *StatsText, *Body);
			}
		}
	}

private:
	FHttpOperation* Operation;
	IHttpReceiver* Next;
	TArray64<uint8> BodyArray;
	FHttpByteArrayReceiver BodyReceiver{BodyArray, this};
};

class FHttpCacheStore::FHttpOperation::FAsyncHttpOperationReceiver final : public FRequestBase, public IHttpReceiver
{
public:
	FAsyncHttpOperationReceiver(const FAsyncHttpOperationReceiver&) = delete;
	FAsyncHttpOperationReceiver& operator=(const FAsyncHttpOperationReceiver&) = delete;

	FAsyncHttpOperationReceiver(FHttpOperation* InOperation, IRequestOwner* InOwner, TUniqueFunction<void ()>&& InOperationComplete)
		: Owner(InOwner)
		, BaseReceiver(InOperation, this)
		, OperationComplete(MoveTemp(InOperationComplete))
	{
		LLM_IF_ENABLED(MemTag = FLowLevelMemTracker::Get().GetActiveTagData(ELLMTracker::Default));
	}

private:
	// IRequest Interface

	void SetPriority(EPriority Priority) final {}
	void Cancel() final { Monitor->Cancel(); }
	void Wait() final { Monitor->Wait(); }

	// IHttpReceiver Interface

	IHttpReceiver* OnCreate(IHttpResponse& LocalResponse) final
	{
		LLM_SCOPE(MemTag);
		Monitor = LocalResponse.GetMonitor();
		Owner->Begin(this);
		return &BaseReceiver;
	}

	IHttpReceiver* OnComplete(IHttpResponse& LocalResponse) final
	{
		LLM_SCOPE(MemTag);
		Owner->End(this, [Self = this]
		{
			FHttpOperation* Operation = Self->BaseReceiver.GetOperation();
			if (IHttpRequest* LocalRequest = Operation->Request.Get())
			{
				// Retry as indicated by the request not being reset.
				TRefCountPtr<FAsyncHttpOperationReceiver> Receiver = new FAsyncHttpOperationReceiver(Operation, Self->Owner, MoveTemp(Self->OperationComplete));
				LocalRequest->SendAsync(Receiver, Operation->Response);
			}
			else if (Self->OperationComplete)
			{
				// Launch a task for the completion function since it can execute arbitrary code.
				Self->Owner->LaunchTask(TEXT("HttpOperationComplete"), [Self = TRefCountPtr(Self)]
				{
					Self->OperationComplete();
				});
			}
		});
		return nullptr;
	}

private:
	IRequestOwner* Owner;
	FHttpOperationReceiver BaseReceiver;
	TUniqueFunction<void ()> OperationComplete;
	TRefCountPtr<IHttpResponseMonitor> Monitor;
	LLM(const UE::LLMPrivate::FTagData* MemTag = nullptr);
};

void FHttpCacheStore::FHttpOperation::Send()
{
	FHttpOperationReceiver Receiver(this);
	do
	{
		Request->Send(&Receiver, Response);
	}
	while (Request);
}

void FHttpCacheStore::FHttpOperation::SendAsync(IRequestOwner& Owner, TUniqueFunction<void ()>&& OnComplete)
{
	TRefCountPtr<FAsyncHttpOperationReceiver> Receiver = new FAsyncHttpOperationReceiver(this, &Owner, MoveTemp(OnComplete));
	Request->SendAsync(Receiver, Response);
}

FString FHttpCacheStore::FHttpOperation::GetBodyAsString() const
{
	static_assert(sizeof(uint8) == sizeof(UTF8CHAR));
	const int32 Len = IntCastChecked<int32>(ResponseBody.GetSize());
	return FString(Len, (const UTF8CHAR*)ResponseBody.GetData());
}

TSharedPtr<FJsonObject> FHttpCacheStore::FHttpOperation::GetBodyAsJson() const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(GetBodyAsString());
	FJsonSerializer::Deserialize(JsonReader, JsonObject);
	return JsonObject;
}

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore::FHealthCheckOp
//----------------------------------------------------------------------------------------------------------
class FHttpCacheStore::FHealthCheckOp final
{
public:
	FHealthCheckOp(FHttpCacheStore& CacheStore, IHttpClient& Client)
		: Operation(Client.TryCreateRequest({}))
		, Owner(EPriority::High)
		, Domain(*CacheStore.Domain)
	{
		Operation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/health/ready")));
		Operation.SendAsync(Owner, []{});
	}

	bool IsReady()
	{
		Owner.Wait();
		const FString Body = Operation.GetBodyAsString();
		if (Operation.GetStatusCode() == 200)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: HTTP DDC: %s"), Domain, *Body);
			return true;
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Unable to reach HTTP DDC at %s. %s"),
				Domain, *WriteToString<256>(Operation), *Body);
			return false;
		}
	}

private:
	FHttpOperation Operation;
	FRequestOwner Owner;
	const TCHAR* Domain;
};

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore::FPutPackageOp
//----------------------------------------------------------------------------------------------------------
class FHttpCacheStore::FPutPackageOp final : public FThreadSafeRefCountedObject
{
public:

	struct FCachePutPackageResponse
	{
		FSharedString Name;
		FCacheKey Key;
		uint64 UserData = 0;
		uint64 BytesSent = 0;
		EStatus Status = EStatus::Error;
	};
	using FOnCachePutPackageComplete = TUniqueFunction<void(FCachePutPackageResponse&& Response)>;

	/** Performs a multi-request operation for uploading a package of content. */
	static void PutPackage(
		FHttpCacheStore& CacheStore,
		IRequestOwner& Owner,
		const FSharedString& Name,
		FCacheKey Key,
		FCbPackage&& Package,
		FCacheRecordPolicy Policy,
		uint64 UserData,
		FOnCachePutPackageComplete&& OnComplete);

private:
	FHttpCacheStore& CacheStore;
	IRequestOwner& Owner;
	const FSharedString Name;
	const FCacheKey Key;
	const uint64 UserData;
	std::atomic<uint64> BytesSent;
	const FCbObject PackageObject;
	const FIoHash PackageObjectHash;
	const uint32 TotalBlobUploads;
	std::atomic<uint32> SuccessfulBlobUploads;
	std::atomic<uint32> PendingBlobUploads;
	FOnCachePutPackageComplete OnComplete;

	struct FCachePutRefResponse
	{
		FSharedString Name;
		FCacheKey Key;
		uint64 UserData = 0;
		uint64 BytesSent = 0;
		TConstArrayView<FIoHash> NeededBlobHashes;
		EStatus Status = EStatus::Error;
	};
	using FOnCachePutRefComplete = TUniqueFunction<void(FCachePutRefResponse&& Response)>;

	FPutPackageOp(
		FHttpCacheStore& InCacheStore,
		IRequestOwner& InOwner,
		const FSharedString& InName,
		const FCacheKey& InKey,
		uint64 InUserData,
		uint64 InBytesSent,
		const FCbObject& InPackageObject,
		const FIoHash& InPackageObjectHash,
		uint32 InTotalBlobUploads,
		FOnCachePutPackageComplete&& InOnComplete);

	static void PutRefAsync(
		FHttpCacheStore& CacheStore,
		IRequestOwner& Owner,
		const FSharedString& Name,
		FCacheKey Key,
		FCbObject Object,
		FIoHash ObjectHash,
		uint64 UserData,
		bool bFinalize,
		FOnCachePutRefComplete&& OnComplete);

	static void OnPackagePutRefComplete(
		FHttpCacheStore& CacheStore,
		IRequestOwner& Owner,
		const FSharedString& Name,
		const FCacheKey& Key,
		FCbPackage&& Package,
		FCacheRecordPolicy Policy,
		uint64 UserData,
		FOnCachePutPackageComplete&& OnComplete,
		FCachePutRefResponse&& Response);

	void OnCompressedBlobUploadComplete(FHttpOperation& Operation);

	void OnPutRefFinalizationComplete(FCachePutRefResponse&& Response);

	FCachePutPackageResponse MakeResponse(uint64 InBytesSent, EStatus Status)
	{
		return FCachePutPackageResponse{ Name, Key, UserData, InBytesSent, Status };
	};
};

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore::FGetRecordOp
//----------------------------------------------------------------------------------------------------------
class FHttpCacheStore::FGetRecordOp final : public FThreadSafeRefCountedObject
{
public:
	/** Performs a multi-request operation for downloading a record. */
	static void GetRecord(
		FHttpCacheStore& CacheStore,
		IRequestOwner& Owner,
		const FSharedString& Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy,
		uint64 UserData,
		TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& OnComplete);

	struct FGetCachedDataBatchResponse
	{
		FSharedString Name;
		FCacheKey Key;
		int32 ValueIndex;
		uint64 BytesReceived = 0;
		FCompressedBuffer DataBuffer;
		EStatus Status = EStatus::Error;
	};
	using FOnGetCachedDataBatchComplete = TUniqueFunction<void(FGetCachedDataBatchResponse&& Response)>;

	/** Utility method for fetching a batch of value data. */
	template <typename ValueType, typename ValueIdGetterType>
	static void GetDataBatch(
		FHttpCacheStore& CacheStore,
		IRequestOwner& Owner,
		FSharedString Name,
		const FCacheKey& Key,
		TConstArrayView<ValueType> Values,
		ValueIdGetterType ValueIdGetter,
		FOnGetCachedDataBatchComplete&& OnComplete);
private:
	FHttpCacheStore& CacheStore;
	IRequestOwner& Owner;
	const FSharedString Name;
	const FCacheKey Key;
	const uint64 UserData;
	std::atomic<uint64> BytesReceived;
	TArray<FCompressedBuffer> FetchedBuffers;
	const TArray<FValueWithId> RequiredGets;
	const TArray<FValueWithId> RequiredHeads;
	FCacheRecordBuilder RecordBuilder;
	const uint32 TotalOperations;
	std::atomic<uint32> SuccessfulOperations;
	std::atomic<uint32> PendingOperations;
	TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)> OnComplete;

	FGetRecordOp(
		FHttpCacheStore& InCacheStore,
		IRequestOwner& InOwner,
		const FSharedString& InName,
		const FCacheKey& InKey,
		uint64 InUserData,
		uint64 InBytesReceived,
		TArray<FValueWithId>&& InRequiredGets,
		TArray<FValueWithId>&& InRequiredHeads,
		FCacheRecordBuilder&& InRecordBuilder,
		TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& InOnComplete);

	static void OnOnlyRecordComplete(
		FHttpCacheStore& CacheStore,
		IRequestOwner& Owner,
		const FCacheRecordPolicy& Policy,
		TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& OnComplete,
		FGetCacheRecordOnlyResponse&& Response);

	struct FCachedDataProbablyExistsBatchResponse
	{
		FSharedString Name;
		FCacheKey Key;
		int32 ValueIndex;
		EStatus Status = EStatus::Error;
	};
	using FOnCachedDataProbablyExistsBatchComplete = TUniqueFunction<void(FCachedDataProbablyExistsBatchResponse&& Response)>;
	void DataProbablyExistsBatch(
		TConstArrayView<FValueWithId> Values,
		FOnCachedDataProbablyExistsBatchComplete&& OnComplete);

	void FinishDataStep(bool bSuccess, uint64 InBytesReceived);
};

void FHttpCacheStore::FPutPackageOp::PutPackage(
	FHttpCacheStore& CacheStore,
	IRequestOwner& Owner,
	const FSharedString& Name,
	FCacheKey Key,
	FCbPackage&& Package,
	FCacheRecordPolicy Policy,
	uint64 UserData,
	FOnCachePutPackageComplete&& OnComplete)
{
	// TODO: Jupiter currently always overwrites.  It doesn't have a "write if not present" feature (for records or attachments),
	//		 but would require one to implement all policy correctly.

	// Initial record upload
	PutRefAsync(CacheStore, Owner, Name, Key, Package.GetObject(), Package.GetObjectHash(), UserData, false,
		[&CacheStore, &Owner, Name = FSharedString(Name), Key, Package = MoveTemp(Package), Policy, UserData, OnComplete = MoveTemp(OnComplete)](FCachePutRefResponse&& Response) mutable
		{
			return OnPackagePutRefComplete(CacheStore, Owner, Name, Key, MoveTemp(Package), Policy, UserData, MoveTemp(OnComplete), MoveTemp(Response));
		});
}

FHttpCacheStore::FPutPackageOp::FPutPackageOp(
	FHttpCacheStore& InCacheStore,
	IRequestOwner& InOwner,
	const FSharedString& InName,
	const FCacheKey& InKey,
	uint64 InUserData,
	uint64 InBytesSent,
	const FCbObject& InPackageObject,
	const FIoHash& InPackageObjectHash,
	uint32 InTotalBlobUploads,
	FOnCachePutPackageComplete&& InOnComplete)
	: CacheStore(InCacheStore)
	, Owner(InOwner)
	, Name(InName)
	, Key(InKey)
	, UserData(InUserData)
	, BytesSent(InBytesSent)
	, PackageObject(InPackageObject)
	, PackageObjectHash(InPackageObjectHash)
	, TotalBlobUploads(InTotalBlobUploads)
	, SuccessfulBlobUploads(0)
	, PendingBlobUploads(InTotalBlobUploads)
	, OnComplete(MoveTemp(InOnComplete))
{
}

void FHttpCacheStore::FPutPackageOp::PutRefAsync(
	FHttpCacheStore& CacheStore,
	IRequestOwner& Owner,
	const FSharedString& Name,
	FCacheKey Key,
	FCbObject Object,
	FIoHash ObjectHash,
	uint64 UserData,
	bool bFinalize,
	FOnCachePutRefComplete&& OnComplete)
{
	TAnsiStringBuilder<64> Bucket;
	Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);

	TAnsiStringBuilder<256> RefsUri;
	RefsUri << CacheStore.EffectiveDomain << ANSITEXTVIEW("/api/v1/refs/") << CacheStore.Namespace << '/' << Bucket << '/' << Key.Hash;
	if (bFinalize)
	{
		RefsUri << ANSITEXTVIEW("/finalize/") << ObjectHash;
	}

	TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Put, /*bUnboundedOverflow*/ bFinalize);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(RefsUri);
	if (bFinalize)
	{
		LocalOperation.SetMethod(EHttpMethod::Post);
		LocalOperation.SetContentType(EHttpMediaType::FormUrlEncoded);
	}
	else
	{
		LocalOperation.SetMethod(EHttpMethod::Put);
		LocalOperation.SetContentType(EHttpMediaType::CbObject);
		LocalOperation.AddHeader(ANSITEXTVIEW("X-Jupiter-IoHash"), WriteToAnsiString<48>(ObjectHash));
		LocalOperation.SetBody(Object.GetBuffer());
	}
	LocalOperation.AddAcceptType(EHttpMediaType::Json);
	LocalOperation.SendAsync(Owner, [Operation = MoveTemp(Operation), &CacheStore, Name, Key, Object, UserData, bFinalize, OnComplete = MoveTemp(OnComplete)]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_PutRefAsync_OnHttpRequestComplete);

		const int32 StatusCode = Operation->GetStatusCode();
		if (StatusCode >= 200 && StatusCode <= 204)
		{
			TArray<FIoHash> NeededBlobHashes;

			// Useful when debugging issues related to compressed/uncompressed blobs being returned from Jupiter
			const bool bPutRefBlobsAlways = false;

			if (bPutRefBlobsAlways && !bFinalize)
			{
				Object.IterateAttachments([&NeededBlobHashes](FCbFieldView AttachmentFieldView)
				{
					FIoHash AttachmentHash = AttachmentFieldView.AsHash();
					if (!AttachmentHash.IsZero())
					{
						NeededBlobHashes.Add(AttachmentHash);
					}
				});
			}
			else if (TSharedPtr<FJsonObject> ResponseObject = Operation->GetBodyAsJson())
			{
				TArray<FString> NeedsArrayStrings;
				ResponseObject->TryGetStringArrayField(TEXT("needs"), NeedsArrayStrings);

				NeededBlobHashes.Reserve(NeedsArrayStrings.Num());
				for (const FString& NeededString : NeedsArrayStrings)
				{
					FIoHash BlobHash;
					LexFromString(BlobHash, *NeededString);
					if (!BlobHash.IsZero())
					{
						NeededBlobHashes.Add(BlobHash);
					}
				}
			}

			OnComplete({ Name, Key, UserData, Operation->GetBytesSent(), NeededBlobHashes, EStatus::Ok });
		}
		else
		{
			const EStatus Status = Operation->GetErrorCode() == EHttpErrorCode::Canceled ? EStatus::Canceled : EStatus::Error;
			OnComplete({ Name, Key, UserData, Operation->GetBytesSent(), {}, Status });
		}
	});
}

void FHttpCacheStore::FPutPackageOp::OnPackagePutRefComplete(
	FHttpCacheStore& CacheStore,
	IRequestOwner& Owner,
	const FSharedString& Name,
	const FCacheKey& Key,
	FCbPackage&& Package,
	FCacheRecordPolicy Policy,
	uint64 UserData,
	FOnCachePutPackageComplete&& OnComplete,
	FCachePutRefResponse&& Response)
{
	if (Response.Status != EStatus::Ok)
	{
		if (Response.Status == EStatus::Error)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Failed to put reference object for put of %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Response.Key), *Response.Name);
		}
		return OnComplete(FCachePutPackageResponse{ Name, Key, UserData, Response.BytesSent, Response.Status });
	}

	struct FCompressedBlobUpload
	{
		FIoHash Hash;
		FSharedBuffer BlobBuffer;
		FCompressedBlobUpload(const FIoHash& InHash, FSharedBuffer&& InBlobBuffer) : Hash(InHash), BlobBuffer(InBlobBuffer)
		{
		}
	};

	TArray<FCompressedBlobUpload> CompressedBlobUploads;

	// TODO: blob uploading and finalization should be replaced with a single batch compressed blob upload endpoint in the future.
	TStringBuilder<128> ExpectedHashes;
	bool bExpectedHashesSerialized = false;

	// Needed blob upload (if any missing)
	for (const FIoHash& NeededBlobHash : Response.NeededBlobHashes)
	{
		if (const FCbAttachment* Attachment = Package.FindAttachment(NeededBlobHash))
		{
			FSharedBuffer TempBuffer;
			if (Attachment->IsCompressedBinary())
			{
				TempBuffer = Attachment->AsCompressedBinary().GetCompressed().ToShared();
			}
			else if (Attachment->IsBinary())
			{
				TempBuffer = FValue::Compress(Attachment->AsCompositeBinary()).GetData().GetCompressed().ToShared();
			}
			else
			{
				TempBuffer = FValue::Compress(Attachment->AsObject().GetBuffer()).GetData().GetCompressed().ToShared();
			}

			CompressedBlobUploads.Emplace(NeededBlobHash, MoveTemp(TempBuffer));
		}
		else
		{
			if (!bExpectedHashesSerialized)
			{
				bool bFirstHash = true;
				for (const FCbAttachment& PackageAttachment : Package.GetAttachments())
				{
					if (!bFirstHash)
					{
						ExpectedHashes << TEXT(", ");
					}
					ExpectedHashes << PackageAttachment.GetHash();
					bFirstHash = false;
				}
				bExpectedHashesSerialized = true;
			}
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Server reported needed hash '%s' that is outside the set of expected hashes (%s) for put of %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(NeededBlobHash), ExpectedHashes.ToString(), *WriteToString<96>(Response.Key), *Response.Name);
		}
	}

	if (CompressedBlobUploads.IsEmpty())
	{
		// No blobs need to be uploaded.  No finalization necessary.
		return OnComplete(FCachePutPackageResponse{ Name, Key, UserData, Response.BytesSent, EStatus::Ok });
	}

	// Having this be a ref ensures we don't have the op reach 0 ref count as we queue up multiple operations which MAY execute synchronously
	TRefCountPtr<FPutPackageOp> PutPackageOp = new FPutPackageOp(
		CacheStore,
		Owner,
		Response.Name,
		Response.Key,
		Response.UserData,
		Response.BytesSent,
		Package.GetObject(),
		Package.GetObjectHash(),
		(uint32)CompressedBlobUploads.Num(),
		MoveTemp(OnComplete)
	);

	FRequestBarrier Barrier(Owner);
	for (const FCompressedBlobUpload& CompressedBlobUpload : CompressedBlobUploads)
	{
		TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Put, /*bUnboundedOverflow*/ true);
		FHttpOperation& LocalOperation = *Operation;
		LocalOperation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/api/v1/compressed-blobs/"), CacheStore.Namespace, '/', CompressedBlobUpload.Hash));
		LocalOperation.SetMethod(EHttpMethod::Put);
		LocalOperation.SetContentType(EHttpMediaType::CompressedBinary);
		LocalOperation.SetBody(FCompositeBuffer(CompressedBlobUpload.BlobBuffer));
		LocalOperation.SendAsync(Owner, [Operation = MoveTemp(Operation), PutPackageOp]
		{
			PutPackageOp->OnCompressedBlobUploadComplete(*Operation);
		});
	}
}

void FHttpCacheStore::FPutPackageOp::OnCompressedBlobUploadComplete(FHttpOperation& Operation)
{
	BytesSent.fetch_add(Operation.GetBytesSent(), std::memory_order_relaxed);

	const int32 StatusCode = Operation.GetStatusCode();
	if (StatusCode >= 200 && StatusCode <= 204)
	{
		SuccessfulBlobUploads.fetch_add(1, std::memory_order_relaxed);
	}

	if (PendingBlobUploads.fetch_sub(1, std::memory_order_relaxed) == 1)
	{
		const uint32 LocalSuccessfulBlobUploads = SuccessfulBlobUploads.load(std::memory_order_relaxed);
		if (Owner.IsCanceled())
		{
			OnComplete(MakeResponse(BytesSent.load(std::memory_order_relaxed), EStatus::Canceled));
		}
		else if (LocalSuccessfulBlobUploads == TotalBlobUploads)
		{
			// Perform finalization
			PutRefAsync(CacheStore, Owner, Name, Key, PackageObject, PackageObjectHash, UserData, /*bFinalize*/ true,
				[PutPackageOp = TRefCountPtr<FPutPackageOp>(this)](FCachePutRefResponse&& Response)
				{
					return PutPackageOp->OnPutRefFinalizationComplete(MoveTemp(Response));
				});
		}
		else
		{
			const uint32 FailedBlobUploads = TotalBlobUploads - LocalSuccessfulBlobUploads;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Failed to put %d/%d blobs for put of %s from '%s'"),
				*CacheStore.Domain, FailedBlobUploads, TotalBlobUploads, *WriteToString<96>(Key), *Name);
			OnComplete(MakeResponse(BytesSent.load(std::memory_order_relaxed), EStatus::Error));
		}
	}
}

void FHttpCacheStore::FPutPackageOp::OnPutRefFinalizationComplete(FCachePutRefResponse&& Response)
{
	BytesSent.fetch_add(Response.BytesSent, std::memory_order_relaxed);

	if (Response.Status == EStatus::Error)
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Failed to finalize reference object for put of %s from '%s'"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
	}

	return OnComplete(MakeResponse(BytesSent.load(std::memory_order_relaxed), Response.Status));
}

void FHttpCacheStore::FGetRecordOp::GetRecord(
	FHttpCacheStore& CacheStore,
	IRequestOwner& Owner,
	const FSharedString& Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy,
	uint64 UserData,
	TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& OnComplete)
{
	CacheStore.GetCacheRecordOnlyAsync(Owner, Name, Key, Policy, UserData, [&CacheStore, &Owner, Policy = FCacheRecordPolicy(Policy), OnComplete = MoveTemp(OnComplete)](FGetCacheRecordOnlyResponse&& Response) mutable
	{
		OnOnlyRecordComplete(CacheStore, Owner, Policy, MoveTemp(OnComplete), MoveTemp(Response));
	});
}

template <typename ValueType, typename ValueIdGetterType>
void FHttpCacheStore::FGetRecordOp::GetDataBatch(
	FHttpCacheStore& CacheStore,
	IRequestOwner& Owner,
	FSharedString Name,
	const FCacheKey& Key,
	TConstArrayView<ValueType> Values,
	ValueIdGetterType ValueIdGetter,
	FOnGetCachedDataBatchComplete&& OnComplete)
{
	if (Values.IsEmpty())
	{
		return;
	}

	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnGetCachedDataBatchComplete> SharedOnComplete = MakeShared<FOnGetCachedDataBatchComplete>(MoveTemp(OnComplete));
	for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
	{
		const ValueType Value = Values[ValueIndex].RemoveData();
		TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Get, /*bUnboundedOverflow*/ true);
		FHttpOperation& LocalOperation = *Operation;
		LocalOperation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/api/v1/compressed-blobs/"), CacheStore.Namespace, '/', Value.GetRawHash()));
		LocalOperation.SetMethod(EHttpMethod::Get);
		LocalOperation.AddAcceptType(EHttpMediaType::Any);
		LocalOperation.SetExpectedErrorCodes({404});
		LocalOperation.SendAsync(Owner, [Operation = MoveTemp(Operation), &CacheStore, Name, Key, Value, ValueIndex, ValueIdGetter, SharedOnComplete]
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetDataBatch_OnHttpRequestComplete);

			bool bHit = false;
			FCompressedBuffer CompressedBuffer;
			if (Operation->GetStatusCode() == 200)
			{
				switch (Operation->GetContentType())
				{
				case EHttpMediaType::Any:
				case EHttpMediaType::CompressedBinary:
					CompressedBuffer = FCompressedBuffer::FromCompressed(Operation->GetBody());
					bHit = true;
					break;
				case EHttpMediaType::Binary:
					CompressedBuffer = FValue::Compress(Operation->GetBody()).GetData();
					bHit = true;
					break;
				default:
					break;
				}
			}

			if (bHit)
			{
				if (CompressedBuffer.GetRawHash() == Value.GetRawHash())
				{
					SharedOnComplete.Get()({ Name, Key, ValueIndex, Operation->GetBytesReceived(), MoveTemp(CompressedBuffer), EStatus::Ok });
				}
				else
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Cache miss with corrupted value %s with hash %s for %s from '%s'"),
						*CacheStore.Domain, *ValueIdGetter(Value), *WriteToString<48>(Value.GetRawHash()),
						*WriteToString<96>(Key), *Name);
					SharedOnComplete.Get()({ Name, Key, ValueIndex, Operation->GetBytesReceived(), {}, EStatus::Error });
				}
			}
			else if (Operation->GetErrorCode() == EHttpErrorCode::Canceled)
			{
				SharedOnComplete.Get()({ Name, Key, ValueIndex, Operation->GetBytesReceived(), {}, EStatus::Canceled });
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Verbose,
					TEXT("%s: Cache miss with missing value %s with hash %s for %s from '%s'"),
					*CacheStore.Domain, *ValueIdGetter(Value), *WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Key),
					*Name);
				SharedOnComplete.Get()({ Name, Key, ValueIndex, Operation->GetBytesReceived(), {}, EStatus::Error });
			}
		});
	}
}

FHttpCacheStore::FGetRecordOp::FGetRecordOp(
	FHttpCacheStore& InCacheStore,
	IRequestOwner& InOwner,
	const FSharedString& InName,
	const FCacheKey& InKey,
	uint64 InUserData,
	uint64 InBytesReceived,
	TArray<FValueWithId>&& InRequiredGets,
	TArray<FValueWithId>&& InRequiredHeads,
	FCacheRecordBuilder&& InRecordBuilder,
	TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& InOnComplete)
	: CacheStore(InCacheStore)
	, Owner(InOwner)
	, Name(InName)
	, Key(InKey)
	, UserData(InUserData)
	, BytesReceived(InBytesReceived)
	, RequiredGets(MoveTemp(InRequiredGets))
	, RequiredHeads(MoveTemp(InRequiredHeads))
	, RecordBuilder(MoveTemp(InRecordBuilder))
	, TotalOperations(RequiredGets.Num() + RequiredHeads.Num())
	, SuccessfulOperations(0)
	, PendingOperations(TotalOperations)
	, OnComplete(MoveTemp(InOnComplete))
{
	FetchedBuffers.AddDefaulted(RequiredGets.Num());
}

void FHttpCacheStore::FGetRecordOp::OnOnlyRecordComplete(
	FHttpCacheStore& CacheStore,
	IRequestOwner& Owner,
	const FCacheRecordPolicy& Policy,
	TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& OnComplete,
	FGetCacheRecordOnlyResponse&& Response)
{
	FCacheRecordBuilder RecordBuilder(Response.Key);
	if (Response.Status != EStatus::Ok)
	{
		return OnComplete({ Response.Name, RecordBuilder.Build(), Response.UserData, Response.Status }, Response.BytesReceived);
	}

	if (!EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::SkipMeta))
	{
		RecordBuilder.SetMeta(FCbObject(Response.Record.Get().GetMeta()));
	}

	// TODO: There is not currently a batched GET endpoint for Jupiter.  Once there is, all payload data should be fetched in one call.
	//		 In the meantime, we try to keep the code structured in a way that is friendly to future batching of GETs.

	TArray<FValueWithId> RequiredGets;
	TArray<FValueWithId> RequiredHeads;

	for (FValueWithId Value : Response.Record.Get().GetValues())
	{
		const ECachePolicy ValuePolicy = Policy.GetValuePolicy(Value.GetId());
		if (IsValueDataReady(Value, ValuePolicy))
		{
			RecordBuilder.AddValue(MoveTemp(Value));
		}
		else
		{
			if (EnumHasAnyFlags(ValuePolicy, ECachePolicy::SkipData))
			{
				RequiredHeads.Emplace(Value);
			}
			else
			{
				RequiredGets.Emplace(Value);
			}
		}
	}

	if (RequiredGets.IsEmpty() && RequiredHeads.IsEmpty())
	{
		return OnComplete({ Response.Name, RecordBuilder.Build(), Response.UserData, Response.Status }, Response.BytesReceived);
	}

	// Having this be a ref ensures we don't have the op reach 0 ref count in between the start of the exist batch operation and the get batch operation
	TRefCountPtr<FGetRecordOp> GetRecordOp = new FGetRecordOp(
		CacheStore,
		Owner,
		Response.Name,
		Response.Key,
		Response.UserData,
		Response.BytesReceived,
		MoveTemp(RequiredGets),
		MoveTemp(RequiredHeads),
		MoveTemp(RecordBuilder),
		MoveTemp(OnComplete)
	);

	auto IdGetter = [](const FValueWithId& Value)
	{
		return FString(WriteToString<16>(Value.GetId()));
	};

	{
		FRequestBarrier Barrier(Owner);
		GetRecordOp->DataProbablyExistsBatch(GetRecordOp->RequiredHeads, [GetRecordOp](FCachedDataProbablyExistsBatchResponse&& Response)
		{
			GetRecordOp->FinishDataStep(Response.Status == EStatus::Ok, 0);
		});

		GetDataBatch<FValueWithId>(CacheStore, Owner, Response.Name, Response.Key, GetRecordOp->RequiredGets, IdGetter, [GetRecordOp](FGetCachedDataBatchResponse&& Response)
		{
			GetRecordOp->FetchedBuffers[Response.ValueIndex] = MoveTemp(Response.DataBuffer);
			GetRecordOp->FinishDataStep(Response.Status == EStatus::Ok, Response.BytesReceived);
		});

	}
}

void FHttpCacheStore::FGetRecordOp::DataProbablyExistsBatch(
	TConstArrayView<FValueWithId> Values,
	FOnCachedDataProbablyExistsBatchComplete&& InOnComplete)
{
	if (Values.IsEmpty())
	{
		return;
	}

	TAnsiStringBuilder<256> CompressedBlobsUri;
	CompressedBlobsUri << CacheStore.EffectiveDomain << ANSITEXTVIEW("/api/v1/compressed-blobs/") << CacheStore.Namespace << ANSITEXTVIEW("/exists?");
	bool bFirstItem = true;
	for (const FValueWithId& Value : Values)
	{
		if (!bFirstItem)
		{
			CompressedBlobsUri << '&';
		}
		CompressedBlobsUri << ANSITEXTVIEW("id=") << Value.GetRawHash();
		bFirstItem = false;
	}

	TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Get, /*bUnboundedOverflow*/ true);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(CompressedBlobsUri);
	LocalOperation.SetMethod(EHttpMethod::Post);
	LocalOperation.SetContentType(EHttpMediaType::FormUrlEncoded);
	LocalOperation.AddAcceptType(EHttpMediaType::Json);
	LocalOperation.SendAsync(Owner, [Operation = MoveTemp(Operation), this, Values = TArray<FValueWithId>(Values), InOnComplete = MoveTemp(InOnComplete)]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_DataProbablyExistsBatch_OnHttpRequestComplete);

		const int32 StatusCode = Operation->GetStatusCode();
		if (StatusCode >= 200 && StatusCode <= 204)
		{
			if (TSharedPtr<FJsonObject> ResponseObject = Operation->GetBodyAsJson())
			{
				TArray<FString> NeedsArrayStrings;
				if (ResponseObject->TryGetStringArrayField(TEXT("needs"), NeedsArrayStrings))
				{
					if (NeedsArrayStrings.IsEmpty())
					{
						for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
						{
							const FValueWithId& Value = Values[ValueIndex];
							UE_LOG(LogDerivedDataCache, VeryVerbose,
								TEXT("%s: Cache exists hit for value %s with hash %s for %s from '%s'"),
								*CacheStore.Domain, *WriteToString<16>(Value.GetId()),
								*WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Key), *Name);
							InOnComplete({ Name, Key, ValueIndex, EStatus::Ok });
						}
						return;
					}
				}

				TBitArray<> ResultStatus(true, Values.Num());
				for (const FString& NeedsString : NeedsArrayStrings)
				{
					const FIoHash NeedHash(NeedsString);
					for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
					{
						const FValueWithId& Value = Values[ValueIndex];
						if (ResultStatus[ValueIndex] && NeedHash == Value.GetRawHash())
						{
							ResultStatus[ValueIndex] = false;
							break;
						}
					}
				}

				for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
				{
					const FValueWithId& Value = Values[ValueIndex];

					if (ResultStatus[ValueIndex])
					{
						UE_LOG(LogDerivedDataCache, VeryVerbose,
							TEXT("%s: Cache exists hit for value %s with hash %s for %s from '%s'"),
							*CacheStore.Domain, *WriteToString<32>(Value.GetId()),
							*WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Key), *Name);
						InOnComplete({ Name, Key, ValueIndex, EStatus::Ok });
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Verbose,
							TEXT("%s: Cache exists miss with missing value %s with hash %s for %s from '%s'"),
							*CacheStore.Domain, *WriteToString<32>(Value.GetId()),
							*WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Key), *Name);
						InOnComplete({ Name, Key, ValueIndex, EStatus::Error });
					}
				}
			}
			else
			{
				for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
				{
					UE_LOG(LogDerivedDataCache, Log,
						TEXT("%s: Cache exists miss with invalid response for value %s for %s from '%s'"),
						*CacheStore.Domain, *WriteToString<32>(Values[ValueIndex].GetId()),
						*WriteToString<96>(Key), *Name);
					InOnComplete({ Name, Key, ValueIndex, EStatus::Error });
				}
			}
		}
		else
		{
			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				UE_LOG(LogDerivedDataCache, Verbose,
					TEXT("%s: Cache exists miss with failed response for value %s for %s from '%s'"),
					*CacheStore.Domain, *WriteToString<32>(Values[ValueIndex].GetId()),
					*WriteToString<96>(Key), *Name);
				InOnComplete({Name, Key, ValueIndex, EStatus::Error});
			}
		}
	});
}

void FHttpCacheStore::FGetRecordOp::FinishDataStep(bool bSuccess, uint64 InBytesReceived)
{
	BytesReceived.fetch_add(InBytesReceived, std::memory_order_relaxed);
	if (bSuccess)
	{
		SuccessfulOperations.fetch_add(1, std::memory_order_relaxed);
	}

	if (PendingOperations.fetch_sub(1, std::memory_order_acq_rel) == 1)
	{
		EStatus Status = EStatus::Error;
		uint32 LocalSuccessfulOperations = SuccessfulOperations.load(std::memory_order_relaxed);
		if (LocalSuccessfulOperations == TotalOperations)
		{
			for (int32 Index = 0; Index < RequiredHeads.Num(); ++Index)
			{
				RecordBuilder.AddValue(RequiredHeads[Index].RemoveData());
			}

			for (int32 Index = 0; Index < RequiredGets.Num(); ++Index)
			{
				RecordBuilder.AddValue(FValueWithId(RequiredGets[Index].GetId(), FetchedBuffers[Index]));
			}
			Status = EStatus::Ok;
		}
		OnComplete({Name, RecordBuilder.Build(), UserData, Status}, BytesReceived.load(std::memory_order_relaxed));
	}
}

FHttpCacheStore::FHttpCacheStore(const FHttpCacheStoreParams& Params)
	: Domain(Params.Host)
	, Namespace(Params.Namespace)
	, OAuthProvider(Params.OAuthProvider)
	, OAuthClientId(Params.OAuthClientId)
	, OAuthSecret(Params.OAuthSecret)
	, OAuthScope(Params.OAuthScope)
	, OAuthProviderIdentifier(Params.OAuthProviderIdentifier)
	, OAuthAccessToken(Params.OAuthAccessToken)
	, bReadOnly(Params.bReadOnly)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_Construct);

	EffectiveDomain.Append(Domain);
	TAnsiStringBuilder<256> ResolvedDomain;
	if (Params.bResolveHostCanonicalName && TryResolveCanonicalHost(EffectiveDomain, ResolvedDomain))
	{
		// Store the URI with the canonical name to pin to one region when using DNS-based region selection.
		UE_LOG(LogDerivedDataCache, Display,
			TEXT("%s: Pinned to %hs based on DNS canonical name."), *Domain, *ResolvedDomain);
		EffectiveDomain.Reset();
		EffectiveDomain.Append(ResolvedDomain);
	}

#if WITH_SSL
	if (!Params.HostPinnedPublicKeys.IsEmpty() && EffectiveDomain.ToView().StartsWith(ANSITEXTVIEW("https://")))
	{
		FSslModule::Get().GetCertificateManager().SetPinnedPublicKeys(FString(GetDomainFromUri(EffectiveDomain)), Params.HostPinnedPublicKeys);
	}
	if (!Params.OAuthPinnedPublicKeys.IsEmpty() && OAuthProvider.StartsWith(TEXT("https://")))
	{
		FSslModule::Get().GetCertificateManager().SetPinnedPublicKeys(FString(GetDomainFromUri(WriteToAnsiString<256>(OAuthProvider))), Params.OAuthPinnedPublicKeys);
	}
#endif

	constexpr uint32 MaxTotalConnections = 8;
	FHttpConnectionPoolParams ConnectionPoolParams;
	ConnectionPoolParams.MaxConnections = MaxTotalConnections;
	ConnectionPoolParams.MinConnections = MaxTotalConnections;
	ConnectionPool = IHttpManager::Get().CreateConnectionPool(ConnectionPoolParams);

	FHttpClientParams ClientParams = GetDefaultClientParams();

	THttpUniquePtr<IHttpClient> Client = ConnectionPool->CreateClient(ClientParams);
	FHealthCheckOp HealthCheck(*this, *Client);
	if (AcquireAccessToken(Client.Get()) && HealthCheck.IsReady())
	{
		ClientParams.MaxRequests = UE_HTTPDDC_GET_REQUEST_POOL_SIZE;
		ClientParams.MinRequests = UE_HTTPDDC_GET_REQUEST_POOL_SIZE;
		GetRequestQueues[0] = FHttpRequestQueue(*ConnectionPool, ClientParams);
		GetRequestQueues[1] = FHttpRequestQueue(*ConnectionPool, ClientParams);

		ClientParams.MaxRequests = UE_HTTPDDC_PUT_REQUEST_POOL_SIZE;
		ClientParams.MinRequests = UE_HTTPDDC_PUT_REQUEST_POOL_SIZE;
		PutRequestQueues[0] = FHttpRequestQueue(*ConnectionPool, ClientParams);
		PutRequestQueues[1] = FHttpRequestQueue(*ConnectionPool, ClientParams);

		ClientParams.MaxRequests = UE_HTTPDDC_NONBLOCKING_REQUEST_POOL_SIZE * 2;
		ClientParams.MinRequests = UE_HTTPDDC_NONBLOCKING_REQUEST_POOL_SIZE;
		NonBlockingRequestQueue = FHttpRequestQueue(*ConnectionPool, ClientParams);

		bIsUsable = true;
	}

	AnyInstance = this;
}

FHttpCacheStore::~FHttpCacheStore()
{
	if (RefreshAccessTokenHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RefreshAccessTokenHandle);
	}

	if (AnyInstance == this)
	{
		AnyInstance = nullptr;
	}
}

FHttpClientParams FHttpCacheStore::GetDefaultClientParams() const
{
	FHttpClientParams ClientParams;
	ClientParams.DnsCacheTimeout = 300;
	ClientParams.ConnectTimeout = 30 * 1000;
	ClientParams.LowSpeedLimit = 1024;
	ClientParams.LowSpeedTime = 30;
	ClientParams.TlsLevel = EHttpTlsLevel::All;
	ClientParams.bFollowRedirects = true;
	ClientParams.bFollow302Post = true;
	return ClientParams;
}

bool FHttpCacheStore::LegacyDebugOptions(FBackendDebugOptions& InOptions)
{
	DebugOptions = InOptions;
	return true;
}

bool FHttpCacheStore::AcquireAccessToken(IHttpClient* Client)
{
	if (Domain.StartsWith(TEXT("http://localhost")))
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Skipping authorization for connection to localhost."), *Domain);
		return true;
	}

	// Avoid spamming this if the service is down.
	if (FailedLoginAttempts > UE_HTTPDDC_MAX_FAILED_LOGIN_ATTEMPTS)
	{
		return false;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_AcquireAccessToken);

	// In case many requests wants to update the token at the same time
	// get the current serial while we wait to take the CS.
	const uint32 WantsToUpdateTokenSerial = Access ? Access->GetSerial() : 0;

	FScopeLock Lock(&AccessCs);

	// If the token was updated while we waited to take the lock, then it should now be valid.
	if (Access && Access->GetSerial() > WantsToUpdateTokenSerial)
	{
		return true;
	}

	if (!OAuthAccessToken.IsEmpty())
	{
		SetAccessToken(OAuthAccessToken);
		return true;
	}

	if (!OAuthSecret.IsEmpty())
	{
		THttpUniquePtr<IHttpClient> LocalClient;
		if (!Client)
		{
			LocalClient = ConnectionPool->CreateClient(GetDefaultClientParams());
			Client = LocalClient.Get();
		}

		FHttpRequestParams RequestParams;
		RequestParams.bIgnoreMaxRequests = true;
		FHttpOperation Operation(Client->TryCreateRequest(RequestParams));
		Operation.SetUri(StringCast<ANSICHAR>(*OAuthProvider));

		if (OAuthProvider.StartsWith(TEXT("http://localhost")))
		{
			// Simple unauthenticated call to a local endpoint that mimics the result from an OIDC provider.
			Operation.Send();
		}
		else
		{
			TUtf8StringBuilder<256> OAuthFormData;
			OAuthFormData
				<< ANSITEXTVIEW("client_id=") << OAuthClientId
				<< ANSITEXTVIEW("&scope=") << OAuthScope
				<< ANSITEXTVIEW("&grant_type=client_credentials")
				<< ANSITEXTVIEW("&client_secret=") << OAuthSecret;

			Operation.SetMethod(EHttpMethod::Post);
			Operation.SetContentType(EHttpMediaType::FormUrlEncoded);
			Operation.SetBody(FCompositeBuffer(FSharedBuffer::MakeView(MakeMemoryView(OAuthFormData))));
			Operation.Send();
		}

		if (Operation.GetStatusCode() == 200)
		{
			if (TSharedPtr<FJsonObject> ResponseObject = Operation.GetBodyAsJson())
			{
				FString AccessTokenString;
				double ExpiryTimeSeconds = 0.0;
				if (ResponseObject->TryGetStringField(TEXT("access_token"), AccessTokenString) &&
					ResponseObject->TryGetNumberField(TEXT("expires_in"), ExpiryTimeSeconds))
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Logged in to HTTP DDC services. Expires in %.0f seconds."), *Domain, ExpiryTimeSeconds);
					SetAccessToken(AccessTokenString, ExpiryTimeSeconds);
					return true;
				}
			}
		}

		UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to log in to HTTP services with request %s."), *Domain, *WriteToString<256>(Operation));
		FailedLoginAttempts++;
		return false;
	}

	if (!OAuthProviderIdentifier.IsEmpty())
	{
		FString AccessTokenString;
		FDateTime TokenExpiresAt;
		if (FDesktopPlatformModule::Get()->GetOidcAccessToken(FPaths::RootDir(), FPaths::GetProjectFilePath(), OAuthProviderIdentifier, FApp::IsUnattended(), GWarn, AccessTokenString, TokenExpiresAt))
		{
			const double ExpiryTimeSeconds = (TokenExpiresAt - FDateTime::UtcNow()).GetTotalSeconds();
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: OidcToken: Logged in to HTTP DDC services. Expires at %s which is in %.0f seconds."),
				*Domain, *TokenExpiresAt.ToString(), ExpiryTimeSeconds);
			SetAccessToken(AccessTokenString, ExpiryTimeSeconds);
			return true;
		}

		UE_LOG(LogDerivedDataCache, Error, TEXT("%s: OidcToken: Failed to log in to HTTP services."), *Domain);
		FailedLoginAttempts++;
		return false;
	}

	UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: No available configuration to acquire an access token."), *Domain);
	return false;
}

void FHttpCacheStore::SetAccessToken(FStringView Token, double RefreshDelay)
{
	if (RefreshAccessTokenHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RefreshAccessTokenHandle);
		RefreshAccessTokenHandle.Reset();
	}

	if (!Access)
	{
		Access = MakeUnique<FHttpAccessToken>();
	}
	Access->SetToken(Token);

	constexpr double RefreshGracePeriod = 20.0f;
	if (RefreshDelay > RefreshGracePeriod)
	{
		// Schedule a refresh of the token ahead of expiry time (this will not work in commandlets)
		if (!IsRunningCommandlet())
		{
			RefreshAccessTokenHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[this](float DeltaTime)
				{
					AcquireAccessToken();
					return false;
				}
			), float(FMath::Min(RefreshDelay - RefreshGracePeriod, MAX_flt)));
		}

		// Schedule a forced refresh of the token when the scheduled refresh is starved or unavailable.
		RefreshAccessTokenTime = FPlatformTime::Seconds() + RefreshDelay - RefreshGracePeriod * 0.5f;
	}
	else
	{
		RefreshAccessTokenTime = 0.0;
	}

	// Reset failed login attempts, the service is indeed alive.
	FailedLoginAttempts = 0;
}

TUniquePtr<FHttpCacheStore::FHttpOperation> FHttpCacheStore::WaitForHttpOperation(EOperationCategory Category, bool bUnboundedOverflow)
{
	if (Access && RefreshAccessTokenTime > 0.0 && RefreshAccessTokenTime < FPlatformTime::Seconds())
	{
		AcquireAccessToken();
	}

	THttpUniquePtr<IHttpRequest> Request;

	FHttpRequestParams Params;
	if (FPlatformProcess::SupportsMultithreading() && bHttpEnableAsync)
	{
		Params.bIgnoreMaxRequests = bUnboundedOverflow;
		Request = NonBlockingRequestQueue.CreateRequest(Params);
	}
	else
	{
		const bool bIsInGameThread = IsInGameThread();
		if (Category == EOperationCategory::Get)
		{
			Request = GetRequestQueues[bIsInGameThread].CreateRequest(Params);
		}
		else
		{
			Request = PutRequestQueues[bIsInGameThread].CreateRequest(Params);
		}
	}

	if (Access)
	{
		Request->AddHeader(ANSITEXTVIEW("Authorization"), WriteToAnsiString<1024>(*Access));
	}

	return MakeUnique<FHttpOperation>(MoveTemp(Request));
}

void FHttpCacheStore::GetCacheRecordOnlyAsync(
	IRequestOwner& Owner,
	const FSharedString& Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy,
	uint64 UserData,
	FOnGetCacheRecordOnlyComplete&& OnComplete)
{
	auto MakeResponse = [Name = FSharedString(Name), Key, UserData](uint64 BytesReceived, EStatus Status)
	{
		return FGetCacheRecordOnlyResponse{ Name, Key, UserData, BytesReceived, {}, Status };
	};

	if (!IsUsable())
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped get of %s from '%s' because this cache store is not available"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(0, EStatus::Error));
	}

	// Skip the request if querying the cache is disabled.
	if (!EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::QueryRemote))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped get of %s from '%s' due to cache policy"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(0, EStatus::Error));
	}

	if (DebugOptions.ShouldSimulateGetMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%s'"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(0, EStatus::Error));
	}

	TAnsiStringBuilder<64> Bucket;
	Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);

	TUniquePtr<FHttpOperation> Operation = WaitForHttpOperation(EOperationCategory::Get, /*bUnboundedOverflow*/ false);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(WriteToAnsiString<256>(EffectiveDomain, ANSITEXTVIEW("/api/v1/refs/"), Namespace, '/', Bucket, '/', Key.Hash));
	LocalOperation.SetMethod(EHttpMethod::Get);
	LocalOperation.AddAcceptType(EHttpMediaType::CbObject);
	LocalOperation.SetExpectedErrorCodes({404});
	LocalOperation.SendAsync(Owner, [Operation = MoveTemp(Operation), this, Name, Key, UserData, OnComplete = MoveTemp(OnComplete)]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetCacheRecordOnlyAsync_OnHttpRequestComplete);

		const int32 StatusCode = Operation->GetStatusCode();
		if (StatusCode >= 200 && StatusCode <= 204)
		{
			FSharedBuffer Body = Operation->GetBody();

			if (ValidateCompactBinary(Body, ECbValidateMode::Default) != ECbValidateError::None)
			{
				UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache miss with invalid package for %s from '%s'"),
					*Domain, *WriteToString<96>(Key), *Name);
				OnComplete({ Name, Key, UserData, Operation->GetBytesReceived(), {}, EStatus::Error });
			}
			else if (FOptionalCacheRecord Record = FCacheRecord::Load(FCbPackage(FCbObject(Body))); Record.IsNull())
			{
				UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache miss with record load failure for %s from '%s'"),
					*Domain, *WriteToString<96>(Key), *Name);
				OnComplete({ Name, Key, UserData, Operation->GetBytesReceived(), {}, EStatus::Error });
			}
			else
			{
				OnComplete({ Name, Key, UserData, Operation->GetBytesReceived(), MoveTemp(Record), EStatus::Ok });
			}
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with missing package for %s from '%s'"),
				*Domain, *WriteToString<96>(Key), *Name);
			OnComplete({ Name, Key, UserData, Operation->GetBytesReceived(), {}, EStatus::Error });
		}
	});
}

void FHttpCacheStore::PutCacheRecordAsync(
	IRequestOwner& Owner,
	const FSharedString& Name,
	const FCacheRecord& Record,
	const FCacheRecordPolicy& Policy,
	uint64 UserData,
	TUniqueFunction<void(FCachePutResponse&& Response, uint64 BytesSent)>&& OnComplete)
{
	const FCacheKey& Key = Record.GetKey();
	auto MakeResponse = [Name = FSharedString(Name), Key = FCacheKey(Key), UserData](EStatus Status)
	{
		return FCachePutResponse{ Name, Key, UserData, Status };
	};

	if (bReadOnly)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped put of %s from '%s' because this cache store is read-only"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(EStatus::Error), 0);
	}

	// Skip the request if storing to the cache is disabled.
	const ECachePolicy RecordPolicy = Policy.GetRecordPolicy();
	if (!EnumHasAnyFlags(RecordPolicy, ECachePolicy::StoreRemote))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped put of %s from '%s' due to cache policy"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(EStatus::Error), 0);
	}

	if (DebugOptions.ShouldSimulatePutMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%s'"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(EStatus::Error), 0);
	}

	// TODO: Jupiter currently always overwrites.  It doesn't have a "write if not present" feature (for records or attachments),
	//		 but would require one to implement all policy correctly.

	FCbPackage Package = Record.Save();

	FPutPackageOp::PutPackage(*this, Owner, Name, Key, MoveTemp(Package), Policy, UserData, [MakeResponse = MoveTemp(MakeResponse), OnComplete = MoveTemp(OnComplete)](FPutPackageOp::FCachePutPackageResponse&& Response)
	{
		OnComplete(MakeResponse(Response.Status), Response.BytesSent);
	});
}

void FHttpCacheStore::PutCacheValueAsync(
	IRequestOwner& Owner,
	const FSharedString& Name,
	const FCacheKey& Key,
	const FValue& Value,
	const ECachePolicy Policy,
	uint64 UserData,
	TUniqueFunction<void(FCachePutValueResponse&& Response, uint64 BytesSent)>&& OnComplete)
{
	auto MakeResponse = [Name = FSharedString(Name), Key = FCacheKey(Key), UserData](EStatus Status)
	{
		return FCachePutValueResponse{ Name, Key, UserData, Status };
	};

	if (bReadOnly)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped put of %s from '%s' because this cache store is read-only"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(EStatus::Error), 0);
	}

	// Skip the request if storing to the cache is disabled.
	if (!EnumHasAnyFlags(Policy, ECachePolicy::StoreRemote))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped put of %s from '%s' due to cache policy"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(EStatus::Error), 0);
	}

	if (DebugOptions.ShouldSimulatePutMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%s'"),
			*Domain, *WriteToString<96>(Key), *Name);
		return OnComplete(MakeResponse(EStatus::Error), 0);
	}

	// TODO: Jupiter currently always overwrites.  It doesn't have a "write if not present" feature (for records or attachments),
	//		 but would require one to implement all policy correctly.

	FCbWriter Writer;
	Writer.BeginObject();
	Writer.AddBinaryAttachment("RawHash", Value.GetRawHash());
	Writer.AddInteger("RawSize", Value.GetRawSize());
	Writer.EndObject();

	FCbPackage Package(Writer.Save().AsObject());
	Package.AddAttachment(FCbAttachment(Value.GetData()));

	FPutPackageOp::PutPackage(*this, Owner, Name, Key, MoveTemp(Package), Policy, UserData, [MakeResponse = MoveTemp(MakeResponse), OnComplete = MoveTemp(OnComplete)](FPutPackageOp::FCachePutPackageResponse&& Response)
	{
		OnComplete(MakeResponse(Response.Status), Response.BytesSent);
	});
}

void FHttpCacheStore::GetCacheValueAsync(
	IRequestOwner& Owner,
	FSharedString Name,
	const FCacheKey& Key,
	ECachePolicy Policy,
	uint64 UserData,
	FOnCacheGetValueComplete&& OnComplete)
{
	if (!IsUsable())
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped get of %s from '%s' because this cache store is not available"),
			*Domain, *WriteToString<96>(Key), *Name);
		OnComplete({Name, Key, {}, UserData, EStatus::Error});
		return;
	}

	// Skip the request if querying the cache is disabled.
	if (!EnumHasAnyFlags(Policy, ECachePolicy::QueryRemote))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped get of %s from '%s' due to cache policy"),
			*Domain, *WriteToString<96>(Key), *Name);
		OnComplete({Name, Key, {}, UserData, EStatus::Error});
		return;
	}

	if (DebugOptions.ShouldSimulateGetMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%s'"),
			*Domain, *WriteToString<96>(Key), *Name);
		OnComplete({Name, Key, {}, UserData, EStatus::Error});
		return;
	}

	const bool bSkipData = EnumHasAnyFlags(Policy, ECachePolicy::SkipData);

	TAnsiStringBuilder<64> Bucket;
	Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);

	TUniquePtr<FHttpOperation> Operation = WaitForHttpOperation(EOperationCategory::Get, /*bUnboundedOverflow*/ false);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(WriteToAnsiString<256>(EffectiveDomain, ANSITEXTVIEW("/api/v1/refs/"), Namespace, '/', Bucket, '/', Key.Hash));
	LocalOperation.SetMethod(EHttpMethod::Get);
	if (bSkipData)
	{
		LocalOperation.AddAcceptType(EHttpMediaType::CbObject);
	}
	else
	{
		LocalOperation.AddHeader(ANSITEXTVIEW("Accept"), ANSITEXTVIEW("application/x-jupiter-inline"));
	}
	LocalOperation.SetExpectedErrorCodes({404});
	LocalOperation.SendAsync(Owner, [Operation = MoveTemp(Operation), this, Name, Key, UserData, bSkipData, OnComplete = MoveTemp(OnComplete)]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetCacheValueAsync_OnHttpRequestComplete);

		const int32 StatusCode = Operation->GetStatusCode();
		if (StatusCode >= 200 && StatusCode <= 204)
		{
			FValue ResultValue;
			FSharedBuffer ResponseBuffer = Operation->GetBody();

			if (bSkipData)
			{
				if (ValidateCompactBinary(ResponseBuffer, ECbValidateMode::Default) != ECbValidateError::None)
				{
					UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%s'"),
						*Domain, *WriteToString<96>(Key), *Name);
					OnComplete({Name, Key, {}, UserData, EStatus::Error});
					return;
				}

				const FCbObjectView Object = FCbObject(ResponseBuffer);
				const FIoHash RawHash = Object["RawHash"].AsHash();
				const uint64 RawSize = Object["RawSize"].AsUInt64(MAX_uint64);
				if (RawHash.IsZero() || RawSize == MAX_uint64)
				{
					UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid value for %s from '%'"),
						*Domain, *WriteToString<96>(Key), *Name);
					OnComplete({Name, Key, {}, UserData, EStatus::Error});
					return;
				}
				ResultValue = FValue(RawHash, RawSize);
			}
			else
			{
				FCompressedBuffer CompressedBuffer = FCompressedBuffer::FromCompressed(ResponseBuffer);
				if (!CompressedBuffer)
				{
					if (FAnsiStringView ReceivedHashStr = Operation->GetHeader("X-Jupiter-InlinePayloadHash"); !ReceivedHashStr.IsEmpty())
					{
						FIoHash ReceivedHash(ReceivedHashStr);
						FIoHash ComputedHash = FIoHash::HashBuffer(ResponseBuffer.GetView());
						if (ReceivedHash == ComputedHash)
						{
							CompressedBuffer = FCompressedBuffer::Compress(ResponseBuffer);
						}
					}
				}

				if (!CompressedBuffer)
				{
					UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%s'"),
						*Domain, *WriteToString<96>(Key), *Name);
					OnComplete({Name, Key, {}, UserData, EStatus::Error});
					return;
				}
				ResultValue = FValue(CompressedBuffer);
			}
			OnComplete({Name, Key, ResultValue, UserData, EStatus::Ok});
			return;
		}

		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with failed HTTP request for %s from '%s'"),
			*Domain, *WriteToString<96>(Key), *Name);
		OnComplete({Name, Key, {}, UserData, EStatus::Error});
	});
}

void FHttpCacheStore::GetCacheRecordAsync(
	IRequestOwner& Owner,
	const FSharedString& Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy,
	uint64 UserData,
	TUniqueFunction<void(FCacheGetResponse&& Response, uint64 BytesReceived)>&& OnComplete)
{
	FGetRecordOp::GetRecord(*this, Owner, Name, Key, Policy, UserData, MoveTemp(OnComplete));
}

void FHttpCacheStore::RefCachedDataProbablyExistsBatchAsync(
	IRequestOwner& Owner,
	TConstArrayView<FCacheGetValueRequest> ValueRefs,
	FOnCacheGetValueComplete&& OnComplete)
{
	if (ValueRefs.IsEmpty())
	{
		return;
	}

	if (!IsUsable())
	{
		for (const FCacheGetValueRequest& ValueRef : ValueRefs)
		{
			UE_LOG(LogDerivedDataCache, VeryVerbose,
				TEXT("%s: Skipped exists check of %s from '%s' because this cache store is not available"),
				*Domain, *WriteToString<96>(ValueRef.Key), *ValueRef.Name);
			OnComplete(ValueRef.MakeResponse(EStatus::Error));
		}
		return;
	}

	FCbWriter RequestWriter;
	RequestWriter.BeginObject();
	RequestWriter.BeginArray(ANSITEXTVIEW("ops"));
	uint32 OpIndex = 0;
	for (const FCacheGetValueRequest& ValueRef : ValueRefs)
	{
		RequestWriter.BeginObject();
		RequestWriter.AddInteger(ANSITEXTVIEW("opId"), OpIndex);
		RequestWriter.AddString(ANSITEXTVIEW("op"), ANSITEXTVIEW("GET"));
		const FCacheKey& Key = ValueRef.Key;
		TAnsiStringBuilder<64> Bucket;
		Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);
		RequestWriter.AddString(ANSITEXTVIEW("bucket"), Bucket);
		RequestWriter.AddString(ANSITEXTVIEW("key"), LexToString(Key.Hash));
		RequestWriter.AddBool(ANSITEXTVIEW("resolveAttachments"), true);
		RequestWriter.EndObject();
		++OpIndex;
	}
	RequestWriter.EndArray();
	RequestWriter.EndObject();
	FCbFieldIterator RequestFields = RequestWriter.Save();

	TUniquePtr<FHttpOperation> Operation = WaitForHttpOperation(EOperationCategory::Get, /*bUnboundedOverflow*/ false);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(WriteToAnsiString<256>(EffectiveDomain, ANSITEXTVIEW("/api/v1/refs/"), Namespace));
	LocalOperation.SetMethod(EHttpMethod::Post);
	LocalOperation.SetContentType(EHttpMediaType::CbObject);
	LocalOperation.AddAcceptType(EHttpMediaType::CbObject);
	LocalOperation.SetBody(FCompositeBuffer(RequestFields.GetOuterBuffer()));
	LocalOperation.SendAsync(Owner, [Operation = MoveTemp(Operation), this, ValueRefs = TArray<FCacheGetValueRequest>(ValueRefs), OnComplete = MoveTemp(OnComplete)]
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_RefCachedDataProbablyExistsBatchAsync_OnHttpRequestComplete);

		const int32 OverallStatusCode = Operation->GetStatusCode();
		if (OverallStatusCode >= 200 && OverallStatusCode <= 204)
		{
			FMemoryView ResponseView = Operation->GetBody();
			if (ValidateCompactBinary(ResponseView, ECbValidateMode::Default) != ECbValidateError::None)
			{
				for (const FCacheGetValueRequest& ValueRef : ValueRefs)
				{
					UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache exists returned invalid results."), *Domain);
					OnComplete(ValueRef.MakeResponse(EStatus::Error));
				}
				return;
			}

			const FCbObjectView ResponseObject(ResponseView.GetData());

			FCbArrayView ResultsArrayView = ResponseObject[ANSITEXTVIEW("results")].AsArrayView();

			if (ResultsArrayView.Num() != ValueRefs.Num())
			{
				for (const FCacheGetValueRequest& ValueRef : ValueRefs)
				{
					UE_LOG(LogDerivedDataCache, Log,
						TEXT("%s: Cache exists returned unexpected quantity of results (expected %d, got %d)."),
						*Domain, ValueRefs.Num(), ResultsArrayView.Num());
						OnComplete(ValueRef.MakeResponse(EStatus::Error));
				}
				return;
			}

			for (FCbFieldView ResultFieldView : ResultsArrayView)
			{
				FCbObjectView ResultObjectView = ResultFieldView.AsObjectView();
				uint32 OpId = ResultObjectView[ANSITEXTVIEW("opId")].AsUInt32();
				FCbObjectView ResponseObjectView = ResultObjectView[ANSITEXTVIEW("response")].AsObjectView();
				int32 StatusCode = ResultObjectView[ANSITEXTVIEW("statusCode")].AsInt32();

				if (OpId >= (uint32)ValueRefs.Num())
				{
					UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Encountered invalid opId %d while querying %d values"),
						*Domain, OpId, ValueRefs.Num());
					continue;
				}

				const FCacheGetValueRequest& ValueRef = ValueRefs[OpId];

				if (StatusCode < 200 || StatusCode > 204)
				{
					UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with unsuccessful response code %d for %s from '%s'"),
						*Domain, StatusCode, *WriteToString<96>(ValueRef.Key), *ValueRef.Name);
					OnComplete(ValueRef.MakeResponse(EStatus::Error));
					continue;
				}

				if (!EnumHasAnyFlags(ValueRef.Policy, ECachePolicy::QueryRemote))
				{
					UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped exists check of %s from '%s' due to cache policy"),
						*Domain, *WriteToString<96>(ValueRef.Key), *ValueRef.Name);
					OnComplete(ValueRef.MakeResponse(EStatus::Error));
					continue;
				}

				const FIoHash RawHash = ResponseObjectView[ANSITEXTVIEW("RawHash")].AsHash();
				const uint64 RawSize = ResponseObjectView[ANSITEXTVIEW("RawSize")].AsUInt64(MAX_uint64);
				if (RawHash.IsZero() || RawSize == MAX_uint64)
				{
					UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid value for %s from '%s'"),
						*Domain, *WriteToString<96>(ValueRef.Key), *ValueRef.Name);
					OnComplete(ValueRef.MakeResponse(EStatus::Error));
					continue;
				}

				OnComplete({ValueRef.Name, ValueRef.Key, FValue(RawHash, RawSize), ValueRef.UserData, EStatus::Ok});
			}
			return;
		}

		for (const FCacheGetValueRequest& ValueRef : ValueRefs)
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with failed HTTP request for %s from '%s'"),
				*Domain, *WriteToString<96>(ValueRef.Key), *ValueRef.Name);
			OnComplete(ValueRef.MakeResponse(EStatus::Error));
		}
	});
}

void FHttpCacheStore::LegacyStats(FDerivedDataCacheStatsNode& OutNode)
{
	OutNode = {TEXT("Unreal Cloud DDC"), FString::Printf(TEXT("%s (%s)"), *Domain, *Namespace), /*bIsLocal*/ false};
	OutNode.UsageStats.Add(TEXT(""), UsageStats);
}

void FHttpCacheStore::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_Put);
	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnCachePutComplete> SharedOnComplete = MakeShared<FOnCachePutComplete>(MoveTemp(OnComplete));
	for (const FCachePutRequest& Request : Requests)
	{
		PutCacheRecordAsync(Owner, Request.Name, Request.Record, Request.Policy, Request.UserData,
			[COOK_STAT(Timer = UsageStats.TimePut(), ) SharedOnComplete](FCachePutResponse&& Response, uint64 BytesSent) mutable
		{
			TRACE_COUNTER_ADD(HttpDDC_BytesSent, BytesSent);
			if (Response.Status == EStatus::Ok)
			{
				COOK_STAT(if (BytesSent) { Timer.AddHit(BytesSent); });
			}
			SharedOnComplete.Get()(MoveTemp(Response));
		});
	}
}

void FHttpCacheStore::Get(
	const TConstArrayView<FCacheGetRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_Get);
	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnCacheGetComplete> SharedOnComplete = MakeShared<FOnCacheGetComplete>(MoveTemp(OnComplete));
	for (const FCacheGetRequest& Request : Requests)
	{
		GetCacheRecordAsync(Owner, Request.Name, Request.Key, Request.Policy, Request.UserData,
			[COOK_STAT(Timer = UsageStats.TimePut(), ) SharedOnComplete](FCacheGetResponse&& Response, uint64 BytesReceived) mutable
		{
			TRACE_COUNTER_ADD(HttpDDC_BytesReceived, BytesReceived);
			if (Response.Status == EStatus::Ok)
			{
				COOK_STAT(Timer.AddHit(BytesReceived););
			}
			SharedOnComplete.Get()(MoveTemp(Response));
		});
	}
}

void FHttpCacheStore::PutValue(
	const TConstArrayView<FCachePutValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutValueComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_PutValue);
	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnCachePutValueComplete> SharedOnComplete = MakeShared<FOnCachePutValueComplete>(MoveTemp(OnComplete));
	for (const FCachePutValueRequest& Request : Requests)
	{
		PutCacheValueAsync(Owner, Request.Name, Request.Key, Request.Value, Request.Policy, Request.UserData,
			[COOK_STAT(Timer = UsageStats.TimePut(),) SharedOnComplete](FCachePutValueResponse&& Response, uint64 BytesSent) mutable
		{
			TRACE_COUNTER_ADD(HttpDDC_BytesSent, BytesSent);
			if (Response.Status == EStatus::Ok)
			{
				COOK_STAT(if (BytesSent) { Timer.AddHit(BytesSent); });
			}
			SharedOnComplete.Get()(MoveTemp(Response));
		});
	}
}

void FHttpCacheStore::GetValue(
	const TConstArrayView<FCacheGetValueRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetValueComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetValue);
	COOK_STAT(double StartTime = FPlatformTime::Seconds());
	COOK_STAT(bool bIsInGameThread = IsInGameThread());

	bool bBatchExistsCandidate = true;
	for (const FCacheGetValueRequest& Request : Requests)
	{
		if (!EnumHasAnyFlags(Request.Policy, ECachePolicy::SkipData))
		{
			bBatchExistsCandidate = false;
			break;
		}
	}
	if (bBatchExistsCandidate)
	{
		RefCachedDataProbablyExistsBatchAsync(Owner, Requests,
		[this, COOK_STAT(StartTime, bIsInGameThread, ) OnComplete = MoveTemp(OnComplete)](FCacheGetValueResponse&& Response)
		{
			if (Response.Status != EStatus::Ok)
			{
				COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter, 1l, bIsInGameThread));
				OnComplete(MoveTemp(Response));
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
					*Domain, *WriteToString<96>(Response.Key), *Response.Name);
				COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter, 1l, bIsInGameThread));
				OnComplete(MoveTemp(Response));
			}

			COOK_STAT(const int64 CyclesUsed = int64((FPlatformTime::Seconds() - StartTime) / FPlatformTime::GetSecondsPerCycle()));
			COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Cycles, CyclesUsed, bIsInGameThread));
		});
	}
	else
	{
		FRequestBarrier Barrier(Owner);
		TSharedRef<FOnCacheGetValueComplete> SharedOnComplete = MakeShared<FOnCacheGetValueComplete>(MoveTemp(OnComplete));
		int64 HitBytes = 0;
		for (const FCacheGetValueRequest& Request : Requests)
		{
			GetCacheValueAsync(Owner, Request.Name, Request.Key, Request.Policy, Request.UserData,
			[this, COOK_STAT(StartTime, bIsInGameThread,) Policy = Request.Policy, SharedOnComplete](FCacheGetValueResponse&& Response)
			{
				const FOnCacheGetValueComplete& OnComplete = SharedOnComplete.Get();
				check(OnComplete);
				if (Response.Status != EStatus::Ok)
				{
					COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter, 1l, bIsInGameThread));
					OnComplete(MoveTemp(Response));
				}
				else
				{
					if (!IsValueDataReady(Response.Value, Policy) && !EnumHasAnyFlags(Policy, ECachePolicy::SkipData))
					{
						// With inline fetching, expect we will always have a value we can use.  Even SkipData/Exists can rely on the blob existing if the ref is reported to exist.
						UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache miss due to inlining failure for %s from '%s'"),
									*Domain, *WriteToString<96>(Response.Key), *Response.Name);
						COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Miss, FCookStats::CallStats::EStatType::Counter, 1l, bIsInGameThread));
						OnComplete(MoveTemp(Response));
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
									*Domain, *WriteToString<96>(Response.Key), *Response.Name);
						uint64 ValueSize = Response.Value.GetData().GetCompressedSize();
						TRACE_COUNTER_ADD(HttpDDC_BytesReceived, ValueSize);
						COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Counter, 1l, bIsInGameThread));
						OnComplete({ Response.Name, Response.Key, Response.Value, Response.UserData, EStatus::Ok });
						
						COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Bytes, ValueSize, bIsInGameThread));
					}
				}
				COOK_STAT(const int64 CyclesUsed = int64((FPlatformTime::Seconds() - StartTime) / FPlatformTime::GetSecondsPerCycle()));
				COOK_STAT(UsageStats.GetStats.Accumulate(FCookStats::CallStats::EHitOrMiss::Hit, FCookStats::CallStats::EStatType::Cycles, CyclesUsed, bIsInGameThread));
			});
		}
	}

}

void FHttpCacheStore::GetChunks(
	const TConstArrayView<FCacheGetChunkRequest> Requests,
	IRequestOwner& Owner,
	FOnCacheGetChunkComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetChunks);
	// TODO: This is inefficient because Jupiter doesn't allow us to get only part of a compressed blob, so we have to
	//		 get the whole thing and then decompress only the portion we need.  Furthermore, because there is no propagation
	//		 between cache stores during chunk requests, the fetched result won't end up in the local store.
	//		 These efficiency issues will be addressed by changes to the Hierarchy that translate chunk requests that
	//		 are missing in local/fast stores and have to be retrieved from slow stores into record requests instead.  That
	//		 will make this code path unused/uncommon as Jupiter will most always be a slow store with a local/fast store in front of it.
	//		 Regardless, to adhere to the functional contract, this implementation must exist.
	TArray<FCacheGetChunkRequest, TInlineAllocator<16>> SortedRequests(Requests);
	SortedRequests.StableSort(TChunkLess());

	bool bHasValue = false;
	FValue Value;
	FValueId ValueId;
	FCacheKey ValueKey;
	FCompressedBuffer ValueBuffer;
	FCompressedBufferReader ValueReader;
	EStatus ValueStatus = EStatus::Error;
	FOptionalCacheRecord Record;
	for (const FCacheGetChunkRequest& Request : SortedRequests)
	{
		const bool bExistsOnly = EnumHasAnyFlags(Request.Policy, ECachePolicy::SkipData);
		COOK_STAT(auto Timer = bExistsOnly ? UsageStats.TimeProbablyExists() : UsageStats.TimeGet());
		if (!(bHasValue && ValueKey == Request.Key && ValueId == Request.Id) || ValueReader.HasSource() < !bExistsOnly)
		{
			ValueStatus = EStatus::Error;
			ValueReader.ResetSource();
			ValueKey = {};
			ValueId.Reset();
			Value.Reset();
			bHasValue = false;
			if (Request.Id.IsValid())
			{
				if (!(Record && Record.Get().GetKey() == Request.Key))
				{
					FCacheRecordPolicyBuilder PolicyBuilder(ECachePolicy::None);
					PolicyBuilder.AddValuePolicy(Request.Id, Request.Policy);
					Record.Reset();

					FRequestOwner BlockingOwner(EPriority::Blocking);
					GetCacheRecordOnlyAsync(BlockingOwner, Request.Name, Request.Key, PolicyBuilder.Build(), 0, [&Record](FGetCacheRecordOnlyResponse&& Response)
					{
						Record = MoveTemp(Response.Record);
					});
					BlockingOwner.Wait();
				}
				if (Record)
				{
					const FValueWithId& ValueWithId = Record.Get().GetValue(Request.Id);
					bHasValue = ValueWithId.IsValid();
					Value = ValueWithId;
					ValueId = Request.Id;
					ValueKey = Request.Key;

					if (IsValueDataReady(Value, Request.Policy))
					{
						ValueReader.SetSource(Value.GetData());
					}
					else
					{
						auto IdGetter = [](const FValueWithId& Value)
						{
							return FString(WriteToString<16>(Value.GetId()));
						};

						FRequestOwner BlockingOwner(EPriority::Blocking);
						bool bSucceeded = false;
						FCompressedBuffer NewBuffer;
						FGetRecordOp::GetDataBatch(*this, BlockingOwner, Request.Name, Request.Key, ::MakeArrayView({ ValueWithId }), IdGetter, [&bSucceeded, &NewBuffer](FGetRecordOp::FGetCachedDataBatchResponse&& Response)
						{
							if (Response.Status == EStatus::Ok)
							{
								bSucceeded = true;
								NewBuffer = MoveTemp(Response.DataBuffer);
							}
						});
						BlockingOwner.Wait();

						if (bSucceeded)
						{
							ValueBuffer = MoveTemp(NewBuffer);
							ValueReader.SetSource(ValueBuffer);
						}
						else
						{
							ValueBuffer.Reset();
							ValueReader.ResetSource();
						}
					}
				}
			}
			else
			{
				ValueKey = Request.Key;

				{
					FRequestOwner BlockingOwner(EPriority::Blocking);
					bool bSucceeded = false;
					GetCacheValueAsync(BlockingOwner, Request.Name, Request.Key, Request.Policy, 0, [&bSucceeded, &Value](FCacheGetValueResponse&& Response)
					{
						Value = MoveTemp(Response.Value);
						bSucceeded = Response.Status == EStatus::Ok;
					});
					BlockingOwner.Wait();
					bHasValue = bSucceeded;
				}

				if (bHasValue)
				{
					if (IsValueDataReady(Value, Request.Policy))
					{
						ValueReader.SetSource(Value.GetData());
					}
					else
					{
						auto IdGetter = [](const FValue& Value)
						{
							return FString(TEXT("Default"));
						};

						FRequestOwner BlockingOwner(EPriority::Blocking);
						bool bSucceeded = false;
						FCompressedBuffer NewBuffer;
						FGetRecordOp::GetDataBatch(*this, BlockingOwner, Request.Name, Request.Key, ::MakeArrayView({ Value }), IdGetter, [&bSucceeded, &NewBuffer](FGetRecordOp::FGetCachedDataBatchResponse&& Response)
						{
							if (Response.Status == EStatus::Ok)
							{
								bSucceeded = true;
								NewBuffer = MoveTemp(Response.DataBuffer);
							}
						});
						BlockingOwner.Wait();

						if (bSucceeded)
						{
							ValueBuffer = MoveTemp(NewBuffer);
							ValueReader.SetSource(ValueBuffer);
						}
						else
						{
							ValueBuffer.Reset();
							ValueReader.ResetSource();
						}
					}
				}
				else
				{
					ValueBuffer.Reset();
					ValueReader.ResetSource();
				}
			}
		}
		if (bHasValue)
		{
			const uint64 RawOffset = FMath::Min(Value.GetRawSize(), Request.RawOffset);
			const uint64 RawSize = FMath::Min(Value.GetRawSize() - RawOffset, Request.RawSize);
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
				*Domain, *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);
			COOK_STAT(Timer.AddHit(!bExistsOnly ? RawSize : 0));
			FSharedBuffer Buffer;
			if (!bExistsOnly)
			{
				Buffer = ValueReader.Decompress(RawOffset, RawSize);
			}
			const EStatus ChunkStatus = bExistsOnly || Buffer.GetSize() == RawSize ? EStatus::Ok : EStatus::Error;
			OnComplete({Request.Name, Request.Key, Request.Id, Request.RawOffset,
				RawSize, Value.GetRawHash(), MoveTemp(Buffer), Request.UserData, ChunkStatus});
			continue;
		}

		OnComplete(Request.MakeResponse(EStatus::Error));
	}
}

void FHttpCacheStoreParams::Parse(const TCHAR* NodeName, const TCHAR* Config)
{
	FString ServerId;
	if (FParse::Value(Config, TEXT("ServerID="), ServerId))
	{
		FString ServerEntry;
		const TCHAR* ServerSection = TEXT("StorageServers");
		const TCHAR* FallbackServerSection = TEXT("HordeStorageServers");
		if (GConfig->GetString(ServerSection, *ServerId, ServerEntry, GEngineIni))
		{
			Parse(NodeName, *ServerEntry);
		}
		else if (GConfig->GetString(FallbackServerSection, *ServerId, ServerEntry, GEngineIni))
		{
			Parse(NodeName, *ServerEntry);
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Using ServerID=%s which was not found in [%s]"), NodeName, *ServerId, ServerSection);
		}
	}

	FString OverrideName;

	// Host Params

	FParse::Value(Config, TEXT("Host="), Host);
	if (FParse::Value(Config, TEXT("EnvHostOverride="), OverrideName))
	{
		FString HostEnv = FPlatformMisc::GetEnvironmentVariable(*OverrideName);
		if (!HostEnv.IsEmpty())
		{
			Host = HostEnv;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found environment override for Host %s=%s"), NodeName, *OverrideName, *Host);
		}
	}
	if (FParse::Value(Config, TEXT("CommandLineHostOverride="), OverrideName))
	{
		if (FParse::Value(FCommandLine::Get(), *(OverrideName + TEXT("=")), Host))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override for Host %s=%s"), NodeName, *OverrideName, *Host);
		}
	}

	FParse::Value(Config, TEXT("HostPinnedPublicKeys="), HostPinnedPublicKeys);

	FParse::Bool(Config, TEXT("ResolveHostCanonicalName="), bResolveHostCanonicalName);

	// Namespace Params

	if (Namespace.IsEmpty())
	{
		FParse::Value(Config, TEXT("Namespace="), Namespace);
	}
	FParse::Value(Config, TEXT("StructuredNamespace="), Namespace);

	// OAuth Params

	FParse::Value(Config, TEXT("OAuthProvider="), OAuthProvider);

	if (FParse::Value(Config, TEXT("CommandLineOAuthProviderOverride="), OverrideName))
	{
		if (FParse::Value(FCommandLine::Get(), *(OverrideName + TEXT("=")), OAuthProvider))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override for OAuthProvider %s=%s"), NodeName, *OverrideName, *OAuthProvider);
		}
	}

	FParse::Value(Config, TEXT("OAuthClientId="), OAuthClientId);
	FParse::Value(Config, TEXT("OAuthSecret="), OAuthSecret);

	if (FParse::Value(Config, TEXT("CommandLineOAuthSecretOverride="), OverrideName))
	{
		if (FParse::Value(FCommandLine::Get(), *(OverrideName + TEXT("=")), OAuthSecret))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override for OAuthSecret %s=%s"), NodeName, *OverrideName, *OAuthSecret);
		}
	}

	// If the secret is a file path, read the secret from the file.
	if (OAuthSecret.StartsWith(TEXT("file://")))
	{
		TStringBuilder<256> FilePath;
		FilePath << MakeStringView(OAuthSecret).RightChop(TEXTVIEW("file://").Len());
		if (!FFileHelper::LoadFileToString(OAuthSecret, *FilePath))
		{
			OAuthSecret.Empty();
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to read OAuth secret file: %s"), NodeName, *FilePath);
		}
	}

	FParse::Value(Config, TEXT("OAuthScope="), OAuthScope);

	FParse::Value(Config, TEXT("OAuthProviderIdentifier="), OAuthProviderIdentifier);

	if (FParse::Value(Config, TEXT("OAuthAccessTokenEnvOverride="), OverrideName))
	{
		FString AccessToken = FPlatformMisc::GetEnvironmentVariable(*OverrideName);
		if (!AccessToken.IsEmpty())
		{
			OAuthAccessToken = AccessToken;
			// We do not log the access token as it is sensitive information.
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found OAuth access token in %s."), NodeName, *OverrideName);
		}
	}

	FParse::Value(Config, TEXT("OAuthPinnedPublicKeys="), OAuthPinnedPublicKeys);

	// Cache Params

	FParse::Bool(Config, TEXT("ReadOnly="), bReadOnly);
}

} // UE::DerivedData

#endif // WITH_HTTP_DDC_BACKEND

namespace UE::DerivedData
{

TTuple<ILegacyCacheStore*, ECacheStoreFlags> CreateHttpCacheStore(const TCHAR* NodeName, const TCHAR* Config)
{
#if !WITH_HTTP_DDC_BACKEND
	UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: HTTP cache is not yet supported in the current build configuration."), NodeName);
#else
	FHttpCacheStoreParams Params;
	Params.Parse(NodeName, Config);

	bool bValidParams = true;

	if (Params.Host.IsEmpty())
	{
		UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Missing required parameter 'Host'"), NodeName);
		bValidParams = false;
	}
	else if (Params.Host == TEXTVIEW("None"))
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Disabled because Host is set to 'None'"), NodeName);
		bValidParams = false;
	}

	if (Params.Namespace.IsEmpty())
	{
		Params.Namespace = FApp::GetProjectName();
		UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Missing required parameter 'StructuredNamespace', falling back to '%s'"), NodeName, *Params.Namespace);
	}

	if (!Params.Host.StartsWith(TEXT("http://localhost")))
	{
		bool bValidOAuthAccessToken = !Params.OAuthAccessToken.IsEmpty();

		bool bValidOAuthProviderIdentifier = !Params.OAuthProviderIdentifier.IsEmpty();

		bool bValidOAuthProvider = !Params.OAuthProvider.IsEmpty();
		if (bValidOAuthProvider)
		{
			if (!Params.OAuthProvider.StartsWith(TEXT("http://")) &&
				!Params.OAuthProvider.StartsWith(TEXT("https://")))
			{
				UE_LOG(LogDerivedDataCache, Error, TEXT("%s: OAuth provider '%s' must be a complete URI including the scheme."), NodeName, *Params.OAuthProvider);
				bValidParams = false;
			}

			// No need for OAuthClientId and OAuthSecret if using a local provider.
			if (!Params.OAuthProvider.StartsWith(TEXT("http://localhost")))
			{
				if (Params.OAuthClientId.IsEmpty())
				{
					UE_LOG(LogDerivedDataCache, Error, TEXT("%s: Missing required parameter 'OAuthClientId'"), NodeName);
					bValidOAuthProvider = false;
					bValidParams = false;
				}

				if (Params.OAuthSecret.IsEmpty())
				{
					UE_CLOG(!bValidOAuthAccessToken && !bValidOAuthProviderIdentifier,
						LogDerivedDataCache, Error, TEXT("%s: Missing required parameter 'OAuthSecret'"), NodeName);
					bValidOAuthProvider = false;
				}
			}
		}

		if (!bValidOAuthAccessToken && !bValidOAuthProviderIdentifier && !bValidOAuthProvider)
		{
			UE_LOG(LogDerivedDataCache, Error, TEXT("%s: At least one OAuth configuration must be provided and valid. "
				"Options are 'OAuthProvider', 'OAuthProviderIdentifier', and 'OAuthAccessTokenEnvOverride'"), NodeName);
			bValidParams = false;
		}
	}

	if (Params.OAuthScope.IsEmpty())
	{
		Params.OAuthScope = TEXTVIEW("cache_access");
	}

	if (bValidParams)
	{
		if (TUniquePtr<FHttpCacheStore> Store = MakeUnique<FHttpCacheStore>(Params); Store->IsUsable())
		{
			const ECacheStoreFlags StoreFlag = (Params.bReadOnly ? ECacheStoreFlags::None : ECacheStoreFlags::Store);
			return MakeTuple(Store.Release(), ECacheStoreFlags::Remote | ECacheStoreFlags::Query | StoreFlag);
		}
		UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to contact the service (%s), will not use it."), NodeName, *Params.Host);
	}
#endif

	return MakeTuple(nullptr, ECacheStoreFlags::None);
}

ILegacyCacheStore* GetAnyHttpCacheStore(
	FString& OutDomain,
	FString& OutOAuthProvider,
	FString& OutOAuthClientId,
	FString& OutOAuthSecret,
	FString& OutOAuthScope,
	FString& OAuthProviderIdentifier,
	FString& OAuthAccessToken,
	FString& OutNamespace)
{
#if WITH_HTTP_DDC_BACKEND
	if (FHttpCacheStore* HttpBackend = FHttpCacheStore::GetAny())
	{
		OutDomain = HttpBackend->GetDomain();
		OutOAuthProvider = HttpBackend->GetOAuthProvider();
		OutOAuthClientId = HttpBackend->GetOAuthClientId();
		OutOAuthSecret = HttpBackend->GetOAuthSecret();
		OutOAuthScope = HttpBackend->GetOAuthScope();
		OAuthProviderIdentifier = HttpBackend->GetOAuthProviderIdentifier();
		OAuthAccessToken = HttpBackend->GetOAuthAccessToken();
		OutNamespace = HttpBackend->GetNamespace();
		return HttpBackend;
	}
#endif
	return nullptr;
}

} // UE::DerivedData
