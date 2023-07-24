// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class UE_DEPRECATED(5.1, "OculusVR plugin is deprecated; please use the built-in OpenXR plugin or OculusXR plugin from the Marketplace.") FOculusAssetDirectory;

class FOculusAssetDirectory
{
public: 
#if WITH_EDITORONLY_DATA
	OCULUSHMD_API static void LoadForCook();
	OCULUSHMD_API static void ReleaseAll();
#endif

	static FSoftObjectPath AssetListing[];
};
