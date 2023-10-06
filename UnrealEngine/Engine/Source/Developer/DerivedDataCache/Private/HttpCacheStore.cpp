// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBackendInterface.h"
#include "DerivedDataLegacyCacheStore.h"

#if WITH_HTTP_DDC_BACKEND

#include "Algo/AllOf.h"
#include "Algo/Transform.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
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
#include "HAL/PlatformProcess.h"
#include "Http/HttpClient.h"
#include "IO/IoHash.h"
#include "Memory/SharedBuffer.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Optional.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinarySerialization.h"
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
#define UE_HTTPDDC_NONBLOCKING_GET_REQUEST_POOL_SIZE 128
#define UE_HTTPDDC_NONBLOCKING_PUT_REQUEST_POOL_SIZE 24
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
	FString Name;
	FString Host;
	FString HostPinnedPublicKeys;
	FString Namespace;
	FString HttpVersion;
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
	FHttpCacheStore(const FHttpCacheStoreParams& Params, ICacheStoreOwner* Owner);

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
	const FString GetAccessToken() const
	{
		TAnsiStringBuilder<128> AccessTokenBuilder;

		if (Access.IsValid())
		{
			AccessTokenBuilder << *Access;
		}

		return FString(ANSI_TO_TCHAR(AccessTokenBuilder.ToString()));
	}

private:
	FString Domain;
	FString Namespace;
	FString OAuthProvider;
	FString OAuthClientId;
	FString OAuthSecret;
	FString OAuthScope;
	FString OAuthProviderIdentifier;
	FString OAuthAccessToken;
	FString HttpVersion;

	FAnsiStringBuilderBase EffectiveDomain;

	ICacheStoreOwner* StoreOwner = nullptr;
	ICacheStoreStats* StoreStats = nullptr;

	FDerivedDataCacheUsageStats UsageStats;
	FBackendDebugOptions DebugOptions;
	THttpUniquePtr<IHttpConnectionPool> ConnectionPool;
	FHttpRequestQueue GetRequestQueues[2];
	FHttpRequestQueue PutRequestQueues[2];
	FHttpRequestQueue NonBlockingGetRequestQueue;
	FHttpRequestQueue NonBlockingPutRequestQueue;

	FCriticalSection AccessCs;
	TUniquePtr<FHttpAccessToken> Access;
	FTSTicker::FDelegateHandle RefreshAccessTokenHandle;
	double RefreshAccessTokenTime = 0.0;
	uint32 LoginAttempts = 0;
	uint32 FailedLoginAttempts = 0;
	uint32 InteractiveLoginAttempts = 0;

	bool bIsUsable = false;
	bool bReadOnly = false;

	static inline FHttpCacheStore* AnyInstance = nullptr;

	FHttpClientParams GetDefaultClientParams() const;

	bool AcquireAccessToken(IHttpClient* Client = nullptr);
	void SetAccessTokenAndUnlock(FScopeLock &Lock, FStringView Token, double RefreshDelay = 0.0);
	
	enum class EOperationCategory
	{
		Get,
		Put,
	};

	class FHttpOperation;

	TUniquePtr<FHttpOperation> WaitForHttpOperation(EOperationCategory Category);

	void PutCacheRecordAsync(IRequestOwner& Owner, const FCachePutRequest& Request, FOnCachePutComplete&& OnComplete);
	void PutCacheValueAsync(IRequestOwner& Owner, const FCachePutValueRequest& Request, FOnCachePutValueComplete&& OnComplete);

	void GetCacheRecordAsync(
		IRequestOwner& Owner,
		const FSharedString& Name,
		const FCacheKey& Key,
		const FCacheRecordPolicy& Policy,
		uint64 UserData,
		FOnCacheGetComplete&& OnComplete);

	void GetCacheValueAsync(
		IRequestOwner& Owner,
		FSharedString Name,
		const FCacheKey& Key,
		ECachePolicy Policy,
		ERequestOp RequestOp,
		uint64 UserData,
		FOnCacheGetValueComplete&& OnComplete);

	class FHealthCheckOp;
	class FPutPackageOp;
	class FGetRecordOp;
	class FGetValueOp;
	class FExistsBatchOp;
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
	void GetStats(FRequestStats& OutStats) const;

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
				StatsText.Appendf(TEXT("%.3f seconds %.3f|%.3f|%.3f|%.3f"), Stats.TotalTime, Stats.NameResolveTime, Stats.ConnectTime, Stats.TlsConnectTime, Stats.StartTransferTime);
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
	{}

private:
	// IRequest Interface

	void SetPriority(EPriority Priority) final {}
	void Cancel() final { Monitor->Cancel(); }
	void Wait() final { Monitor->Wait(); }

	// IHttpReceiver Interface

	IHttpReceiver* OnCreate(IHttpResponse& LocalResponse) final
	{
		Monitor = LocalResponse.GetMonitor();
		Owner->Begin(this);
		return &BaseReceiver;
	}

	IHttpReceiver* OnComplete(IHttpResponse& LocalResponse) final
	{
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
	if (GetContentType() == EHttpMediaType::CbObject)
	{
		if (ValidateCompactBinary(ResponseBody, ECbValidateMode::Default) == ECbValidateError::None)
		{
			TUtf8StringBuilder<1024> JsonStringBuilder;
			const FCbObject ResponseObject(ResponseBody);
			CompactBinaryToCompactJson(ResponseObject, JsonStringBuilder);
			return JsonStringBuilder.ToString();
		}
	}
	return FString(Len, (const UTF8CHAR*)ResponseBody.GetData());
}

TSharedPtr<FJsonObject> FHttpCacheStore::FHttpOperation::GetBodyAsJson() const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(GetBodyAsString());
	FJsonSerializer::Deserialize(JsonReader, JsonObject);
	return JsonObject;
}

void FHttpCacheStore::FHttpOperation::GetStats(FRequestStats& OutStats) const
{
	const FHttpResponseStats& Stats = Response->GetStats();
	TUniqueLock Lock(OutStats.Mutex);
	OutStats.PhysicalReadSize += Stats.RecvSize;
	OutStats.PhysicalWriteSize += Stats.SendSize;
	if (const EHttpMethod Method = Response->GetMethod(); Method == EHttpMethod::Get || Method == EHttpMethod::Head)
	{
		OutStats.AddLatency(FMonotonicTimeSpan::FromSeconds(Stats.StartTransferTime));
	}
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
	struct FResponse
	{
		EStatus Status = EStatus::Error;
	};
	using FOnPackageComplete = TUniqueFunction<void (FResponse&& Response)>;

	static TRefCountPtr<FPutPackageOp> New(FHttpCacheStore& CacheStore, IRequestOwner& Owner, const FSharedString& Name)
	{
		return new FPutPackageOp(CacheStore, Owner, Name);
	}

	void Put(const FCacheKey& Key, FCbPackage&& Package, FOnPackageComplete&& OnComplete);

	const FRequestStats& ReadStats() const { return RequestStats; }
	FRequestStats& EditStats() { return RequestStats; }

private:
	FHttpCacheStore& CacheStore;
	IRequestOwner& Owner;
	const FSharedString Name;

	FCacheKey Key;
	FCbObject Object;
	FIoHash ObjectHash;
	FOnPackageComplete OnPackageComplete;

	FRequestStats RequestStats;

	std::atomic<uint32> SuccessfulBlobUploads = 0;
	std::atomic<uint32> PendingBlobUploads = 0;
	uint32 TotalBlobUploads = 0;

	struct FCachePutRefResponse
	{
		TConstArrayView<FIoHash> NeededBlobHashes;
		EStatus Status = EStatus::Error;
	};
	using FOnCachePutRefComplete = TUniqueFunction<void(FCachePutRefResponse&& Response)>;

	FPutPackageOp(FHttpCacheStore& CacheStore, IRequestOwner& Owner, const FSharedString& Name);

	void BeginPutRef(bool bFinalize, FOnCachePutRefComplete&& OnComplete);
	void EndPutRef(TUniquePtr<FHttpOperation> Operation, bool bFinalize, FOnCachePutRefComplete&& OnComplete);

	void BeginPutBlobs(FCbPackage&& Package, FCachePutRefResponse&& Response);
	void EndPutBlob(FHttpOperation& Operation, uint64 LogicalSize);

	void EndPutRefFinalize(FCachePutRefResponse&& Response);

	void EndPut(EStatus Status);
};

FHttpCacheStore::FPutPackageOp::FPutPackageOp(FHttpCacheStore& InCacheStore, IRequestOwner& InOwner, const FSharedString& InName)
	: CacheStore(InCacheStore)
	, Owner(InOwner)
	, Name(InName)
{
	RequestStats.Name = Name;
}

void FHttpCacheStore::FPutPackageOp::Put(const FCacheKey& InKey, FCbPackage&& Package, FOnPackageComplete&& OnComplete)
{
	Key = InKey;
	Object = Package.GetObject();
	ObjectHash = Package.GetObjectHash();
	OnPackageComplete = MoveTemp(OnComplete);
	BeginPutRef(/*bFinalize*/ false, [Self = TRefCountPtr(this), Package = MoveTemp(Package)](FCachePutRefResponse&& Response) mutable
	{
		return Self->BeginPutBlobs(MoveTemp(Package), MoveTemp(Response));
	});
}

