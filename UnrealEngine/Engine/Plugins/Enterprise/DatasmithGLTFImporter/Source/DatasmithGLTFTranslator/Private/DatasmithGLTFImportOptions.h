// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithImportOptions.h"

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "DatasmithGLTFImportOptions.generated.h"

UCLASS(config = EditorPerProjectUserSettings, HideCategories = (DebugProperty))
class UDatasmithGLTFImportOptions : public UDatasmithOptionsBase
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Lightmaps, meta = (
		ToolTip = "Generate new UV coordinates for lightmapping instead of using the highest index UV set. \nTurn this on to have Unreal Studio generate lightmap UV sets automatically.\nTurn this off to try using the highest index existing UV set (if available) as the lightmap UV set.\nFor both cases, geometry without existing UV sets will receive an empty UV set, which will by itself not be valid for use with Lightmass."))
	bool bGenerateLightmapUVs = false;

	UPROPERTY( config, EditAnywhere, BlueprintReadWrite, Category = AssetImporting, meta = (
		DisplayName = "Import Uniform Scale",
		ToolTip = "Scale factor used for importing assets, by default: 100, for conversion from meters(glTF) to centimeters(Unreal default)."))
	float ImportScale = 100.f;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = AssetImporting, meta = (
		DisplayName = "Animation FPS from File",
		ToolTip = "Use animation frame rate from source (as it was exported). If unchecked, animations are resampled with 30 FPS."))
	bool bAnimationFrameRateFromFile = false;
};
