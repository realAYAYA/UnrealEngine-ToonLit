// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "HttpModule.h"
#include "Async/Future.h"
#include "Containers/UnrealString.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "HordeHttpClient.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUbaHorde, Log, Log);

#define HORDE_NONCE_SIZE 64

// When asking the Horde server for a machine, this will be returned when the HTTP
// reponse comes back (after a machine request). If the response didn't come through
// (i.e., Horde was unable to assign a machine or something), then:
// - Ip will be set to ""
// - Port will be set to 0xFFFF
// - Nonce will be all 0
struct FHordeRemoteMachineInfo
{
	FString Ip;
	uint16 Port;
	uint16 LogicalCores;
	uint8 Nonce[HORDE_NONCE_SIZE];
	bool bRunsWindowOS;
};

// This encapsulates the mechanism of talking to the Horde "meta server". The server which
// can grant us access to remote machines.
class FUbaHordeMetaClient
{
public:
	// We return the HttpResponse in case more information could be used out of it later.
	using HordeMachinePromise = TPromise<TTuple<FHttpResponsePtr, FHordeRemoteMachineInfo>>;
	
	// Construct the client with a single URL that includes the protocol, IP address, and port in the common format "PROTOCOL://IP:PORT".
	FUbaHordeMetaClient(const FStringView& HordeServerUrl, const FStringView& InOAuthProviderIdentifier);

	// Local Horde server uses port 5000 by default.
	FUbaHordeMetaClient(const FStringView& HordeServerIp, uint16 HordeServerPort, bool bInConnectWithAuthentication, const FStringView& OAuthProviderIdentifier);

	bool RefreshHttpClient();

	// This will make a request to Horde for a remote machine to do work on.
	// Example of actually getting the FHordeRemoteMachineInfo struct:
	// ```
	// auto Promise = HordeServer.RequestMachine();
	// auto Future = Promise->GetFuture();
	// ... // Can to asynchronous work
	// auto Future.Wait(); // Wait for the response to arrive
	// FHordeRemoteMachineInfo MachineInfo = Future.Get().Value;
	TSharedPtr<HordeMachinePromise, ESPMode::ThreadSafe> RequestMachine(const FString& PoolId, const FString& Machine = "default");

private:
	void ParseConfig(const FString& IniFilename, const FString& IniSection);

	const FString ServerUrl;
	const bool bConnectWithAuthentication;
	TUniquePtr<FHordeHttpClient> HttpClient;
	FString OAuthProviderIdentifier;
};