void FHttpCacheStore::FPutPackageOp::BeginPutRef(bool bFinalize, FOnCachePutRefComplete&& OnComplete)
{
	FRequestTimer RequestTimer(RequestStats);

	TAnsiStringBuilder<64> Bucket;
	Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);

	TAnsiStringBuilder<256> RefsUri;
	RefsUri << CacheStore.EffectiveDomain << ANSITEXTVIEW("/api/v1/refs/") << CacheStore.Namespace << '/' << Bucket << '/' << Key.Hash;
	if (bFinalize)
	{
		RefsUri << ANSITEXTVIEW("/finalize/") << ObjectHash;
	}

	TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Put);
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

	RequestTimer.Stop();
	LocalOperation.SendAsync(Owner, [Self = TRefCountPtr(this), Operation = MoveTemp(Operation), bFinalize, OnComplete = MoveTemp(OnComplete)]() mutable
	{
		Operation->GetStats(Self->RequestStats);
		Self->EndPutRef(MoveTemp(Operation), bFinalize, MoveTemp(OnComplete));
	});
}

void FHttpCacheStore::FPutPackageOp::EndPutRef(
	TUniquePtr<FHttpOperation> Operation,
	bool bFinalize,
	FOnCachePutRefComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_PutPackage_EndPutRef);

	if (const int32 StatusCode = Operation->GetStatusCode(); StatusCode < 200 || StatusCode > 204)
	{
		const EStatus Status = Operation->GetErrorCode() == EHttpErrorCode::Canceled ? EStatus::Canceled : EStatus::Error;
		return OnComplete({{}, Status});
	}

	FRequestTimer RequestTimer(RequestStats);

	TArray<FIoHash> NeededBlobHashes;

	// Useful when debugging issues related to compressed/uncompressed blobs being returned from Jupiter
	static const bool bHttpCacheAlwaysPut = FParse::Param(FCommandLine::Get(), TEXT("HttpCacheAlwaysPut"));

	if (bHttpCacheAlwaysPut && !bFinalize)
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

	RequestTimer.Stop();
	OnComplete({NeededBlobHashes, EStatus::Ok});
}

void FHttpCacheStore::FPutPackageOp::BeginPutBlobs(FCbPackage&& Package, FCachePutRefResponse&& Response)
{
	if (Response.Status != EStatus::Ok)
	{
		if (Response.Status == EStatus::Error)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Failed to put reference object for put of %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		}
		return EndPut(Response.Status);
	}

	FRequestTimer RequestTimer(RequestStats);

	// TODO: blob uploading and finalization should be replaced with a single batch compressed blob upload endpoint in the future.
	TStringBuilder<128> ExpectedHashes;
	bool bExpectedHashesSerialized = false;

	// Needed blob upload (if any missing)
	TArray<FCompressedBuffer> Blobs;
	for (const FIoHash& NeededBlobHash : Response.NeededBlobHashes)
	{
		if (const FCbAttachment* Attachment = Package.FindAttachment(NeededBlobHash))
		{
			FCompressedBuffer Blob;
			if (Attachment->IsCompressedBinary())
			{
				Blob = Attachment->AsCompressedBinary();
			}
			else if (Attachment->IsBinary())
			{
				Blob = FValue::Compress(Attachment->AsCompositeBinary()).GetData();
			}
			else
			{
				Blob = FValue::Compress(Attachment->AsObject().GetBuffer()).GetData();
			}
			Blobs.Emplace(MoveTemp(Blob));
		}
		else
		{
			if (!bExpectedHashesSerialized)
			{
				for (const FCbAttachment& PackageAttachment : Package.GetAttachments())
				{
					ExpectedHashes << PackageAttachment.GetHash() << TEXTVIEW(", ");
				}
				if (ExpectedHashes.Len() >= 2)
				{
					ExpectedHashes.RemoveSuffix(2);
				}
				bExpectedHashesSerialized = true;
			}
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Server reported needed hash '%s' that is outside the set of expected hashes (%s) for put of %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(NeededBlobHash), *ExpectedHashes, *WriteToString<96>(Key), *Name);
		}
	}

	if (Blobs.IsEmpty())
	{
		RequestTimer.Stop();
		return EndPut(EStatus::Ok);
	}

	TotalBlobUploads = Blobs.Num();
	PendingBlobUploads.store(TotalBlobUploads, std::memory_order_relaxed);

	FRequestBarrier Barrier(Owner);
	for (const FCompressedBuffer& Blob : Blobs)
	{
		TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Put);
		FHttpOperation& LocalOperation = *Operation;
		LocalOperation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/api/v1/compressed-blobs/"), CacheStore.Namespace, '/', Blob.GetRawHash()));
		LocalOperation.SetMethod(EHttpMethod::Put);
		LocalOperation.SetContentType(EHttpMediaType::CompressedBinary);
		LocalOperation.SetBody(Blob.GetCompressed());
		LocalOperation.SendAsync(Owner, [Self = TRefCountPtr(this), Operation = MoveTemp(Operation), LogicalSize = Blob.GetRawSize()]
		{
			Operation->GetStats(Self->RequestStats);
			Self->EndPutBlob(*Operation, LogicalSize);
		});
	}
}

void FHttpCacheStore::FPutPackageOp::EndPutBlob(FHttpOperation& Operation, uint64 LogicalSize)
{
	const int32 StatusCode = Operation.GetStatusCode();
	if (StatusCode >= 200 && StatusCode <= 204)
	{
		SuccessfulBlobUploads.fetch_add(1, std::memory_order_relaxed);
		TUniqueLock Lock(RequestStats.Mutex);
		RequestStats.LogicalWriteSize += LogicalSize;
	}

	if (PendingBlobUploads.fetch_sub(1, std::memory_order_relaxed) == 1)
	{
		const uint32 LocalSuccessfulBlobUploads = SuccessfulBlobUploads.load(std::memory_order_relaxed);
		if (Owner.IsCanceled())
		{
			EndPut(EStatus::Canceled);
		}
		else if (LocalSuccessfulBlobUploads == TotalBlobUploads)
		{
			BeginPutRef(/*bFinalize*/ true, [Self = TRefCountPtr(this)](FCachePutRefResponse&& Response)
			{
				return Self->EndPutRefFinalize(MoveTemp(Response));
			});
		}
		else
		{
			const uint32 FailedBlobUploads = TotalBlobUploads - LocalSuccessfulBlobUploads;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Failed to put %d/%d blobs for put of %s from '%s'"),
				*CacheStore.Domain, FailedBlobUploads, TotalBlobUploads, *WriteToString<96>(Key), *Name);
			EndPut(EStatus::Error);
		}
	}
}

void FHttpCacheStore::FPutPackageOp::EndPutRefFinalize(FCachePutRefResponse&& Response)
{
	if (Response.Status == EStatus::Error)
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Failed to finalize reference object for put of %s from '%s'"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
	}

	return EndPut(Response.Status);
}

void FHttpCacheStore::FPutPackageOp::EndPut(EStatus Status)
{
	RequestStats.EndTime = FMonotonicTimePoint::Now();
	RequestStats.Status = Status;
	OnPackageComplete({Status});
	if (CacheStore.StoreStats)
	{
		CacheStore.StoreStats->AddRequest(RequestStats);
	}
}

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore::FGetRecordOp
//----------------------------------------------------------------------------------------------------------
class FHttpCacheStore::FGetRecordOp final : public FThreadSafeRefCountedObject
{
public:
	static TRefCountPtr<FGetRecordOp> New(FHttpCacheStore& CacheStore, IRequestOwner& Owner, const FSharedString& Name)
	{
		return new FGetRecordOp(CacheStore, Owner, Name);
	}

	struct FRecordResponse
	{
		FCacheRecord Record;
		EStatus Status = EStatus::Error;
	};
	using FOnRecordComplete = TUniqueFunction<void (FRecordResponse&& Response)>;

	void GetRecordOnly(const FCacheKey& Key, const ECachePolicy RecordPolicy, FOnRecordComplete&& OnComplete);
	void GetRecord(const FCacheKey& Key, const FCacheRecordPolicy& Policy, FOnRecordComplete&& OnComplete);

	struct FValueResponse
	{
		FValueWithId Value;
		EStatus Status = EStatus::Error;
	};
	using FOnValueComplete = TUniqueFunction<void (FValueResponse&& Response)>;

	void GetValues(TConstArrayView<FValueWithId> Values, FOnValueComplete&& OnComplete);
	void GetValuesExist(TConstArrayView<FValueWithId> Values, FOnValueComplete&& OnComplete);

	const FRequestStats& ReadStats() const { return RequestStats; }
	FRequestStats& EditStats() { return RequestStats; }
	void RecordStats(EStatus Status);

private:
	FGetRecordOp(FHttpCacheStore& CacheStore, IRequestOwner& Owner, const FSharedString& Name);

	void EndGetRef(TUniquePtr<FHttpOperation> Operation);

	void BeginGetValues(const FCacheRecord& Record, const FCacheRecordPolicy& Policy, FOnRecordComplete&& OnComplete);
	void EndGetValues(const FCacheRecordPolicy& Policy, EStatus Status);

	FHttpCacheStore& CacheStore;
	IRequestOwner& Owner;
	FSharedString Name;
	FCacheKey Key;
	FCbPackage Package;
	FOnRecordComplete OnRecordComplete;

	FRequestStats RequestStats;

	int32 PendingValues = 0;
	int32 FailedValues = 0;
	mutable FMutex Mutex;
};

FHttpCacheStore::FGetRecordOp::FGetRecordOp(FHttpCacheStore& InCacheStore, IRequestOwner& InOwner, const FSharedString& InName)
	: CacheStore(InCacheStore)
	, Owner(InOwner)
	, Name(InName)
{
	RequestStats.Name = Name;
}

