// Copyright Epic Games, Inc. All Rights Reserved.

/**
*
* A configuration class used by the FMaterialStats to save settings across sessions.
*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#endif
#include "RHIShaderPlatform.h"
#include "SceneTypes.h"
#include "MaterialStatsOptions.generated.h"

UENUM()
enum class EMaterialStatsDerivedMIOption : uint8
{
	Ignore = 0,
	CompileOnly,
	ShowStats,
	InvalidOrMax,
};

UCLASS(hidecategories = Object, config = EditorPerProjectUserSettings, MinimalAPI)
class UMaterialStatsOptions : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, config, Category=Options)
	int32 bPlatformUsed[SP_NumPlatforms];

	UPROPERTY(EditAnywhere, config, Category = Options)
	int32 bMaterialQualityUsed[EMaterialQualityLevel::Num];

	UPROPERTY(EditAnywhere, config, Category=Options)
	EMaterialStatsDerivedMIOption MaterialStatsDerivedMIOption;
};
