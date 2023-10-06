// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "UObject/Interface.h"
#include "TargetInterfaces/AssetBackedTarget.h"

#include "ClothAssetBackedTarget.generated.h"

UINTERFACE()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothAssetBackedTarget : public UAssetBackedTarget
{
	GENERATED_BODY()
};

class CHAOSCLOTHASSETEDITORTOOLS_API IClothAssetBackedTarget : public IAssetBackedTarget
{
	GENERATED_BODY()

public:

	// IAssetBackedTarget
	/**
	 * @return the underlying source asset for this Target.
	 */
	virtual UObject* GetSourceData() const override
	{
		return GetClothAsset();
	}

	/**
	 * @return the underlying source ClothAsset asset for this Target
	 */
	virtual UChaosClothAsset* GetClothAsset() const = 0;
};