void FHttpCacheStore::FGetRecordOp::GetRecordOnly(const FCacheKey& InKey, const ECachePolicy RecordPolicy, FOnRecordComplete&& InOnComplete)
{
	FRequestTimer RequestTimer(RequestStats);

	if (!CacheStore.IsUsable())
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped get of %s from '%s' because this cache store is not available"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		return InOnComplete({FCacheRecordBuilder(Key).Build(), EStatus::Error});
	}

	// Skip the request if querying the cache is disabled.
	if (!EnumHasAnyFlags(RecordPolicy, ECachePolicy::QueryRemote))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped get of %s from '%s' due to cache policy"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		return InOnComplete({FCacheRecordBuilder(Key).Build(), EStatus::Error});
	}

	if (CacheStore.DebugOptions.ShouldSimulateGetMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%s'"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		return InOnComplete({FCacheRecordBuilder(Key).Build(), EStatus::Error});
	}

	Key = InKey;
	OnRecordComplete = MoveTemp(InOnComplete);
	RequestStats.Bucket = Key.Bucket;

	TAnsiStringBuilder<64> Bucket;
	Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);

	TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Get);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/api/v1/refs/"), CacheStore.Namespace, '/', Bucket, '/', Key.Hash));
	LocalOperation.SetMethod(EHttpMethod::Get);
	LocalOperation.AddAcceptType(EHttpMediaType::CbObject);
	LocalOperation.SetExpectedErrorCodes({404});

	RequestTimer.Stop();
	LocalOperation.SendAsync(Owner, [Self = TRefCountPtr(this), Operation = MoveTemp(Operation)]() mutable
	{
		Operation->GetStats(Self->RequestStats);
		Self->EndGetRef(MoveTemp(Operation));
	});
}

void FHttpCacheStore::FGetRecordOp::EndGetRef(TUniquePtr<FHttpOperation> Operation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetPackage_EndGetRef);

	FRequestTimer RequestTimer(RequestStats);

	FOptionalCacheRecord Record;
	EStatus Status = EStatus::Error;
	ON_SCOPE_EXIT
	{
		Operation.Reset();
		if (Record.IsNull())
		{
			Record = FCacheRecordBuilder(Key).Build();
		}
		RequestTimer.Stop();
		FOnRecordComplete LocalOnComplete = MoveTemp(OnRecordComplete);
		LocalOnComplete({MoveTemp(Record).Get(), Status});
	};

	const int32 StatusCode = Operation->GetStatusCode();
	if (StatusCode < 200 || StatusCode > 204)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with missing package for %s from '%s'"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		return;
	}

	FSharedBuffer Body = Operation->GetBody();

	if (ValidateCompactBinary(Body, ECbValidateMode::Default) != ECbValidateError::None)
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache miss with invalid package for %s from '%s'"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		return;
	}

	Package = FCbPackage(FCbObject(Body));
	Record = FCacheRecord::Load(Package);

	if (Record.IsNull())
	{
		UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache miss with record load failure for %s from '%s'"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		return;
	}

	Status = EStatus::Ok;
}

void FHttpCacheStore::FGetRecordOp::GetRecord(const FCacheKey& LocalKey, const FCacheRecordPolicy& Policy, FOnRecordComplete&& OnComplete)
{
	GetRecordOnly(LocalKey, Policy.GetRecordPolicy(), [Self = TRefCountPtr(this), Policy, OnComplete = MoveTemp(OnComplete)](FRecordResponse&& Response) mutable
	{
		if (Response.Status == EStatus::Ok)
		{
			Self->BeginGetValues(Response.Record, Policy, MoveTemp(OnComplete));
		}
		else
		{
			OnComplete(MoveTemp(Response));
		}
	});
}

void FHttpCacheStore::FGetRecordOp::BeginGetValues(const FCacheRecord& Record, const FCacheRecordPolicy& Policy, FOnRecordComplete&& OnComplete)
{
	FRequestTimer RequestTimer(RequestStats);

	OnRecordComplete = MoveTemp(OnComplete);

	TArray<FValueWithId> RequiredGets;
	TArray<FValueWithId> RequiredHeads;

	for (const FValueWithId& Value : Record.GetValues())
	{
		const ECachePolicy ValuePolicy = Policy.GetValuePolicy(Value.GetId());
		if (EnumHasAnyFlags(ValuePolicy, ECachePolicy::QueryRemote))
		{
			(EnumHasAnyFlags(ValuePolicy, ECachePolicy::SkipData) ? RequiredHeads : RequiredGets).Emplace(Value);
		}
	}

	PendingValues = RequiredGets.Num() + RequiredHeads.Num();

	RequestTimer.Stop();

	if (PendingValues == 0)
	{
		return EndGetValues(Policy, EStatus::Ok);
	}

	GetValues(RequiredGets, [Self = TRefCountPtr(this), Policy](FValueResponse&& Response)
	{
		TDynamicUniqueLock Lock(Self->Mutex);
		const bool bComplete = --Self->PendingValues == 0;
		if (Response.Value.HasData())
		{
			Self->Package.AddAttachment(FCbAttachment(Response.Value.GetData()));
		}
		else
		{
			++Self->FailedValues;
		}
		if (bComplete)
		{
			Lock.Unlock();
			Self->EndGetValues(Policy, Response.Status);
		}
	});

	GetValuesExist(RequiredHeads, [Self = TRefCountPtr(this), Policy](FValueResponse&& Response)
	{
		TDynamicUniqueLock Lock(Self->Mutex);
		const bool bComplete = --Self->PendingValues == 0;
		if (Response.Status != EStatus::Ok)
		{
			++Self->FailedValues;
		}
		if (bComplete)
		{
			Lock.Unlock();
			Self->EndGetValues(Policy, Response.Status);
		}
	});
}

void FHttpCacheStore::FGetRecordOp::EndGetValues(const FCacheRecordPolicy& Policy, EStatus Status)
{
	FCacheRecordBuilder RecordBuilder(Key);
	if (FOptionalCacheRecord Record = FCacheRecord::Load(Package))
	{
		if (!EnumHasAnyFlags(Policy.GetRecordPolicy(), ECachePolicy::SkipMeta))
		{
			RecordBuilder.SetMeta(CopyTemp(Record.Get().GetMeta()));
		}
		for (const FValueWithId& Value : Record.Get().GetValues())
		{
			const ECachePolicy ValuePolicy = Policy.GetValuePolicy(Value.GetId());
			if (EnumHasAnyFlags(ValuePolicy, ECachePolicy::QueryRemote) && !EnumHasAnyFlags(ValuePolicy, ECachePolicy::SkipData))
			{
				if (Status == EStatus::Ok && !Value.HasData())
				{
					Status = EStatus::Error;
				}
				RecordBuilder.AddValue(Value);
			}
			else
			{
				RecordBuilder.AddValue(Value.RemoveData());
			}
		}
	}

	if (FailedValues)
	{
		Status = EStatus::Error;
	}

	FOnRecordComplete LocalOnComplete = MoveTemp(OnRecordComplete);
	LocalOnComplete({RecordBuilder.Build(), Status});
}

void FHttpCacheStore::FGetRecordOp::GetValues(TConstArrayView<FValueWithId> Values, FOnValueComplete&& OnComplete)
{
	int32 MissingDataCount = 0;
	for (const FValueWithId& Value : Values)
	{
		if (Value.HasData())
		{
			OnComplete({Value, EStatus::Ok});
			continue;
		}
		++MissingDataCount;
	}

	if (MissingDataCount == 0)
	{
		return;
	}

	// TODO: Jupiter does not currently provide a batched GET. Once it does, fetch every blob in one request.

	FRequestTimer RequestTimer(RequestStats);

	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnValueComplete> SharedOnComplete = MakeShared<FOnValueComplete>(MoveTemp(OnComplete));
	for (const FValueWithId& Value : Values)
	{
		if (Value.HasData())
		{
			continue;
		}

		TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Get);
		FHttpOperation& LocalOperation = *Operation;
		LocalOperation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/api/v1/compressed-blobs/"), CacheStore.Namespace, '/', Value.GetRawHash()));
		LocalOperation.SetMethod(EHttpMethod::Get);
		LocalOperation.AddAcceptType(EHttpMediaType::Any);
		LocalOperation.SetExpectedErrorCodes({404});
		LocalOperation.SendAsync(Owner, [Self = TRefCountPtr(this), Operation = MoveTemp(Operation), SharedOnComplete, Id = Value.GetId(), RawHash = Value.GetRawHash(), RawSize = Value.GetRawSize()]
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetPackage_GetValues_OnResponse);

			FRequestTimer RequestTimer(Self->RequestStats);
			Operation->GetStats(Self->RequestStats);

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

				TUniqueLock Lock(Self->RequestStats.Mutex);
				Self->RequestStats.LogicalReadSize += CompressedBuffer.GetRawSize();
			}

			RequestTimer.Stop();

			if (bHit)
			{
				if (CompressedBuffer.GetRawHash() == RawHash && CompressedBuffer.GetRawSize() == RawSize)
				{
					SharedOnComplete.Get()({FValueWithId(Id, MoveTemp(CompressedBuffer)), EStatus::Ok});
				}
				else
				{
					UE_LOG(LogDerivedDataCache, Display,
						TEXT("%s: Cache miss with corrupted value %s with hash %s for %s from '%s'"),
						*Self->CacheStore.Domain, *WriteToString<32>(Id), *WriteToString<48>(RawHash),
						*WriteToString<96>(Self->Key), *Self->Name);
					SharedOnComplete.Get()({FValueWithId(Id, RawHash, RawSize), EStatus::Error});
				}
			}
			else if (Operation->GetErrorCode() == EHttpErrorCode::Canceled)
			{
				SharedOnComplete.Get()({FValueWithId(Id, RawHash, RawSize), EStatus::Canceled});
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Verbose,
					TEXT("%s: Cache miss with missing value %s with hash %s for %s from '%s'"),
					*Self->CacheStore.Domain, *WriteToString<32>(Id), *WriteToString<48>(RawHash),
					*WriteToString<96>(Self->Key), *Self->Name);
				SharedOnComplete.Get()({FValueWithId(Id, RawHash, RawSize), EStatus::Error});
			}
		});
	}
}

