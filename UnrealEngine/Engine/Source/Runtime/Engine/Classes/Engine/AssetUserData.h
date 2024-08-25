// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif //UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AssetUserData.generated.h"

/**
 * Object that can be subclassed to store custom data on Unreal asset objects.
 */
UCLASS(DefaultToInstanced, abstract, editinlinenew, MinimalAPI)
class UAssetUserData
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** used for debugging UAssetUserData data in editor */
	virtual void Draw(class FPrimitiveDrawInterface* PDI, const class FSceneView* View) const {}

	/** Called when the owner object is modified */
	virtual void PostEditChangeOwner() {}
};
