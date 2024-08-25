// Copyright Epic Games, Inc. All Rights Reserved.

#include "DistributionEndpoints.h"

#include "Dom/JsonValue.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HttpManager.h"
#include "HttpModule.h"
#include "IO/IoStoreOnDemand.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeRWLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Statistics.h"

namespace UE::IO::IAS
{

static int32 GDistributedEndpointTimeout = 30;
static FAutoConsoleVariableRef CVar_DistributedEndpointTimeout(
	TEXT("ias.DistributedEndpointTimeout"),
	GDistributedEndpointTimeout,
	TEXT("How long to wait (in seconds) for a distributed endoint resolve request before timing out")
);

FDistributionEndpoints::EResult FDistributionEndpoints::ResolveEndpoints(const FString& DistributionUrl, TArray<FString>& OutServiceUrls)
{
	FEventRef Event;
	return ResolveEndpoints(DistributionUrl, OutServiceUrls, *Event.Get());
}

FDistributionEndpoints::EResult FDistributionEndpoints::ResolveEndpoints(const FString& DistributionUrl, TArray<FString>& OutServiceUrls, FEvent& Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistributionEndpoints::ResolveEndpoints);

	std::atomic_bool bHasResponse = false;
	EResult Result = EResult::Failure;

	// The timeout is a cvar but should not be less than 10 seconds.
	const float HttpTimeout = FMath::Max(static_cast<float>(GDistributedEndpointTimeout), 10.0f);

	UE_LOG(LogIas, Log, TEXT("Resolving distributed endpoint '%s'"), *DistributionUrl);

	FHttpRequestPtr HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetTimeout(HttpTimeout);
	HttpRequest->SetURL(DistributionUrl);
	HttpRequest->SetVerb(TEXT("GET"));
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, &bHasResponse, &Event, &Result, &OutServiceUrls](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bOk)
		{
			LLM_SCOPE_BYTAG(Ias);
			/* Work around a code generation issue in MSVC: forced usage of the request so it is not optimized away */
			UE_LOG(LogIas, VeryVerbose, TEXT("HTTP [%s] request completed"), *Request->GetVerb());
			
			bHasResponse = true;
			Result = ParseResponse(Response, OutServiceUrls);

			Event.Trigger();
		});

	HttpRequest->ProcessRequest();

	const uint32 WaitTime = GDistributedEndpointTimeout >= 0 ? (static_cast<uint32>(GDistributedEndpointTimeout) * 1000) : MAX_uint32;

	if (!Event.Wait(FTimespan::FromSeconds(WaitTime)) || bHasResponse == false)
	{
		HttpRequest->CancelRequest();
		HttpRequest->OnProcessRequestComplete().Unbind();
	}

	UE_CLOG(Result == EResult::Success, LogIas, Log, TEXT("Successfully resolved distributed endpoint '%s' %d urls found"), *DistributionUrl, OutServiceUrls.Num());
	UE_CLOG(Result != EResult::Success, LogIas, Log, TEXT("Failed to resolve distributed endpoint '%s'"), *DistributionUrl);

	return Result;
}

FDistributionEndpoints::EResult FDistributionEndpoints::ParseResponse(FHttpResponsePtr HttpResponse, TArray<FString>& OutUrls)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FDistributionEndpoints::ParseResponse);

	using FJsonValuePtr = TSharedPtr<FJsonValue>;
	using FJsonObjPtr = TSharedPtr<FJsonObject>;
	using FJsonReader = TJsonReader<TCHAR>;
	using FJsonReaderPtr = TSharedRef<FJsonReader>;

	if (HttpResponse != nullptr && HttpResponse->GetResponseCode() == 200)
	{
		FString Json = HttpResponse->GetContentAsString();
		
		FJsonReaderPtr JsonReader = TJsonReaderFactory<TCHAR>::Create(Json);

		FJsonObjPtr JsonObj;
		if (FJsonSerializer::Deserialize(JsonReader, JsonObj))
		{
			if (!JsonObj->HasTypedField< EJson::Array>(TEXT("distributions")))
			{
				return EResult::Failure;
			}

			TArray<FJsonValuePtr> JsonValues = JsonObj->GetArrayField(TEXT("distributions"));
			for (const FJsonValuePtr& JsonValue : JsonValues)
			{
				FString ServiceUrl = JsonValue->AsString();
				if (ServiceUrl.EndsWith(TEXT("/")))
				{
					ServiceUrl.LeftInline(ServiceUrl.Len() - 1);
				}
				OutUrls.Add(MoveTemp(ServiceUrl));
			}

			return !OutUrls.IsEmpty() ? EResult::Success : EResult::Failure;
		}
	}

	return EResult::Failure;
}

} // namespace UE::IO::IAS
