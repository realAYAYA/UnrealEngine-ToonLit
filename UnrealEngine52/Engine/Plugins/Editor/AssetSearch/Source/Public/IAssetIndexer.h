// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"

class UObject;
class FSearchSerializer;

class ASSETSEARCH_API IAssetIndexer
{
public:

	virtual ~IAssetIndexer() = 0;
	virtual FString GetName() const = 0;
	virtual int32 GetVersion() const = 0;
	virtual void IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const = 0;

	/**
	 * If your package contains a nested asset, such as the Blueprint stored in Level/World packages, 
	 * it would return UBlueprint's class in the array.  This is only important if you use IndexNestedAsset.
	 */
	virtual void GetNestedAssetTypes(TArray<UClass*>& OutTypes) const { }
};

inline IAssetIndexer::~IAssetIndexer() = default;

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
