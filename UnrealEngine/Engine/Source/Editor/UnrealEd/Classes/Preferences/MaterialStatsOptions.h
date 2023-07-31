// Copyright Epic Games, Inc. All Rights Reserved.

/**
*
* A configuration class used by the FMaterialStats to save settings across sessions.
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#include "MaterialStatsOptions.generated.h"

UCLASS(hidecategories = Object, config = EditorPerProjectUserSettings)
class UNREALED_API UMaterialStatsOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, config, Category=Options)
	int32 bPlatformUsed[SP_NumPlatforms];

	UPROPERTY(EditAnywhere, config, Category = Options)
	int32 bMaterialQualityUsed[EMaterialQualityLevel::Num];
};