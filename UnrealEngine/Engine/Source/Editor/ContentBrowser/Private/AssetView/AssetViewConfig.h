// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "EditorConfigBase.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "AssetViewConfig.generated.h"

USTRUCT()
struct FAssetViewInstanceConfig
{
	GENERATED_BODY()

	/** 
	 * The current thumbnail size, as cast from EThumbnailSize, because that enum is not a UENUM.
	 */
	UPROPERTY()
	uint8 ThumbnailSize = 0; 

	/** 
	 * The current thumbnail size, as cast from EAssetViewType, because that enum is not a UENUM.
	 */
	UPROPERTY()
	uint8 ViewType = 0; 

	UPROPERTY()
	TArray<FName> HiddenColumns;
};

UCLASS(EditorConfig="AssetView")
class CONTENTBROWSER_API UAssetViewConfig : public UEditorConfigBase
{
	GENERATED_BODY()

public:
	static void Initialize();
	static UAssetViewConfig* Get() { return Instance; }

	FAssetViewInstanceConfig& GetInstanceConfig(FName ViewName);
	
	UPROPERTY(meta=(EditorConfig))
	TMap<FName, FAssetViewInstanceConfig> Instances;
	
private:
	static TObjectPtr<UAssetViewConfig> Instance;
};