void FHttpCacheStore::FGetRecordOp::GetValuesExist(TConstArrayView<FValueWithId> Values, FOnValueComplete&& OnComplete)
{
	TArray<FValueWithId> QueryValues;
	for (const FValueWithId& Value : Values)
	{
		if (Value.HasData())
		{
			OnComplete({Value, EStatus::Ok});
			continue;
		}
		QueryValues.Emplace(Value);
	}

	if (QueryValues.IsEmpty())
	{
		return;
	}

	FRequestTimer RequestTimer(RequestStats);

	TAnsiStringBuilder<256> Uri;
	Uri << CacheStore.EffectiveDomain << ANSITEXTVIEW("/api/v1/compressed-blobs/") << CacheStore.Namespace << ANSITEXTVIEW("/exists?");
	for (const FValueWithId& Value : QueryValues)
	{
		Uri << ANSITEXTVIEW("id=") << Value.GetRawHash() << '&';
	}
	Uri.RemoveSuffix(1);

	FRequestBarrier Barrier(Owner);
	TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Get);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(Uri);
	LocalOperation.SetMethod(EHttpMethod::Post);
	LocalOperation.SetContentType(EHttpMediaType::FormUrlEncoded);
	LocalOperation.AddAcceptType(EHttpMediaType::Json);

	RequestTimer.Stop();
	LocalOperation.SendAsync(Owner, [Self = TRefCountPtr(this), Operation = MoveTemp(Operation), Values = MoveTemp(QueryValues), OnComplete = MoveTemp(OnComplete)]() mutable
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_DataProbablyExistsBatch_OnHttpRequestComplete);

		FRequestTimer RequestTimer(Self->RequestStats);
		Operation->GetStats(Self->RequestStats);

		const TCHAR* DefaultMessage = TEXT("Cache exists miss for");
		EStatus DefaultStatus = EStatus::Error;

		if (Operation->GetErrorCode() == EHttpErrorCode::Canceled)
		{
			DefaultMessage = TEXT("Cache exists miss with canceled request for");
			DefaultStatus = EStatus::Canceled;
		}
		else if (const int32 StatusCode = Operation->GetStatusCode(); StatusCode < 200 || StatusCode > 204)
		{
			DefaultMessage = TEXT("Cache exists miss with failed response for");
		}
		else if (TSharedPtr<FJsonObject> ResponseObject = Operation->GetBodyAsJson(); !ResponseObject)
		{
			DefaultMessage = TEXT("Cache exists miss with invalid response for");
		}
		else if (TArray<FString> NeedsArrayStrings; ResponseObject->TryGetStringArrayField(TEXT("needs"), NeedsArrayStrings))
		{
			DefaultMessage = TEXT("Cache exists hit for");
			DefaultStatus = EStatus::Ok;

			for (const FString& NeedsString : NeedsArrayStrings)
			{
				const FIoHash NeedHash(NeedsString);
				for (auto It = Values.CreateIterator(); It; ++It)
				{
					const FValueWithId& Value = *It;
					if (Value.GetRawHash() == NeedHash)
					{
						UE_LOG(LogDerivedDataCache, Verbose,
							TEXT("%s: Cache exists miss with missing value %s with hash %s for %s from '%s'"),
							*Self->CacheStore.Domain, *WriteToString<32>(Value.GetId()),
							*WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Self->Key), *Self->Name);
						OnComplete({Value, EStatus::Error});
						It.RemoveCurrentSwap();
						break;
					}
				}
			}
		}

		RequestTimer.Stop();

		for (const FValueWithId& Value : Values)
		{
			UE_LOG(LogDerivedDataCache, Verbose,
				TEXT("%s: %s value %s with hash %s for %s from '%s'"),
				*Self->CacheStore.Domain, DefaultMessage, *WriteToString<32>(Value.GetId()),
				*WriteToString<48>(Value.GetRawHash()), *WriteToString<96>(Self->Key), *Self->Name);
			OnComplete({Value, DefaultStatus});
		}
	});
}

void FHttpCacheStore::FGetRecordOp::RecordStats(EStatus Status)
{
	RequestStats.EndTime = FMonotonicTimePoint::Now();
	RequestStats.Status = Status;
	if (CacheStore.StoreStats)
	{
		CacheStore.StoreStats->AddRequest(RequestStats);
	}
}

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore::FGetValueOp
//----------------------------------------------------------------------------------------------------------
class FHttpCacheStore::FGetValueOp final : public FThreadSafeRefCountedObject
{
public:
	struct FResponse
	{
		const FSharedString& Name;
		const FCacheKey& Key;
		FValue Value;
		EStatus Status = EStatus::Error;
	};
	using FOnComplete = TUniqueFunction<void (FResponse&& Response)>;

	static TRefCountPtr<FGetValueOp> New(FHttpCacheStore& CacheStore, IRequestOwner& Owner, const FSharedString& Name)
	{
		return new FGetValueOp(CacheStore, Owner, Name);
	}

	void Get(const FCacheKey& Key, ECachePolicy Policy, FOnComplete&& OnComplete);

	const FRequestStats& ReadStats() const { return RequestStats; }
	FRequestStats& EditStats() { return RequestStats; }

private:
	FGetValueOp(FHttpCacheStore& CacheStore, IRequestOwner& Owner, const FSharedString& Name);

	void EndGetRef(TUniquePtr<FHttpOperation> Operation);
	void EndGet(FResponse&& Response);

	FHttpCacheStore& CacheStore;
	IRequestOwner& Owner;
	FSharedString Name;
	FCacheKey Key;
	ECachePolicy Policy = ECachePolicy::None;
	FOnComplete OnComplete;
	FRequestStats RequestStats;
};

FHttpCacheStore::FGetValueOp::FGetValueOp(FHttpCacheStore& InCacheStore, IRequestOwner& InOwner, const FSharedString& InName)
	: CacheStore(InCacheStore)
	, Owner(InOwner)
	, Name(InName)
{
	RequestStats.Name = Name;
}

void FHttpCacheStore::FGetValueOp::Get(const FCacheKey& InKey, ECachePolicy InPolicy, FOnComplete&& InOnComplete)
{
	FRequestTimer RequestTimer(RequestStats);

	Key = InKey;
	Policy = InPolicy;
	OnComplete = MoveTemp(InOnComplete);

	const bool bSkipData = EnumHasAnyFlags(Policy, ECachePolicy::SkipData);

	TAnsiStringBuilder<64> Bucket;
	Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);

	TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Get);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/api/v1/refs/"), CacheStore.Namespace, '/', Bucket, '/', Key.Hash));
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

	RequestTimer.Stop();
	LocalOperation.SendAsync(Owner, [Self = TRefCountPtr(this), Operation = MoveTemp(Operation)]() mutable
	{
		Operation->GetStats(Self->RequestStats);
		Self->EndGetRef(MoveTemp(Operation));
	});
}

void FHttpCacheStore::FGetValueOp::EndGetRef(TUniquePtr<FHttpOperation> Operation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_GetValue_EndGetRef);

	const bool bSkipData = EnumHasAnyFlags(Policy, ECachePolicy::SkipData);

	const int32 StatusCode = Operation->GetStatusCode();
	if (StatusCode < 200 || StatusCode > 204)
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with failed HTTP request for %s from '%s'"),
			*CacheStore.Domain, *WriteToString<96>(Key), *Name);
		return EndGet({Name, Key, {}, EStatus::Error});
	}

	FSharedBuffer Body = Operation->GetBody();

	if (bSkipData)
	{
		if (ValidateCompactBinary(Body, ECbValidateMode::Default) != ECbValidateError::None)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Key), *Name);
			return EndGet({Name, Key, {}, EStatus::Error});
		}

		const FCbObjectView Object = FCbObject(Body);
		const FIoHash RawHash = Object["RawHash"].AsHash();
		const uint64 RawSize = Object["RawSize"].AsUInt64(MAX_uint64);
		if (RawHash.IsZero() || RawSize == MAX_uint64)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid value for %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Key), *Name);
			return EndGet({Name, Key, {}, EStatus::Error});
		}

		EndGet({Name, Key, FValue(RawHash, RawSize), EStatus::Ok});
	}
	else
	{
		FCompressedBuffer CompressedBuffer = FCompressedBuffer::FromCompressed(Body);

		if (!CompressedBuffer)
		{
			FRequestTimer RequestTimer(RequestStats);
			if (FAnsiStringView ReceivedHashStr = Operation->GetHeader("X-Jupiter-InlinePayloadHash"); !ReceivedHashStr.IsEmpty())
			{
				FIoHash ReceivedHash(ReceivedHashStr);
				FIoHash ComputedHash = FIoHash::HashBuffer(Body.GetView());
				if (ReceivedHash == ComputedHash)
				{
					CompressedBuffer = FCompressedBuffer::Compress(Body);
				}
			}
		}

		if (!CompressedBuffer)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid package for %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Key), *Name);
			return EndGet({Name, Key, {}, EStatus::Error});
		}

		EndGet({Name, Key, FValue(CompressedBuffer), EStatus::Ok});
	}
}

