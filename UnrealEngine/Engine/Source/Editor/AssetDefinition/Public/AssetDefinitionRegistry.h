// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "AssetDefinitionRegistry.generated.h"

class UAssetDefinition;

UCLASS(config=Editor)
class ASSETDEFINITION_API UAssetDefinitionRegistry : public UObject
{
	GENERATED_BODY()

public:
	static UAssetDefinitionRegistry* Get();
	
	virtual void BeginDestroy() override;

	const UAssetDefinition* GetAssetDefinitionForAsset(const FAssetData& Asset) const;
	const UAssetDefinition* GetAssetDefinitionForClass(const UClass* Class) const;

	TArray<TObjectPtr<UAssetDefinition>> GetAllAssetDefinitions() const;

	/**
	 * Normally UAssetDefinitionRegistry are registered automatically by their CDO.  The only reason you need to do this is if
	 * you're forced to dynamically create the UAssetDefinition at runtime.  The original reason for this function was
	 * to be able to create wrappers for the to be replaced IAssetTypeActions, that you can access AssetDefinition
	 * versions of any IAssetType making the upgrade easier.
	 */
	void RegisterAssetDefinition(UAssetDefinition* AssetDefinition);

	void UnregisterAssetDefinition(UAssetDefinition* AssetDefinition);
	
private:
	static UAssetDefinitionRegistry* Singleton;
	static bool bHasShutDown;

	UPROPERTY()
	TMap<TSoftClassPtr<UObject>, TObjectPtr<UAssetDefinition>> AssetDefinitions;
};