// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WEBSOCKETS

#include "HoloLensWebSocketsManager.h"

TSharedRef<IWebSocket> FHoloLensWebSocketsManager::CreateWebSocket(const FString& Url, const TArray<FString>& Protocols, const TMap<FString, FString>& UpgradeHeaders)
{
	return MakeShared<FHoloLensWebSocket>(Url, Protocols, UpgradeHeaders);
}

#endif // #if WITH_WEBSOCKETS