// Copyright Epic Games, Inc. All Rights Reserved.

#include "Assets/MultiUserReplicationSessionPreset.h"

UMultiUserReplicationSessionPreset::UMultiUserReplicationSessionPreset(const FObjectInitializer& ObjectInitializer)
{
	UnassignedClient = ObjectInitializer.CreateDefaultSubobject<UMultiUserReplicationClientPreset>(this, TEXT("UnassignedClient"));
}

UMultiUserReplicationClientPreset* UMultiUserReplicationSessionPreset::AddClient()
{
	UMultiUserReplicationClientPreset* Result = NewObject<UMultiUserReplicationClientPreset>(this);
	ClientPresets.Add(Result);
	return Result;
}

void UMultiUserReplicationSessionPreset::RemoveClient(UMultiUserReplicationClientPreset& Client)
{
	ClientPresets.RemoveSingle(&Client);
}

void UMultiUserReplicationSessionPreset::ClearClients()
{
	ClientPresets.Empty();
	UnassignedClient->ClearClient();
}
