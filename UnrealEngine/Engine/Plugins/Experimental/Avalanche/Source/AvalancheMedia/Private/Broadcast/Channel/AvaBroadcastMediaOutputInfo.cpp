// Copyright Epic Games, Inc. All Rights Reserved.

#include "Broadcast/Channel/AvaBroadcastMediaOutputInfo.h"
#include "Broadcast/OutputDevices/AvaBroadcastDeviceProviderProxy.h"

bool FAvaBroadcastMediaOutputInfo::IsRemote(const FString& InServerName)
{
	return !InServerName.IsEmpty() && InServerName != FAvaBroadcastDeviceProviderProxyManager::LocalServerName;
}

void FAvaBroadcastMediaOutputInfo::PostLoad()
{
	if (!Guid.IsValid())
	{
		Guid = FGuid::NewGuid();
	}
}
