// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR

#include "UObject/SoftObjectPtr.h"

class UWorld;
class UBlueprint;
class ILevelInstanceInterface;

class FPackedLevelActorUtils
{
public:
	static void GetPackedBlueprintsForWorldAsset(const TSoftObjectPtr<UWorld>& InWorldAsset, TSet<TSoftObjectPtr<UBlueprint>>& OutPackedBlueprintAssets, bool bInLoadedOnly = false);

	static bool CanPack();

	static void PackAllLoadedActors();

	static bool CreateOrUpdateBlueprint(ILevelInstanceInterface* InLevelInstance, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave = true, bool bPromptForSave = true);

	static bool CreateOrUpdateBlueprint(TSoftObjectPtr<UWorld> InWorldAsset, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave = true, bool bPromptForSave = true);
	
	static void UpdateBlueprint(UBlueprint* InBlueprint, bool bCheckoutAndSave = true);
};

#endif 