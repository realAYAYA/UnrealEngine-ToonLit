// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MultiUserReplicationClientPreset.h"
#include "UObject/Object.h"
#include "MultiUserReplicationSessionPreset.generated.h"

/**
 * 
 */
UCLASS()
class MULTIUSERREPLICATIONEDITOR_API UMultiUserReplicationSessionPreset : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	UMultiUserReplicationClientPreset* AddClient();
	
	void RemoveClient(UMultiUserReplicationClientPreset& Client);
	void ClearClients();

	const TArray<TObjectPtr<UMultiUserReplicationClientPreset>>& GetClientPresets() const { return ClientPresets; }
	UMultiUserReplicationClientPreset* GetUnassignedClient() const { return UnassignedClient; }

private:
	
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UMultiUserReplicationClientPreset>> ClientPresets;

	/** Special preset that is assigned to an invalid client - objects without ownership go here. This is not in ClientPresets. */
	UPROPERTY(Instanced)
	TObjectPtr<UMultiUserReplicationClientPreset> UnassignedClient;
};
