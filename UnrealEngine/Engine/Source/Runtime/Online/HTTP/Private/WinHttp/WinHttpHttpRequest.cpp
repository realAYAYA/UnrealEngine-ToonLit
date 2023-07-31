// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/WinHttpHttpRequest.h"
#include "WinHttp/WinHttpHttpManager.h"
#include "WinHttp/WinHttpHttpResponse.h"
#include "WinHttp/Support/WinHttpConnectionHttp.h"
#include "GenericPlatform/HttpRequestPayload.h"
#include "Http.h"
#include "HttpModule.h"

#include "HAL/PlatformTime.h"
#include "Containers/StringView.h"
#include "HAL/FileManager.h"

FWinHttpHttpRequest::FWinHttpHttpRequest()
{

}

FWinHttpHttpRequest::~FWinHttpHttpRequest()
{
	// Make sure we either didn't start, or we finished before destructing
	check(!RequestStartTimeSeconds.IsSet() || RequestFinishTimeSeconds.IsSet());
}

FString FWinHttpHttpRequest::GetURL() const
{
	return RequestData.Url;
}

FString FWinHttpHttpRequest::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;

	if (TOptional<FString> OptionalParameterValue = FGenericPlatformHttp::GetUrlParameter(RequestData.Url, ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}

	return ReturnValue;
}

FString FWinHttpHttpRequest::GetHeader(const FString& HeaderName) const
{
	const FString* const ExistingHeader = RequestData.Headers.Find(HeaderName);
	return ExistingHeader ? *ExistingHeader : FString();
}

TArray<FString> FWinHttpHttpRequest::GetAllHeaders() const
{
	TArray<FString> AllHeaders;

	for (const TPair<FString, FString>& Header : RequestData.Headers)
	{
		AllHeaders.Add(FString::Printf(TEXT("%s: %s"), *Header.Key, *Header.Value));
	}

	return AllHeaders;
}
	
FString FWinHttpHttpRequest::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

int32 FWinHttpHttpRequest::GetContentLength() const
{
	return RequestData.Payload.IsValid() ? RequestData.Payload->GetContentLength() : 0;
}

const TArray<uint8>& FWinHttpHttpRequest::GetContent() const
{
	static const TArray<uint8> EmptyContent;
	return RequestData.Payload.IsValid() ? RequestData.Payload->GetContent() : EmptyContent;
}

FString FWinHttpHttpRequest::GetVerb() const
{
	return RequestData.Verb;
}

void FWinHttpHttpRequest::SetVerb(const FString& InVerb)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set verb on a request that is inflight"));
		return;
	}

	RequestData.Verb = InVerb.ToUpper();
}

void FWinHttpHttpRequest::SetURL(const FString& InURL)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set URL on a request that is inflight"));
		return;
	}

	RequestData.Url = InURL;
}

void FWinHttpHttpRequest::SetContent(const TArray<uint8>& ContentPayload)
{
	SetContent(CopyTemp(ContentPayload));
}

void FWinHttpHttpRequest::SetContent(TArray<uint8>&& ContentPayload)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return;
	}

	RequestData.Payload = MakeShared<FRequestPayloadInMemory, ESPMode::ThreadSafe>(MoveTemp(ContentPayload));
}

void FWinHttpHttpRequest::SetContentAsString(const FString& ContentString)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return;
	}

	const FTCHARToUTF8 Converter(*ContentString, ContentString.Len());

	TArray<uint8> Content;
	Content.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());

	RequestData.Payload = MakeShared<FRequestPayloadInMemory, ESPMode::ThreadSafe>(MoveTemp(Content));
}

bool FWinHttpHttpRequest::SetContentAsStreamedFile(const FString& Filename)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return false;
	}

	if (FArchive* File = IFileManager::Get().CreateFileReader(*Filename))
	{
		RequestData.Payload = MakeShared<FRequestPayloadInFileStream, ESPMode::ThreadSafe>(MakeShareable(File));
		return true;
	}
	else
	{
		UE_LOG(LogHttp, Warning, TEXT("Failed to open '%s' for reading"), *Filename);
		RequestData.Payload.Reset();
		return false;
	}
}

