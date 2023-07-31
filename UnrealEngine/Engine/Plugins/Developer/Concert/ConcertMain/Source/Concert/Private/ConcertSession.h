// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertSession.h"
#include "ConcertMessages.h"

/**
 * Common implementation for Concert Client and Server sessions (@see IConcertSession for the API description).
 * @note	This doesn't inherit from any session interface, but does implement some of their API with a "Common" prefix on the function names.
 *			Interface implementations can also inherit from this common impl and then call the "Common" functions from the interface overrides.
 */
class FConcertSessionCommonImpl
{
public:
	explicit FConcertSessionCommonImpl(const FConcertSessionInfo& InSessionInfo);

	void CommonStartup();
	void CommonShutdown();

	const FGuid& CommonGetId() const
	{
		return SessionInfo.SessionId;
	}

	const FString& CommonGetName() const
	{
		return SessionInfo.SessionName;
	}

	void CommonSetName(const FString& NewName)
	{
		SessionInfo.SessionName = NewName;
	}

	const FConcertSessionInfo& CommonGetSessionInfo() const
	{
		return SessionInfo;
	}

	TArray<FGuid> CommonGetSessionClientEndpointIds() const;
	TArray<FConcertSessionClientInfo> CommonGetSessionClients() const;
	bool CommonFindSessionClient(const FGuid& EndpointId, FConcertSessionClientInfo& OutSessionClientInfo) const;

	FConcertScratchpadRef CommonGetScratchpad() const;
	FConcertScratchpadPtr CommonGetClientScratchpad(const FGuid& ClientEndpointId) const;

	FDelegateHandle CommonRegisterCustomEventHandler(const FName& EventMessageType, const TSharedRef<IConcertSessionCustomEventHandler>& Handler);
	void CommonUnregisterCustomEventHandler(const FName& EventMessageType, const FDelegateHandle EventHandle);
	void CommonUnregisterCustomEventHandler(const FName& EventMessageType, const void* EventHandler);
	void CommonClearCustomEventHandler(const FName& EventMessageType);
	void CommonHandleCustomEvent(const FConcertMessageContext& Context);

	void CommonRegisterCustomRequestHandler(const FName& RequestMessageType, const TSharedRef<IConcertSessionCustomRequestHandler>& Handler);
	void CommonUnregisterCustomRequestHandler(const FName& RequestMessageType);
	TFuture<FConcertSession_CustomResponse> CommonHandleCustomRequest(const FConcertMessageContext& Context);

	static bool CommonBuildCustomEvent(const UScriptStruct* EventType, const void* EventData, const FGuid& SourceEndpointId, const TArray<FGuid>& DestinationEndpointIds, FConcertSession_CustomEvent& OutCustomEvent);
	static bool CommonBuildCustomRequest(const UScriptStruct* RequestType, const void* RequestData, const FGuid& SourceEndpointId, const FGuid& DestinationEndpointId, FConcertSession_CustomRequest& OutCustomRequest);
	static void CommonHandleCustomResponse(const FConcertSession_CustomResponse& Response, const TSharedRef<IConcertSessionCustomResponseHandler>& Handler);

protected:
	/** Information about this session */
	FConcertSessionInfo SessionInfo;

	/** The scratchpad for this session */
	FConcertScratchpadPtr Scratchpad;

	/** Map of clients connected to this session (excluding us if we're a client session) */
	struct FSessionClient
	{
		FConcertSessionClientInfo ClientInfo;
		FConcertScratchpadPtr Scratchpad;
	};
	TMap<FGuid, FSessionClient> SessionClients;

	/** Map of custom event handlers for this session */
	TMap<FName, TArray<TSharedPtr<IConcertSessionCustomEventHandler>>> CustomEventHandlers;

	/** Map of custom request handlers for this session */
	TMap<FName, TSharedPtr<IConcertSessionCustomRequestHandler>> CustomRequestHandlers;
};
