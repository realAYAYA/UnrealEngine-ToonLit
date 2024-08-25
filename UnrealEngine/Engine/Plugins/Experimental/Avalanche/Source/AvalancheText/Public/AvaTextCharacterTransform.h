// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Text3DCharacterTransform.h"

#include "AvaTextCharacterTransform.generated.h"

/**
 * This Component is kept for compatibility with old AAvaTextActor assets
 */
UCLASS(MinimalAPI, ClassGroup=(Text3D), HideCategories = (Collision, Tags, Activation, Cooking, Rendering, Physics, Mobility, LOD, AssetUserData, Navigation, Transform), meta = (BlueprintSpawnableComponent))
class UAvaTextCharacterTransform : public UText3DCharacterTransform
{
	GENERATED_BODY()
};
