// Copyright Epic Games, Inc. All Rights Reserved.

#include "AdaptiveStreamingPlayerResourceRequest.h"
#include "Player/PlayerSessionServices.h"

#include <Dom/JsonObject.h>
#include <Serialization/JsonReader.h>
#include <Serialization/JsonSerializer.h>
#include <Misc/Base64.h>


namespace Electra
{

FHTTPResourceRequest::FHTTPResourceRequest()
{
	Request = MakeSharedTS<IElectraHttpManager::FRequest>();
	ReceiveBuffer = MakeSharedTS<IElectraHttpManager::FReceiveBuffer>();
	ProgressListener = MakeSharedTS<IElectraHttpManager::FProgressListener>();
	ProgressListener->ProgressDelegate   = IElectraHttpManager::FProgressListener::FProgressDelegate::CreateRaw(this, &FHTTPResourceRequest::HTTPProgressCallback);
	ProgressListener->CompletionDelegate = IElectraHttpManager::FProgressListener::FCompletionDelegate::CreateRaw(this, &FHTTPResourceRequest::HTTPCompletionCallback);
	Request->ProgressListener = ProgressListener;
	Request->ReceiveBuffer = ReceiveBuffer;
}

FHTTPResourceRequest::~FHTTPResourceRequest()
{
	check(!bInCallback);
	Cancel();
	if (bWasAdded && Request.IsValid())
	{
		TSharedPtrTS<IElectraHttpManager> PinnedHTTPManager = HTTPManager.Pin();
		if (PinnedHTTPManager.IsValid())
		{
			PinnedHTTPManager->RemoveRequest(Request, true);
		}
	}
	Request.Reset();
}

bool FHTTPResourceRequest::SetFromJSON(const FString& InJSONParams)
{
	if (InJSONParams.Len())
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InJSONParams);
		TSharedPtr<FJsonObject> JSONParams;
		if (FJsonSerializer::Deserialize(Reader, JSONParams))
		{
			TArray<FString> StringArray;
			FString String;
			// Verb
			if (JSONParams->TryGetStringField(TEXT("verb"), String))
			{
				Verb(String);
				// If POST see if there is base64 encoded post data to send.
				if (String.Equals(TEXT("POST")) && JSONParams->TryGetStringField(TEXT("data"), String))
				{
					TArray<uint8> Data;
					if (FBase64::Decode(String, Data))
					{
						PostData(Data);
					}
				}
			}
			else
			{
				Verb(TEXT("GET"));
			}

			// Custom user agent
			if (JSONParams->TryGetStringField(TEXT("agent"), String))
			{
				UserAgent(String);
			}

			// Accept-encoding
			if (JSONParams->TryGetStringField(TEXT("encoding"), String))
			{
				AcceptEncoding(String);
			}

			// Headers. Must always be an array of strings
			if (JSONParams->TryGetStringArrayField(TEXT("hdrs"), StringArray))
			{
				Headers(StringArray);
				StringArray.Empty();
			}

			// Connection timeout in milliseconds
			int32 TimeOutMS = 0;
			if (JSONParams->TryGetNumberField(TEXT("ctoms"), TimeOutMS))
			{
				ConnectionTimeout(FTimeValue(FTimeValue::MillisecondsToHNS(TimeOutMS)));
			}

			// No-data timeout in milliseconds
			TimeOutMS = 0;
			if (JSONParams->TryGetNumberField(TEXT("ndtoms"), TimeOutMS))
			{
				NoDataTimeout(FTimeValue(FTimeValue::MillisecondsToHNS(TimeOutMS)));
			}
		}
		else
		{
			return false;
		}
	}
	return true;
}