void FHttpCacheStore::FGetValueOp::EndGet(FResponse&& Response)
{
	RequestStats.LogicalReadSize += Response.Value.GetRawSize();
	RequestStats.EndTime = FMonotonicTimePoint::Now();
	RequestStats.Status = Response.Status;
	OnComplete(MoveTemp(Response));
	if (CacheStore.StoreStats)
	{
		CacheStore.StoreStats->AddRequest(RequestStats);
	}
}

//----------------------------------------------------------------------------------------------------------
// FHttpCacheStore::FExistsBatchOp
//----------------------------------------------------------------------------------------------------------
class FHttpCacheStore::FExistsBatchOp final : public FThreadSafeRefCountedObject
{
public:
	static TRefCountPtr<FExistsBatchOp> New(FHttpCacheStore& CacheStore, IRequestOwner& Owner)
	{
		return new FExistsBatchOp(CacheStore, Owner);
	}

	void Exists(TConstArrayView<FCacheGetValueRequest> Requests, FOnCacheGetValueComplete&& OnComplete);

private:
	FExistsBatchOp(FHttpCacheStore& CacheStore, IRequestOwner& Owner);

	void EndExists(TUniquePtr<FHttpOperation> Operation);

	void EndRequest(const FCacheGetValueRequest& Request, const FValue& Value, EStatus Status);

	FHttpCacheStore& CacheStore;
	IRequestOwner& Owner;
	TArray<FCacheGetValueRequest> Requests;
	FOnCacheGetValueComplete OnComplete;
	FRequestStats RequestStats;
};

FHttpCacheStore::FExistsBatchOp::FExistsBatchOp(FHttpCacheStore& InCacheStore, IRequestOwner& InOwner)
	: CacheStore(InCacheStore)
	, Owner(InOwner)
{
}

void FHttpCacheStore::FExistsBatchOp::Exists(TConstArrayView<FCacheGetValueRequest> InRequests, FOnCacheGetValueComplete&& InOnComplete)
{
	FRequestTimer RequestTimer(RequestStats);

	OnComplete = MoveTemp(InOnComplete);

	Requests.Empty(InRequests.Num());
	for (const FCacheGetValueRequest& Request : InRequests)
	{
		if (!CacheStore.IsUsable())
		{
			UE_LOG(LogDerivedDataCache, VeryVerbose,
				TEXT("%s: Skipped exists check of %s from '%s' because this cache store is not available"),
				*CacheStore.Domain, *WriteToString<96>(Request.Key), *Request.Name);
			OnComplete(Request.MakeResponse(EStatus::Error));
			continue;
		}

		if (!EnumHasAnyFlags(Request.Policy, ECachePolicy::QueryRemote))
		{
			UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped exists check of %s from '%s' due to cache policy"),
				*CacheStore.Domain, *WriteToString<96>(Request.Key), *Request.Name);
			OnComplete(Request.MakeResponse(EStatus::Error));
			continue;
		}

		if (CacheStore.DebugOptions.ShouldSimulateGetMiss(Request.Key))
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for get of %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Request.Key), *Request.Name);
			OnComplete(Request.MakeResponse(EStatus::Error));
			continue;
		}

		Requests.Emplace(Request);
	}

	if (Requests.IsEmpty())
	{
		return;
	}

	FCbWriter BodyWriter;
	BodyWriter.BeginObject();
	BodyWriter.BeginArray(ANSITEXTVIEW("ops"));
	uint32 OpIndex = 0;
	for (const FCacheGetValueRequest& Request : Requests)
	{
		BodyWriter.BeginObject();
		BodyWriter.AddInteger(ANSITEXTVIEW("opId"), OpIndex);
		BodyWriter.AddString(ANSITEXTVIEW("op"), ANSITEXTVIEW("GET"));
		const FCacheKey& Key = Request.Key;
		TAnsiStringBuilder<64> Bucket;
		Algo::Transform(Key.Bucket.ToString(), AppendChars(Bucket), FCharAnsi::ToLower);
		BodyWriter.AddString(ANSITEXTVIEW("bucket"), Bucket);
		BodyWriter.AddString(ANSITEXTVIEW("key"), LexToString(Key.Hash));
		BodyWriter.AddBool(ANSITEXTVIEW("resolveAttachments"), true);
		BodyWriter.EndObject();
		++OpIndex;
	}
	BodyWriter.EndArray();
	BodyWriter.EndObject();
	FCbFieldIterator Body = BodyWriter.Save();

	TUniquePtr<FHttpOperation> Operation = CacheStore.WaitForHttpOperation(EOperationCategory::Get);
	FHttpOperation& LocalOperation = *Operation;
	LocalOperation.SetUri(WriteToAnsiString<256>(CacheStore.EffectiveDomain, ANSITEXTVIEW("/api/v1/refs/"), CacheStore.Namespace));
	LocalOperation.SetMethod(EHttpMethod::Post);
	LocalOperation.SetContentType(EHttpMediaType::CbObject);
	LocalOperation.AddAcceptType(EHttpMediaType::CbObject);
	LocalOperation.SetBody(FCompositeBuffer(Body.GetOuterBuffer()));

	RequestTimer.Stop();
	LocalOperation.SendAsync(Owner, [Self = TRefCountPtr(this), Operation = MoveTemp(Operation)]() mutable
	{
		Operation->GetStats(Self->RequestStats);
		Self->EndExists(MoveTemp(Operation));
	});
}

void FHttpCacheStore::FExistsBatchOp::EndExists(TUniquePtr<FHttpOperation> Operation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_ExistsBatch_EndExists);

	// Divide the stats evenly among the requests.
	RequestStats.PhysicalReadSize /= Requests.Num();
	RequestStats.PhysicalWriteSize /= Requests.Num();
	RequestStats.MainThreadTime = FMonotonicTimeSpan::FromSeconds(RequestStats.MainThreadTime.ToSeconds() / Requests.Num());
	RequestStats.OtherThreadTime = FMonotonicTimeSpan::FromSeconds(RequestStats.OtherThreadTime.ToSeconds() / Requests.Num());
	RequestStats.EndTime = FMonotonicTimePoint::Now();
	RequestStats.Type = ERequestType::Value;
	RequestStats.Op = ERequestOp::Get;

	const int32 OverallStatusCode = Operation->GetStatusCode();
	if (OverallStatusCode < 200 || OverallStatusCode > 204)
	{
		for (const FCacheGetValueRequest& Request : Requests)
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with failed HTTP request for %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Request.Key), *Request.Name);
			RequestStats.Bucket = Request.Key.Bucket;
			EndRequest(Request, {}, EStatus::Error);
		}
		return;
	}

	FMemoryView ResponseView = Operation->GetBody();
	if (ValidateCompactBinary(ResponseView, ECbValidateMode::Default) != ECbValidateError::None)
	{
		for (const FCacheGetValueRequest& Request : Requests)
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache miss with corrupt response for %s from '%s'."),
				*CacheStore.Domain, *WriteToString<96>(Request.Key), *Request.Name);
			RequestStats.Bucket = Request.Key.Bucket;
			EndRequest(Request, {}, EStatus::Error);
		}
		return;
	}

	const FCbObjectView ResponseObject(ResponseView.GetData());
	const FCbArrayView Results = ResponseObject[ANSITEXTVIEW("results")].AsArrayView();

	if (Results.Num() != Requests.Num())
	{
		UE_LOG(LogDerivedDataCache, Log,
			TEXT("%s: Cache exists returned unexpected quantity of results (expected %d, got %d)."),
			*CacheStore.Domain, Requests.Num(), Results.Num());
		for (const FCacheGetValueRequest& Request : Requests)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid response for %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Request.Key), *Request.Name);
			RequestStats.Bucket = Request.Key.Bucket;
			EndRequest(Request, {}, EStatus::Error);
		}
		return;
	}

	for (FCbFieldView ResultField : Results)
	{
		const FCbObjectView ResultObject = ResultField.AsObjectView();
		const uint32 OpId = ResultObject[ANSITEXTVIEW("opId")].AsUInt32();
		const int32 StatusCode = ResultObject[ANSITEXTVIEW("statusCode")].AsInt32();
		const FCbObjectView Value = ResultObject[ANSITEXTVIEW("response")].AsObjectView();

		if (OpId >= (uint32)Requests.Num())
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Encountered invalid opId %d while querying %d values"),
				*CacheStore.Domain, OpId, Requests.Num());
			continue;
		}

		const FCacheGetValueRequest& Request = Requests[int32(OpId)];
		RequestStats.Bucket = Request.Key.Bucket;

		if (StatusCode < 200 || StatusCode > 204)
		{
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache miss with unsuccessful response code %d for %s from '%s'"),
				*CacheStore.Domain, StatusCode, *WriteToString<96>(Request.Key), *Request.Name);
			EndRequest(Request, {}, EStatus::Error);
			continue;
		}

		const FIoHash RawHash = Value[ANSITEXTVIEW("RawHash")].AsHash();
		const uint64 RawSize = Value[ANSITEXTVIEW("RawSize")].AsUInt64(MAX_uint64);
		if (RawHash.IsZero() || RawSize == MAX_uint64)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("%s: Cache miss with invalid value for %s from '%s'"),
				*CacheStore.Domain, *WriteToString<96>(Request.Key), *Request.Name);
			EndRequest(Request, {}, EStatus::Error);
			continue;
		}

		EndRequest(Request, FValue(RawHash, RawSize), EStatus::Ok);
	}
}