bool FWinHttpHttpRequest::SetContentFromStream(TSharedRef<FArchive, ESPMode::ThreadSafe> Stream)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set content on a request that is inflight"));
		return false;
	}

	RequestData.Payload = MakeShared<FRequestPayloadInFileStream, ESPMode::ThreadSafe>(Stream);
	return true;
}

void FWinHttpHttpRequest::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set a header on a request that is inflight"));
		return;
	}

	if (HeaderName.IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to set an empty header name"));
		return;
	}

	RequestData.Headers.Add(HeaderName, HeaderValue);
}

void FWinHttpHttpRequest::AppendToHeader(const FString& HeaderName, const FString& AdditionalHeaderValue)
{
	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to append a header on a request that is inflight"));
		return;
	}
	
	if (HeaderName.IsEmpty())
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to append an empty header name"));
		return;
	}

	if (const FString* ExistingHeaderValue = RequestData.Headers.Find(HeaderName))
	{
		RequestData.Headers.Add(HeaderName, FString::Printf(TEXT("%s, %s"), **ExistingHeaderValue, *AdditionalHeaderValue));
	}
	else
	{
		RequestData.Headers.Add(HeaderName, AdditionalHeaderValue);
	}
}

bool FWinHttpHttpRequest::ProcessRequest()
{
	UE_LOG(LogHttp, Verbose, TEXT("FWinHttpHttpRequest::ProcessRequest() FWinHttpHttpRequest=[%p]"), this);

	if (State == EHttpRequestStatus::Processing)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to start request while it is still in inflight"));
		return false;
	}

	FWinHttpHttpManager* HttpManager = FWinHttpHttpManager::GetManager();
	if (!HttpManager)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to start request with no HTTP manager"));
		return false;
	}

	Response.Reset();
	TotalBytesSent = 0;
	TotalBytesReceived = 0;
	RequestStartTimeSeconds.Reset();
	RequestFinishTimeSeconds.Reset();
	bRequestCancelled = false;

	CompletionStatus = EHttpRequestStatus::Processing;
	State = EHttpRequestStatus::Processing;

	TSharedRef<FWinHttpHttpRequest, ESPMode::ThreadSafe> LocalStrongThis = StaticCastSharedRef<FWinHttpHttpRequest>(AsShared());
	HttpManager->QuerySessionForUrl(RequestData.Url, FWinHttpQuerySessionComplete::CreateLambda([LocalWeakThis = TWeakPtr<FWinHttpHttpRequest, ESPMode::ThreadSafe>(LocalStrongThis)](FWinHttpSession* SessionPtr)
	{
		// Validate state
		TSharedPtr<FWinHttpHttpRequest, ESPMode::ThreadSafe> StrongThis = LocalWeakThis.Pin();
		if (!StrongThis.IsValid())
		{
			// We went away
			return;
		}
		if (StrongThis->bRequestCancelled)
		{
			// We were cancelled
			return;
		}
		if (!SessionPtr)
		{
			// Could not create session
			UE_LOG(LogHttp, Warning, TEXT("Unable to create WinHttp Session, failing request"));
			StrongThis->OnWinHttpRequestComplete();
			return;
		}

		FWinHttpHttpRequestData& LocalRequestData = StrongThis->RequestData;

		// Create connection object
		TSharedPtr<FWinHttpConnectionHttp, ESPMode::ThreadSafe> LocalConnection = FWinHttpConnectionHttp::CreateHttpConnection(*SessionPtr, LocalRequestData.Verb, LocalRequestData.Url, LocalRequestData.Headers, LocalRequestData.Payload);
		if (!LocalConnection.IsValid())
		{
			UE_LOG(LogHttp, Warning, TEXT("Unable to create WinHttp Connection, failing request"));
			StrongThis->OnWinHttpRequestComplete();
			return;
		}

		// Bind listeners
		TSharedRef<FWinHttpHttpRequest, ESPMode::ThreadSafe> StrongThisRef = StrongThis.ToSharedRef();
		LocalConnection->SetDataTransferredHandler(FWinHttpConnectionHttpOnDataTransferred::CreateThreadSafeSP(StrongThisRef, &FWinHttpHttpRequest::HandleDataTransferred));
		LocalConnection->SetHeaderReceivedHandler(FWinHttpConnectionHttpOnHeaderReceived::CreateThreadSafeSP(StrongThisRef, &FWinHttpHttpRequest::HandleHeaderReceived));
		LocalConnection->SetRequestCompletedHandler(FWinHttpConnectionHttpOnRequestComplete::CreateThreadSafeSP(StrongThisRef, &FWinHttpHttpRequest::HandleRequestComplete));

		// Start request!
		StrongThisRef->RequestStartTimeSeconds = FPlatformTime::Seconds();
		if (!LocalConnection->StartRequest())
		{
			UE_LOG(LogHttp, Warning, TEXT("Unable to start WinHttp Connection, failing request"));
			StrongThisRef->OnWinHttpRequestComplete();
			return;
		}

		// Save object
		StrongThisRef->Connection = MoveTemp(LocalConnection);
	}));

	// Store our request so it doesn't die if the requester doesn't store it (common use case)
	FHttpModule::Get().GetHttpManager().AddThreadedRequest(LocalStrongThis);
	return true;
}

