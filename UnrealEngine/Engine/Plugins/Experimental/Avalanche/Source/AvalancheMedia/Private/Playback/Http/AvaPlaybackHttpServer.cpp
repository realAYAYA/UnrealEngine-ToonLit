// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playback/Http/AvaPlaybackHttpServer.h"

#include "AvaMediaModule.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "Playback/AvaPlaybackServer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogAvaPlaybackHttpServer)

static const FString& GetHttpRouteVerbString(EHttpServerRequestVerbs InVerbs)
{
	static TMap<EHttpServerRequestVerbs, FString> VerbStrings =
	{
		{ EHttpServerRequestVerbs::VERB_POST, TEXT("POST") },
		{ EHttpServerRequestVerbs::VERB_PUT, TEXT("PUT") },
		{ EHttpServerRequestVerbs::VERB_GET, TEXT("GET") },
		{ EHttpServerRequestVerbs::VERB_PATCH, TEXT("PATCH") },
		{ EHttpServerRequestVerbs::VERB_DELETE, TEXT("DELETE") },
		{ EHttpServerRequestVerbs::VERB_NONE, TEXT("NONE") },
	};

	if (const FString* FoundEntry = VerbStrings.Find(InVerbs))
	{
		return *FoundEntry;		
	}

	static const FString Unknown = TEXT("UNKNOWN");
	return Unknown;
}

FAvaPlaybackHttpRouteBuilder::FAvaPlaybackHttpRouteBuilder(
	const TSharedPtr<FAvaPlaybackHttpServer>& InHttpServer,
	const TSharedPtr<FAvaPlaybackServer>& InPlaybackServer)
	: HttpServer(InHttpServer)
	, PlaybackServer(InPlaybackServer)
{
}

void FAvaPlaybackHttpServer::Start(int32 InPortToUse)
{
	PortToUse = InPortToUse;
	HttpRouter = FHttpServerModule::Get().GetHttpRouter(PortToUse);

	RegisterRoutes();

	FHttpServerModule::Get().StartAllListeners();
}

void FAvaPlaybackHttpServer::RegisterRoutes()
{
	const TSharedPtr<FAvaPlaybackServer> MediaPlaybackServer = FModuleManager::GetModulePtr<FAvaMediaModule>(UE_MODULE_NAME)->GetPlaybackServerInternal();
	check(MediaPlaybackServer.IsValid());
	
	// Map as per FAvaPlaybackServer::Init(const FString& AssignedServerName)
	FAvaPlaybackHttpServer::FBuilder(AsShared(), MediaPlaybackServer)
	.Route<FAvaPlaybackPing>(TEXT("/playback/ping"), EHttpServerRequestVerbs::VERB_POST, &FAvaPlaybackServer::HandlePlaybackPing)
	.Route<FAvaPlaybackDeviceProviderDataRequest>(TEXT("/playback/devices"), EHttpServerRequestVerbs::VERB_POST, &FAvaPlaybackServer::HandleDeviceProviderDataRequest)
	.Route<FAvaPlaybackRequest>(TEXT("/playback"), EHttpServerRequestVerbs::VERB_POST, &FAvaPlaybackServer::HandlePlaybackRequest)
	.Route<FAvaPlaybackAnimPlaybackRequest>(TEXT("/playback/animation"), EHttpServerRequestVerbs::VERB_POST, &FAvaPlaybackServer::HandleAnimPlaybackRequest)
	.Route<FAvaBroadcastRequest>(TEXT("/broadcast"), EHttpServerRequestVerbs::VERB_POST, &FAvaPlaybackServer::HandleBroadcastRequest)
	.Route<FAvaBroadcastStatusRequest>(TEXT("/broadcast/status"), EHttpServerRequestVerbs::VERB_POST, &FAvaPlaybackServer::HandleBroadcastStatusRequest);

	// We always want the ListRegisteredRoutes route bound, no matter what.
	RegisterNewRoute(TEXT("ListRegisteredRoutes"), FHttpPath("/listroutes"), EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateSP(this, &FAvaPlaybackHttpServer::HttpListOpenRoutes), true);
}

bool FAvaPlaybackHttpServer::GetRegisteredRoute(FName RouteName, FAvaPlaybackHttpRouteInfo& OutRouteInfo)
{
	if (RegisteredRoutes.Find(RouteName))
	{
		OutRouteInfo.RouteName = RouteName;
		OutRouteInfo.RoutePath = RegisteredRoutes[RouteName].Handle->Path;
		OutRouteInfo.RequestVerbs = RegisteredRoutes[RouteName].Handle->Verbs;
		OutRouteInfo.InputContentType = RegisteredRoutes[RouteName].InputContentType;
		OutRouteInfo.InputExpectedFormat = RegisteredRoutes[RouteName].InputExpectedFormat;
		return true;
	}
	return false;
}