void FHttpCacheStore::FExistsBatchOp::EndRequest(const FCacheGetValueRequest& Request, const FValue& Value, EStatus Status)
{
	RequestStats.EndTime = FMonotonicTimePoint::Now();
	RequestStats.Status = Status;
	OnComplete({Request.Name, Request.Key, Value, Request.UserData, Status});
	if (CacheStore.StoreStats)
	{
		CacheStore.StoreStats->AddRequest(RequestStats);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FHttpCacheStore::FHttpCacheStore(const FHttpCacheStoreParams& Params, ICacheStoreOwner* Owner)
	: Domain(Params.Host)
	, Namespace(Params.Namespace)
	, OAuthProvider(Params.OAuthProvider)
	, OAuthClientId(Params.OAuthClientId)
	, OAuthSecret(Params.OAuthSecret)
	, OAuthScope(Params.OAuthScope)
	, OAuthProviderIdentifier(Params.OAuthProviderIdentifier)
	, OAuthAccessToken(Params.OAuthAccessToken)
	, HttpVersion(Params.HttpVersion)
	, StoreOwner(Owner)
	, bReadOnly(Params.bReadOnly)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_Construct);

	// Remove any trailing / because constructing a URI will add one.
	while (Domain.RemoveFromEnd(TEXT("/")));

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

		ClientParams.MaxRequests = UE_HTTPDDC_NONBLOCKING_GET_REQUEST_POOL_SIZE;
		ClientParams.MinRequests = UE_HTTPDDC_NONBLOCKING_GET_REQUEST_POOL_SIZE;
		NonBlockingGetRequestQueue = FHttpRequestQueue(*ConnectionPool, ClientParams);

		ClientParams.MaxRequests = UE_HTTPDDC_NONBLOCKING_PUT_REQUEST_POOL_SIZE;
		ClientParams.MinRequests = UE_HTTPDDC_NONBLOCKING_PUT_REQUEST_POOL_SIZE;
		NonBlockingPutRequestQueue = FHttpRequestQueue(*ConnectionPool, ClientParams);

		bIsUsable = true;

		if (StoreOwner)
		{
			const ECacheStoreFlags Flags = ECacheStoreFlags::Remote | ECacheStoreFlags::Query |
				(Params.bReadOnly ? ECacheStoreFlags::None : ECacheStoreFlags::Store);
			TStringBuilder<256> Path(InPlace, Domain, TEXTVIEW(" ("), Namespace, TEXTVIEW(")"));
			StoreOwner->Add(this, Flags);
			StoreStats = StoreOwner->CreateStats(this, Flags, TEXTVIEW("Unreal Cloud DDC"), Params.Name, Path);

			StoreStats->SetAttribute(TEXTVIEW("Domain"), Domain);
			StoreStats->SetAttribute(TEXTVIEW("EffectiveDomain"), WriteToString<128>(EffectiveDomain));
			StoreStats->SetAttribute(TEXTVIEW("Namespace"), Namespace);
			StoreStats->SetAttribute(TEXTVIEW("LoginAttempts"), WriteToString<16>(LoginAttempts));
			StoreStats->SetAttribute(TEXTVIEW("InteractiveLoginAttempts"), WriteToString<16>(InteractiveLoginAttempts));
			StoreStats->SetAttribute(TEXTVIEW("FailedLoginAttempts"), WriteToString<16>(FailedLoginAttempts));
		}
	}

	AnyInstance = this;
}

FHttpCacheStore::~FHttpCacheStore()
{
	if (RefreshAccessTokenHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RefreshAccessTokenHandle);
	}

	if (StoreStats)
	{
		StoreOwner->DestroyStats(StoreStats);
	}

	if (AnyInstance == this)
	{
		AnyInstance = nullptr;
	}
}

template <typename CharType>
static bool HttpVersionFromString(EHttpVersion& OutVersion, const TStringView<CharType> String)
{
	const auto ConvertedString = StringCast<UTF8CHAR, 16>(String.GetData(), String.Len());
	if (ConvertedString == UTF8TEXTVIEW("none"))
	{
		OutVersion = EHttpVersion::None;
	}
	else if (ConvertedString == UTF8TEXTVIEW("http1.0"))
	{
		OutVersion = EHttpVersion::V1_0;
	}
	else if (ConvertedString == UTF8TEXTVIEW("http1.1"))
	{
		OutVersion = EHttpVersion::V1_1;
	}
	else if (ConvertedString == UTF8TEXTVIEW("http2"))
	{
		OutVersion = EHttpVersion::V2;
	}
	else if (ConvertedString == UTF8TEXTVIEW("http2-only"))
	{
		OutVersion = EHttpVersion::V2Only;
	}
	else
	{
		return false;
	}
	return true;
}

bool TryLexFromString(EHttpVersion& OutVersion, FUtf8StringView String) { return HttpVersionFromString(OutVersion, String); }
bool TryLexFromString(EHttpVersion& OutVersion, FWideStringView String) { return HttpVersionFromString(OutVersion, String); }

FHttpClientParams FHttpCacheStore::GetDefaultClientParams() const
{
	FHttpClientParams ClientParams;
	ClientParams.DnsCacheTimeout = 15;
	ClientParams.ConnectTimeout = 3 * 1000;
	ClientParams.LowSpeedLimit = 1024;
	ClientParams.LowSpeedTime = 10;
	ClientParams.TlsLevel = EHttpTlsLevel::All;
	ClientParams.bFollowRedirects = true;
	ClientParams.bFollow302Post = true;

	EHttpVersion HttpVersionEnum = EHttpVersion::V2;
	TryLexFromString(HttpVersionEnum, HttpVersion);
	ClientParams.Version = HttpVersionEnum;

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

	ON_SCOPE_EXIT
	{
		if (StoreStats)
		{
			StoreStats->SetAttribute(TEXTVIEW("LoginAttempts"), WriteToString<16>(LoginAttempts));
			StoreStats->SetAttribute(TEXTVIEW("InteractiveLoginAttempts"), WriteToString<16>(InteractiveLoginAttempts));
			StoreStats->SetAttribute(TEXTVIEW("FailedLoginAttempts"), WriteToString<16>(FailedLoginAttempts));
		}
	};

	LoginAttempts++;

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
		SetAccessTokenAndUnlock(Lock, OAuthAccessToken);
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
					SetAccessTokenAndUnlock(Lock, AccessTokenString, ExpiryTimeSeconds);
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
		bool bWasInteractiveLogin = false;

		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::TryGet();
		if (DesktopPlatform && DesktopPlatform->GetOidcAccessToken(FPaths::RootDir(), FPaths::GetProjectFilePath(), OAuthProviderIdentifier, FApp::IsUnattended(), GWarn, AccessTokenString, TokenExpiresAt, bWasInteractiveLogin))
		{
			if (bWasInteractiveLogin)
			{
				InteractiveLoginAttempts++;
			}

			const double ExpiryTimeSeconds = (TokenExpiresAt - FDateTime::UtcNow()).GetTotalSeconds();
			UE_LOG(LogDerivedDataCache, Display,
				TEXT("%s: OidcToken: Logged in to HTTP DDC services. Expires at %s which is in %.0f seconds."),
				*Domain, *TokenExpiresAt.ToString(), ExpiryTimeSeconds);
			SetAccessTokenAndUnlock(Lock, AccessTokenString, ExpiryTimeSeconds);
			return true;
		}
		else if (DesktopPlatform)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: OidcToken: Failed to log in to HTTP services."), *Domain);
			FailedLoginAttempts++;
			return false;
		}
		else
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: OidcToken: Use of OAuthProviderIdentifier requires that the target depend on DesktopPlatform."), *Domain);
			FailedLoginAttempts++;
			return false;
		}
	}

	UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: No available configuration to acquire an access token."), *Domain);
	FailedLoginAttempts++;
	return false;
}

void FHttpCacheStore::SetAccessTokenAndUnlock(FScopeLock& Lock, FStringView Token, double RefreshDelay)
{
	// Cache the expired refresh handle.
	FTSTicker::FDelegateHandle ExpiredRefreshAccessTokenHandle = MoveTemp(RefreshAccessTokenHandle);
	RefreshAccessTokenHandle.Reset();

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

	// Unlock the critical section before attempting to remove the expired refresh handle.
	// The associated ticker delegate could already be executing, which could cause a
	// hang in RemoveTicker when the critical section is locked.
	Lock.Unlock();
	if (ExpiredRefreshAccessTokenHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(MoveTemp(ExpiredRefreshAccessTokenHandle));
	}
}

