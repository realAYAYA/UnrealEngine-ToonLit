// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WEBSOCKETS

#include "IWebSocketsManager.h"
#include "HoloLensWebSocket.h"

class FHoloLensWebSocketsManager
	: public IWebSocketsManager
{
private:
	// IWebSocketsManager
	virtual void InitWebSockets(TArrayView<const FString> Protocols) override {}
	virtual void ShutdownWebSockets() override {}
	virtual TSharedRef<IWebSocket> CreateWebSocket(const FString& Url, const TArray<FString>& Protocols, const TMap<FString, FString>& UpgradeHeaders) override;
	// End IWebSocketsManager
};

#endif