void FWinHttpHttpRequest::CancelRequest()
{
	UE_LOG(LogHttp, Log, TEXT("FWinHttpHttpRequest::CancelRequest() FWinHttpHttpRequest=[%p]"), this);

	if (EHttpRequestStatus::IsFinished(State))
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to cancel a request that was already finished"));
		return;
	}
	if (bRequestCancelled)
	{
		UE_LOG(LogHttp, Warning, TEXT("Attempted to cancel a request that was already cancelled"));
		return;
	}

	// FinishRequest will cleanup connection
	bRequestCancelled = true;

	FHttpManager& HttpManager = FHttpModule::Get().GetHttpManager();
	if (HttpManager.IsValidRequest(this))
	{
		HttpManager.CancelThreadedRequest(SharedThis(this));
	}
	else if (!IsInGameThread())
	{
		// Always finish on the game thread
		FHttpModule::Get().GetHttpManager().AddGameThreadTask([StrongThis = StaticCastSharedRef<FWinHttpHttpRequest>(AsShared())]()
		{
			StrongThis->FinishRequest();
		});
	}
	else
	{
		FinishRequest();
	}
}

EHttpRequestStatus::Type FWinHttpHttpRequest::GetStatus() const
{
	return CompletionStatus;
}

const FHttpResponsePtr FWinHttpHttpRequest::GetResponse() const
{
	return Response;
}

void FWinHttpHttpRequest::Tick(float DeltaSeconds)
{
	if (Connection.IsValid())
	{
		Connection->PumpMessages();
		// Connection is not guaranteed to be valid anymore here, be sure to check again if it gets used again below
	}
}

float FWinHttpHttpRequest::GetElapsedTime() const
{
	if (!RequestStartTimeSeconds.IsSet())
	{
		// Request hasn't started
		return 0.0f;
	}

	if (RequestFinishTimeSeconds.IsSet())
	{
		// Request finished
		return RequestFinishTimeSeconds.GetValue() - RequestStartTimeSeconds.GetValue();
	}

	// Request still in progress
	return FPlatformTime::Seconds() - RequestStartTimeSeconds.GetValue();
}

bool FWinHttpHttpRequest::StartThreadedRequest()
{
	// No-op, our request is already started
	return true;
}

bool FWinHttpHttpRequest::IsThreadedRequestComplete()
{
	if (bRequestCancelled)
	{
		return true;
	}
	return EHttpRequestStatus::IsFinished(State);
}