void FAvaPlaybackHttpServer::RegisterNewRoute(FName RouteName, const FHttpPath& HttpPath, const EHttpServerRequestVerbs& RequestVerbs, const FHttpRequestHandler& Handler, bool bOverrideIfBound /* = false */, FString OptionalContentType /* = TEXT("")*/, FString OptionalExpectedFormat /*= TEXT("")*/)
{
	FAvaPlaybackHttpRouteInfo InRouteInfo;
	InRouteInfo.RouteName = RouteName;
	InRouteInfo.RoutePath = HttpPath;
	InRouteInfo.RequestVerbs = RequestVerbs;
	InRouteInfo.InputContentType = OptionalContentType;
	InRouteInfo.InputExpectedFormat = OptionalExpectedFormat;
	RegisterNewRoute(InRouteInfo, Handler, bOverrideIfBound);
}

void FAvaPlaybackHttpServer::RegisterNewRoute(FAvaPlaybackHttpRouteInfo InRouteInfo, const FHttpRequestHandler& Handler, bool bOverrideIfBound /* = false */)
{
	if (HttpRouter.IsValid())
	{
		if (RegisteredRoutes.Find(InRouteInfo.RouteName))
		{
			if (!bOverrideIfBound)
			{
				UE_LOG(LogAvaPlaybackHttpServer, Error, TEXT("Failed to bind route with friendly key %s - a route at location %s already exists."), *InRouteInfo.RouteName.ToString(), *InRouteInfo.RoutePath.GetPath());
				return;
			}
			UE_LOG(LogAvaPlaybackHttpServer, Log, TEXT("Overwriting route at friendly key %s - from %s to %s "), *InRouteInfo.RouteName.ToString(), *RegisteredRoutes[InRouteInfo.RouteName].Handle->Path, *InRouteInfo.RoutePath.GetPath());
			HttpRouter->UnbindRoute(RegisteredRoutes[InRouteInfo.RouteName].Handle);
		}
		FAvaPlaybackHttpRouteDesc RouteDesc;
		RouteDesc.Handle = HttpRouter->BindRoute(InRouteInfo.RoutePath, InRouteInfo.RequestVerbs, Handler);
		RouteDesc.InputContentType = InRouteInfo.InputContentType;
		RouteDesc.InputExpectedFormat = InRouteInfo.InputExpectedFormat;
		RegisteredRoutes.Add(InRouteInfo.RouteName, RouteDesc);
	}
}

void FAvaPlaybackHttpServer::CleanUpRoute(FName RouteName, bool bFailIfUnbound /* = false */)
{
	if (HttpRouter.IsValid())
	{
		if (RegisteredRoutes.Find(RouteName))
		{
			HttpRouter->UnbindRoute(RegisteredRoutes[RouteName].Handle);
			RegisteredRoutes.Remove(RouteName);
			UE_LOG(LogAvaPlaybackHttpServer, Log, TEXT("Route name %s was unbound!"), *RouteName.ToString());
		}
		else
		{
			UE_LOG(LogAvaPlaybackHttpServer, Warning, TEXT("Route name %s does not exist, could not unbind."), *RouteName.ToString());
			check(!bFailIfUnbound);
		}
	}
}

bool FAvaPlaybackHttpServer::HttpListOpenRoutes(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString ResponseStr;
	TArray<FName> OutRouteKeys;
	RegisteredRoutes.GetKeys(OutRouteKeys);
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ResponseStr);
	JsonWriter->WriteArrayStart();
	for (const FName& RouteKey : OutRouteKeys)
	{
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("name"), RouteKey.ToString());
		JsonWriter->WriteValue(TEXT("route"), RegisteredRoutes[RouteKey].Handle->Path);
		JsonWriter->WriteValue(TEXT("verb"), GetHttpRouteVerbString(RegisteredRoutes[RouteKey].Handle->Verbs));
		if (!RegisteredRoutes[RouteKey].InputContentType.IsEmpty())
		{
			JsonWriter->WriteValue(TEXT("inputContentType"), RegisteredRoutes[RouteKey].InputContentType);
		}
		if (!RegisteredRoutes[RouteKey].InputExpectedFormat.IsEmpty())
		{
			JsonWriter->WriteValue(TEXT("inputExpectedFormat"), RegisteredRoutes[RouteKey].InputExpectedFormat);
		}
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteArrayEnd();
	JsonWriter->Close();
	TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(ResponseStr, TEXT("application/json"));
	OnComplete(MoveTemp(Response));
	return true;
}