void FHTTPResourceRequest::StartGet(IPlayerSessionServices* InPlayerSessionServices)
{
	HTTPManager = InPlayerSessionServices->GetHTTPManager();
	// Is there a static resource provider that we can try?
	TSharedPtr<IAdaptiveStreamingPlayerResourceProvider, ESPMode::ThreadSafe> StaticResourceProvider = InPlayerSessionServices->GetStaticResourceProvider();
	if (StaticQueryType.IsSet() && StaticResourceProvider.IsValid())
	{
		TSharedPtr<FStaticResourceRequest, ESPMode::ThreadSafe>	StaticRequest = MakeShared<FStaticResourceRequest, ESPMode::ThreadSafe>(AsShared());
		StaticResourceProvider->ProvideStaticPlaybackDataForURL(StaticRequest);
	}
	else
	{
		bWasAdded = true;
		TSharedPtrTS<IElectraHttpManager> PinnedHTTPManager = HTTPManager.Pin();
		if (PinnedHTTPManager.IsValid())
		{
			PinnedHTTPManager->AddRequest(Request, false);
		}
	}
}

void FHTTPResourceRequest::Cancel()
{
	bWasCanceled = true;
	ProgressListener.Reset();
	ReceiveBuffer.Reset();
	if (bWasAdded && Request.IsValid())
	{
		TSharedPtrTS<IElectraHttpManager> PinnedHTTPManager = HTTPManager.Pin();
		if (PinnedHTTPManager.IsValid())
		{
			PinnedHTTPManager->RemoveRequest(Request, true);
		}
		bWasAdded = false;
	}
}


void FHTTPResourceRequest::StaticDataReady()
{
	if (Request.IsValid())
	{
		// Was static data actually set or was there no data provided?
		if (!bStaticDataReady)
		{
			// Do the actual HTTP request now.
			bWasAdded = true;
			TSharedPtrTS<IElectraHttpManager> PinnedHTTPManager = HTTPManager.Pin();
			if (PinnedHTTPManager.IsValid())
			{
				PinnedHTTPManager->AddRequest(Request, false);
			}
		}
		else
		{
			HTTP::FConnectionInfo& ci = Request->ConnectionInfo;
			ci.EffectiveURL = Request->Parameters.URL;
			ci.bIsConnected = true;
			ci.bHaveResponseHeaders  = true;
			ci.bWasAborted = false;
			ci.bHasFinished = true;
			ci.HTTPVersionReceived = 11;
			ci.StatusInfo.HTTPStatus = Request->Parameters.Range.IsSet() ? 206 : 200;
			ci.ContentLength = ci.BytesReadSoFar = ReceiveBuffer.IsValid() ? ReceiveBuffer->Buffer.Num() : 0;

			bInCallback = true;
			CompletedCallback.ExecuteIfBound(AsShared());
			bInCallback = false;
		}
	}
}

int32 FHTTPResourceRequest::HTTPProgressCallback(const IElectraHttpManager::FRequest* InRequest)
{
	return bWasCanceled ? 1 : 0;
}

void FHTTPResourceRequest::HTTPCompletionCallback(const IElectraHttpManager::FRequest* InRequest)
{
	TSharedPtrTS<FHTTPResourceRequest> Self = AsShared();
	if (Self.IsValid())
	{
		if (Request.IsValid())
		{
			const HTTP::FConnectionInfo& ConnInfo = Request->ConnectionInfo;
			if (!ConnInfo.bWasAborted)
			{
				Error = 0;
				if (Request->ConnectionInfo.StatusInfo.ErrorDetail.IsError())
				{
					if (ConnInfo.StatusInfo.ConnectionTimeoutAfterMilliseconds)
					{
						Error = 1;
					}
					else if (ConnInfo.StatusInfo.NoDataTimeoutAfterMilliseconds)
					{
						Error = 2;
					}
					else if (ConnInfo.StatusInfo.bReadError)
					{
						Error = 3;
					}
					else
					{
						Error = ConnInfo.StatusInfo.HTTPStatus ? ConnInfo.StatusInfo.HTTPStatus : 4;
					}
				}

				bInCallback = true;
				CompletedCallback.ExecuteIfBound(Self);
				bInCallback = false;
			}
		}
		bHasFinished = true;
	}
}

} // namespace Electra