void FWinHttpHttpRequest::TickThreadedRequest(float DeltaSeconds)
{
	TSharedPtr<FWinHttpConnectionHttp, ESPMode::ThreadSafe> LocalConnection = Connection;
	if (LocalConnection.IsValid())
	{
		LocalConnection->PumpStates();
		// Connection is not guaranteed to be valid anymore here, be sure to check again if it gets used again below
	}
}

void FWinHttpHttpRequest::OnWinHttpRequestComplete()
{
	if (RequestFinishTimeSeconds.IsSet())
	{
		// Already finished
		return;
	}
	RequestFinishTimeSeconds = FPlatformTime::Seconds();

	// Set our final state if it's not set yet
	if (!EHttpRequestStatus::IsFinished(State))
	{
		State = EHttpRequestStatus::Failed;
	}
}

void FWinHttpHttpRequest::FinishRequest()
{
	check(IsInGameThread());
	check(IsThreadedRequestComplete());

	// If we were cancelled, set our finished time
	if (bRequestCancelled && !RequestFinishTimeSeconds.IsSet())
	{
		RequestFinishTimeSeconds = FPlatformTime::Seconds();
	}

	// Shutdown our connection
	if (Connection.IsValid())
	{
		if (!Connection->IsComplete())
		{
			Connection->CancelRequest();
		}
		Connection.Reset();
	}

	CompletionStatus = State;

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> KeepAlive = AsShared();
	OnProcessRequestComplete().ExecuteIfBound(KeepAlive, Response, Response.IsValid());
}

void FWinHttpHttpRequest::HandleDataTransferred(int32 BytesSent, int32 BytesReceived)
{
	check(IsInGameThread());

	if (BytesSent > 0 || BytesReceived > 0)
	{
		if (BytesReceived > 0)
		{
			UpdateResponseBody();
		}
		TotalBytesSent += BytesSent;
		TotalBytesReceived += BytesReceived;
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> KeepAlive = AsShared();
		OnRequestProgress().ExecuteIfBound(AsShared(), TotalBytesSent, TotalBytesReceived);
	}
}

void FWinHttpHttpRequest::HandleHeaderReceived(const FString& HeaderKey, const FString& HeaderValue)
{
	check(IsInGameThread());

	if (Response.IsValid())
	{
		Response->AppendHeader(HeaderKey, HeaderValue);
	}
	else if (Connection.IsValid())
	{
		Response = MakeShared<FWinHttpHttpResponse, ESPMode::ThreadSafe>(RequestData.Url, Connection->GetResponseCode(), CopyTemp(Connection->GetHeadersReceived()), TArray<uint8>());
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> KeepAlive = AsShared();
	OnHeaderReceived().ExecuteIfBound(AsShared(), HeaderKey, HeaderValue);
}

void FWinHttpHttpRequest::HandleRequestComplete(EHttpRequestStatus::Type RequestCompletionStatus)
{
	check(IsInGameThread());
	check(EHttpRequestStatus::IsFinished(RequestCompletionStatus));

	State = RequestCompletionStatus;

	if (RequestCompletionStatus == EHttpRequestStatus::Succeeded)
	{
		UpdateResponseBody(true);
	}

	OnWinHttpRequestComplete();
}

void FWinHttpHttpRequest::UpdateResponseBody(bool bForceResponseExist)
{
	if (Connection.IsValid())
	{
		TArray<uint8> NewChunk(MoveTemp(Connection->GetLastChunk()));
		if (NewChunk.Num() > 0 || bForceResponseExist)
		{
			if (Response.IsValid())
			{
				if (NewChunk.Num() > 0)
				{
					Response->AppendPayload(NewChunk);
				}
			}
			else
			{
				Response = MakeShared<FWinHttpHttpResponse, ESPMode::ThreadSafe>(RequestData.Url, Connection->GetResponseCode(), CopyTemp(Connection->GetHeadersReceived()), MoveTemp(NewChunk));
			}
		}
	}
}

#endif // WITH_WINHTTP