TUniquePtr<FHttpCacheStore::FHttpOperation> FHttpCacheStore::WaitForHttpOperation(EOperationCategory Category)
{
	if (Access && RefreshAccessTokenTime > 0.0 && RefreshAccessTokenTime < FPlatformTime::Seconds())
	{
		AcquireAccessToken();
	}

	THttpUniquePtr<IHttpRequest> Request;

	FHttpRequestParams Params;
	if (FPlatformProcess::SupportsMultithreading() && bHttpEnableAsync)
	{
		if (Category == EOperationCategory::Get)
		{
			Request = NonBlockingGetRequestQueue.CreateRequest(Params);
		}
		else
		{
			Request = NonBlockingPutRequestQueue.CreateRequest(Params);
		}
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

void FHttpCacheStore::PutCacheRecordAsync(IRequestOwner& Owner, const FCachePutRequest& Request, FOnCachePutComplete&& OnComplete)
{
	const FCacheKey& Key = Request.Record.GetKey();

	if (bReadOnly)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped put of %s from '%s' because this cache store is read-only"),
			*Domain, *WriteToString<96>(Key), *Request.Name);
		return OnComplete(Request.MakeResponse(EStatus::Error));
	}

	// Skip the request if storing to the cache is disabled.
	const ECachePolicy RecordPolicy = Request.Policy.GetRecordPolicy();
	if (!EnumHasAnyFlags(RecordPolicy, ECachePolicy::StoreRemote))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped put of %s from '%s' due to cache policy"),
			*Domain, *WriteToString<96>(Key), *Request.Name);
		return OnComplete(Request.MakeResponse(EStatus::Error));
	}

	if (DebugOptions.ShouldSimulatePutMiss(Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%s'"),
			*Domain, *WriteToString<96>(Key), *Request.Name);
		return OnComplete(Request.MakeResponse(EStatus::Error));
	}

	// TODO: Jupiter currently always overwrites.  It doesn't have a "write if not present" feature (for records or attachments),
	//		 but would require one to implement all policy correctly.

	TRefCountPtr<FPutPackageOp> Op = FPutPackageOp::New(*this, Owner, Request.Name);

	FCbPackage Package;
	{
		FRequestStats& RequestStats = Op->EditStats();
		RequestStats.Bucket = Key.Bucket;
		RequestStats.Type = ERequestType::Record;
		RequestStats.Op = ERequestOp::Put;

		FRequestTimer RequestTimer(RequestStats);
		Package = Request.Record.Save();
	}

	Op->Put(Key, MoveTemp(Package), [Op, Request, OnComplete = MoveTemp(OnComplete)](FPutPackageOp::FResponse&& Response)
	{
		if (Response.Status == EStatus::Ok)
		{
			if (const FCbObject& Meta = Request.Record.GetMeta())
			{
				Op->EditStats().LogicalWriteSize += Meta.GetSize();
			}
		}
		TRACE_COUNTER_ADD(HttpDDC_BytesReceived, Op->ReadStats().PhysicalReadSize);
		TRACE_COUNTER_ADD(HttpDDC_BytesSent, Op->ReadStats().PhysicalWriteSize);
		OnComplete(Request.MakeResponse(Response.Status));
	});
}

void FHttpCacheStore::PutCacheValueAsync(IRequestOwner& Owner, const FCachePutValueRequest& Request, FOnCachePutValueComplete&& OnComplete)
{
	if (bReadOnly)
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose,
			TEXT("%s: Skipped put of %s from '%s' because this cache store is read-only"),
			*Domain, *WriteToString<96>(Request.Key), *Request.Name);
		return OnComplete(Request.MakeResponse(EStatus::Error));
	}

	// Skip the request if storing to the cache is disabled.
	if (!EnumHasAnyFlags(Request.Policy, ECachePolicy::StoreRemote))
	{
		UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("%s: Skipped put of %s from '%s' due to cache policy"),
			*Domain, *WriteToString<96>(Request.Key), *Request.Name);
		return OnComplete(Request.MakeResponse(EStatus::Error));
	}

	if (DebugOptions.ShouldSimulatePutMiss(Request.Key))
	{
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Simulated miss for put of %s from '%s'"),
			*Domain, *WriteToString<96>(Request.Key), *Request.Name);
		return OnComplete(Request.MakeResponse(EStatus::Error));
	}

	// TODO: Jupiter currently always overwrites.  It doesn't have a "write if not present" feature (for records or attachments),
	//		 but would require one to implement all policy correctly.

	TRefCountPtr<FPutPackageOp> Op = FPutPackageOp::New(*this, Owner, Request.Name);

	FCbPackage Package;
	{
		FRequestStats& RequestStats = Op->EditStats();
		RequestStats.Bucket = Request.Key.Bucket;
		RequestStats.Type = ERequestType::Value;
		RequestStats.Op = ERequestOp::Put;

		FRequestTimer RequestTimer(RequestStats);

		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddBinaryAttachment("RawHash", Request.Value.GetRawHash());
		Writer.AddInteger("RawSize", Request.Value.GetRawSize());
		Writer.EndObject();

		Package.SetObject(Writer.Save().AsObject());
		Package.AddAttachment(FCbAttachment(Request.Value.GetData()));
	}

	Op->Put(Request.Key, MoveTemp(Package), [Op, Request, OnComplete = MoveTemp(OnComplete)](FPutPackageOp::FResponse&& Response)
	{
		TRACE_COUNTER_ADD(HttpDDC_BytesReceived, Op->ReadStats().PhysicalReadSize);
		TRACE_COUNTER_ADD(HttpDDC_BytesSent, Op->ReadStats().PhysicalWriteSize);
		OnComplete(Request.MakeResponse(Response.Status));
	});
}

void FHttpCacheStore::GetCacheValueAsync(
	IRequestOwner& Owner,
	FSharedString Name,
	const FCacheKey& Key,
	ECachePolicy Policy,
	ERequestOp RequestOp,
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

	TRefCountPtr<FGetValueOp> Op = FGetValueOp::New(*this, Owner, Name);

	FRequestStats& RequestStats = Op->EditStats();
	RequestStats.Bucket = Key.Bucket;
	RequestStats.Type = ERequestType::Value;
	RequestStats.Op = RequestOp;

	Op->Get(Key, Policy, [Op, UserData, OnComplete = MoveTemp(OnComplete)](FGetValueOp::FResponse&& Response)
	{
		TRACE_COUNTER_ADD(HttpDDC_BytesReceived, Op->ReadStats().PhysicalReadSize);
		TRACE_COUNTER_ADD(HttpDDC_BytesSent, Op->ReadStats().PhysicalWriteSize);
		OnComplete({Response.Name, Response.Key, MoveTemp(Response.Value), UserData, Response.Status});
	});
}

void FHttpCacheStore::GetCacheRecordAsync(
	IRequestOwner& Owner,
	const FSharedString& Name,
	const FCacheKey& Key,
	const FCacheRecordPolicy& Policy,
	uint64 UserData,
	FOnCacheGetComplete&& OnComplete)
{
	TRefCountPtr<FGetRecordOp> Op = FGetRecordOp::New(*this, Owner, Name);

	FRequestStats& RequestStats = Op->EditStats();
	RequestStats.Bucket = Key.Bucket;
	RequestStats.Type = ERequestType::Record;
	RequestStats.Op = ERequestOp::Get;

	Op->GetRecord(Key, Policy, [Op, Name, UserData, OnComplete = MoveTemp(OnComplete)](FGetRecordOp::FRecordResponse&& Response)
	{
		if (Response.Status == EStatus::Ok)
		{
			if (const FCbObject& Meta = Response.Record.GetMeta())
			{
				Op->EditStats().LogicalReadSize += Meta.GetSize();
			}
		}
		Op->RecordStats(Response.Status);
		TRACE_COUNTER_ADD(HttpDDC_BytesReceived, Op->ReadStats().PhysicalReadSize);
		TRACE_COUNTER_ADD(HttpDDC_BytesSent, Op->ReadStats().PhysicalWriteSize);
		OnComplete({Name, MoveTemp(Response.Record), UserData, Response.Status});
	});
}

void FHttpCacheStore::LegacyStats(FDerivedDataCacheStatsNode& OutNode)
{
	checkNoEntry();
}

