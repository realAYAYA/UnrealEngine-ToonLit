// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesCommonEngineUtils.h"

#if WITH_ENGINE
#include "Engine/GameEngine.h"
#include "Engine/NetDriver.h"
#endif

namespace UE::Online {

#if WITH_ENGINE
UWorld* GetWorldForOnline(FName InstanceName)
{
	UWorld* World = NULL;
#if WITH_EDITOR
	if (InstanceName.ToString() != LexToString(UE::Online::EOnlineServices::Default) && InstanceName != NAME_None)
	{
		FWorldContext& WorldContext = GEngine->GetWorldContextFromHandleChecked(InstanceName);
		check(WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE);
		World = WorldContext.World();
	}
	else
#endif
	{
		UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
		World = GameEngine ? GameEngine->GetGameWorld() : NULL;
	}

	return World;
}
#endif

int32 GetPortFromNetDriver(FName InstanceName)
{
	int32 Port = 0;
#if WITH_ENGINE
	if (GEngine)
	{
		UWorld* World = GetWorldForOnline(InstanceName);
		UNetDriver* NetDriver = World ? GEngine->FindNamedNetDriver(World, NAME_GameNetDriver) : NULL;
		if (NetDriver && NetDriver->GetNetMode() < NM_Client)
		{
			FString AddressStr = NetDriver->LowLevelGetNetworkNumber();
			int32 Colon = AddressStr.Find(":", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Colon != INDEX_NONE)
			{
				FString PortStr = AddressStr.Mid(Colon + 1);
				if (!PortStr.IsEmpty())
				{
					Port = FCString::Atoi(*PortStr);
				}
			}
		}
	}
#endif
	return Port;
}

/* UE::Online */ }