void FHttpCacheStore::Put(
	const TConstArrayView<FCachePutRequest> Requests,
	IRequestOwner& Owner,
	FOnCachePutComplete&& OnComplete)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(HttpDDC_Put);
	TRACE_COUNTER_ADD(HttpDDC_Put, Requests.Num());

	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnCachePutComplete> SharedOnComplete = MakeShared<FOnCachePutComplete>(MoveTemp(OnComplete));
	for (const FCachePutRequest& Request : Requests)
	{
		PutCacheRecordAsync(Owner, Request, [SharedOnComplete](FCachePutResponse&& Response)
		{
			if (Response.Status == EStatus::Ok)
			{
				TRACE_COUNTER_INCREMENT(HttpDDC_PutHit);
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
	TRACE_COUNTER_ADD(HttpDDC_Get, Requests.Num());

	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnCacheGetComplete> SharedOnComplete = MakeShared<FOnCacheGetComplete>(MoveTemp(OnComplete));
	for (const FCacheGetRequest& Request : Requests)
	{
		GetCacheRecordAsync(Owner, Request.Name, Request.Key, Request.Policy, Request.UserData,
			[SharedOnComplete](FCacheGetResponse&& Response)
		{
			if (Response.Status == EStatus::Ok)
			{
				TRACE_COUNTER_INCREMENT(HttpDDC_GetHit);
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
	TRACE_COUNTER_ADD(HttpDDC_Put, Requests.Num());

	FRequestBarrier Barrier(Owner);
	TSharedRef<FOnCachePutValueComplete> SharedOnComplete = MakeShared<FOnCachePutValueComplete>(MoveTemp(OnComplete));
	for (const FCachePutValueRequest& Request : Requests)
	{
		PutCacheValueAsync(Owner, Request, [SharedOnComplete](FCachePutValueResponse&& Response)
		{
			if (Response.Status == EStatus::Ok)
			{
				TRACE_COUNTER_INCREMENT(HttpDDC_PutHit);
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
	TRACE_COUNTER_ADD(HttpDDC_Get, Requests.Num());

	const auto HasSkipData = [](const FCacheGetValueRequest& Request) { return EnumHasAnyFlags(Request.Policy, ECachePolicy::SkipData); };
	if (Algo::AllOf(Requests, HasSkipData))
	{
		TRefCountPtr<FExistsBatchOp> Op = FExistsBatchOp::New(*this, Owner);
		Op->Exists(Requests, [this, OnComplete = MoveTemp(OnComplete)](FCacheGetValueResponse&& Response)
		{
			if (Response.Status == EStatus::Ok)
			{
				TRACE_COUNTER_INCREMENT(HttpDDC_GetHit);
				UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
					*Domain, *WriteToString<96>(Response.Key), *Response.Name);
			}
			OnComplete(MoveTemp(Response));
		});
	}
	else
	{
		FRequestBarrier Barrier(Owner);
		TSharedRef<FOnCacheGetValueComplete> SharedOnComplete = MakeShared<FOnCacheGetValueComplete>(MoveTemp(OnComplete));
		for (const FCacheGetValueRequest& Request : Requests)
		{
			GetCacheValueAsync(Owner, Request.Name, Request.Key, Request.Policy, ERequestOp::Get, Request.UserData,
			[this, Policy = Request.Policy, SharedOnComplete](FCacheGetValueResponse&& Response)
			{
				if (Response.Status == EStatus::Ok && !IsValueDataReady(Response.Value, Policy) && !EnumHasAnyFlags(Policy, ECachePolicy::SkipData))
				{
					Response.Status = EStatus::Error;
					// With inline fetching, expect we will always have a value we can use.
					// Even SkipData/Exists can rely on the blob existing if the ref is reported to exist.
					UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Cache miss due to inlining failure for %s from '%s'"),
						*Domain, *WriteToString<96>(Response.Key), *Response.Name);
				}

				if (Response.Status == EStatus::Ok)
				{
					TRACE_COUNTER_INCREMENT(HttpDDC_GetHit);
					UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
						*Domain, *WriteToString<96>(Response.Key), *Response.Name);
				}

				SharedOnComplete.Get()(MoveTemp(Response));
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
	TRACE_COUNTER_ADD(HttpDDC_GetHit, Requests.Num());
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
		if (!(bHasValue && ValueKey == Request.Key && ValueId == Request.Id) || ValueReader.HasSource() < !bExistsOnly)
		{
			ValueStatus = EStatus::Error;
			ValueReader.ResetSource();
			ValueBuffer.Reset();
			ValueKey = {};
			ValueId.Reset();
			Value.Reset();
			bHasValue = false;
			if (Request.Id.IsValid())
			{
				FRequestOwner BlockingOwner(EPriority::Blocking);
				bool bOpUsed = false;
				EStatus OpStatus = EStatus::Ok;
				TRefCountPtr<FGetRecordOp> Op = FGetRecordOp::New(*this, BlockingOwner, Request.Name);
				if (!(Record && Record.Get().GetKey() == Request.Key))
				{
					bOpUsed = true;
					Op->GetRecordOnly(Request.Key, Request.Policy, [&Record, &OpStatus](FGetRecordOp::FRecordResponse&& Response)
					{
						Record = MoveTemp(Response.Record);
						OpStatus = Response.Status;
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
						ValueBuffer = Value.GetData();
						ValueReader.SetSource(ValueBuffer);
					}
					else if (bHasValue)
					{
						bOpUsed = true;
						Op->GetValues({ValueWithId}, [&Value, &OpStatus](FGetRecordOp::FValueResponse&& Response)
						{
							Value = MoveTemp(Response.Value);
							OpStatus = Response.Status;
						});
						BlockingOwner.Wait();

						if (Value.HasData())
						{
							ValueBuffer = Value.GetData();
							ValueReader.SetSource(ValueBuffer);
						}
					}
				}
				if (bOpUsed)
				{
					FRequestStats& RequestStats = Op->EditStats();
					RequestStats.Type = ERequestType::Record;
					RequestStats.Bucket = Request.Key.Bucket;
					RequestStats.Op = ERequestOp::GetChunk;
					Op->RecordStats(OpStatus);
					TRACE_COUNTER_ADD(HttpDDC_BytesReceived, Op->ReadStats().PhysicalReadSize);
					TRACE_COUNTER_ADD(HttpDDC_BytesSent, Op->ReadStats().PhysicalWriteSize);
				}
			}
			else
			{
				ValueKey = Request.Key;

				FRequestOwner BlockingOwner(EPriority::Blocking);
				GetCacheValueAsync(BlockingOwner, Request.Name, Request.Key, Request.Policy, ERequestOp::GetChunk, 0, [&bHasValue, &Value](FCacheGetValueResponse&& Response)
				{
					bHasValue = Response.Status == EStatus::Ok;
					Value = MoveTemp(Response.Value);
				});
				BlockingOwner.Wait();

				if (bHasValue && IsValueDataReady(Value, Request.Policy))
				{
					ValueBuffer = Value.GetData();
					ValueReader.SetSource(ValueBuffer);
				}
			}
		}
		if (bHasValue)
		{
			const uint64 RawOffset = FMath::Min(Value.GetRawSize(), Request.RawOffset);
			const uint64 RawSize = FMath::Min(Value.GetRawSize() - RawOffset, Request.RawSize);
			UE_LOG(LogDerivedDataCache, Verbose, TEXT("%s: Cache hit for %s from '%s'"),
				*Domain, *WriteToString<96>(Request.Key, '/', Request.Id), *Request.Name);
			FSharedBuffer Buffer;
			if (!bExistsOnly)
			{
				Buffer = ValueReader.Decompress(RawOffset, RawSize);
			}
			const EStatus ChunkStatus = bExistsOnly || Buffer.GetSize() == RawSize ? EStatus::Ok : EStatus::Error;
			if (ChunkStatus == EStatus::Ok)
			{
				TRACE_COUNTER_INCREMENT(HttpDDC_GetHit);
			}
			OnComplete({Request.Name, Request.Key, Request.Id, Request.RawOffset,
				RawSize, Value.GetRawHash(), MoveTemp(Buffer), Request.UserData, ChunkStatus});
			continue;
		}

		OnComplete(Request.MakeResponse(EStatus::Error));
	}
}

void FHttpCacheStoreParams::Parse(const TCHAR* NodeName, const TCHAR* Config)
{
	Name = NodeName;

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

	// Http version Params

	FParse::Value(Config, TEXT("HttpVersion="), HttpVersion);
	if (FParse::Value(Config, TEXT("EnvHttpVersionOverride="), OverrideName))
	{
		FString HttpEnv = FPlatformMisc::GetEnvironmentVariable(*OverrideName);
		if (!HttpEnv.IsEmpty())
		{
			HttpVersion = HttpEnv;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found environment override for HttpVersion %s=%s"), NodeName, *OverrideName, *HttpVersion);
		}
	}
	if (FParse::Value(Config, TEXT("CommandLineHttpVersionOverride="), OverrideName))
	{
		if (FParse::Value(FCommandLine::Get(), *(OverrideName + TEXT("=")), HttpVersion))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override for HttpVersion %s=%s"), NodeName, *OverrideName, *HttpVersion);
		}
	}

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
	if (FParse::Value(Config, TEXT("EnvOAuthSecretOverride="), OverrideName))
	{
		FString OAuthSecretEnv = FPlatformMisc::GetEnvironmentVariable(*OverrideName);
		if (!OAuthSecretEnv.IsEmpty())
		{
			OAuthSecret = OAuthSecretEnv;
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found environment override for OAuthSecret %s={SECRET}"), NodeName, *OverrideName);
		}
	}
	if (FParse::Value(Config, TEXT("CommandLineOAuthSecretOverride="), OverrideName))
	{
		if (FParse::Value(FCommandLine::Get(), *(OverrideName + TEXT("=")), OAuthSecret))
		{
			UE_LOG(LogDerivedDataCache, Log, TEXT("%s: Found command line override for OAuthSecret %s={SECRET}"), NodeName, *OverrideName);
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

ILegacyCacheStore* CreateHttpCacheStore(const TCHAR* NodeName, const TCHAR* Config, ICacheStoreOwner* Owner)
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

	if (bValidParams && !Params.Host.StartsWith(TEXT("http://localhost")))
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
		if (TUniquePtr<FHttpCacheStore> Store = MakeUnique<FHttpCacheStore>(Params, Owner); Store->IsUsable())
		{
			return Store.Release();
		}
		UE_LOG(LogDerivedDataCache, Warning, TEXT("%s: Failed to contact the service (%s), will not use it."), NodeName, *Params.Host);
	}
#endif

	return nullptr;
}

ILegacyCacheStore* GetAnyHttpCacheStore(
	FString& OutDomain,
	FString& OutAccessToken,
	FString& OutNamespace)
{
#if WITH_HTTP_DDC_BACKEND
	if (FHttpCacheStore* HttpBackend = FHttpCacheStore::GetAny())
	{
		OutDomain = HttpBackend->GetDomain();
		OutAccessToken = HttpBackend->GetAccessToken();
		OutNamespace = HttpBackend->GetNamespace();
		return HttpBackend;
	}
#endif
	return nullptr;
}

} // UE::DerivedData
