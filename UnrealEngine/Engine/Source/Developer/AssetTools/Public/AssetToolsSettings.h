// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AdvancedCopyCustomization.h"
#include "Engine/AssetUserData.h"
#include "Engine/EngineTypes.h"
#include "AssetToolsSettings.generated.h"

USTRUCT()
struct FAdvancedCopyMap
{
	GENERATED_BODY()

public:
	/** When copying this class, use a particular set of dependency and destination rules */
	UPROPERTY(EditAnywhere, Category = "Asset Tools", meta = (MetaClass = "/Script/CoreUObject.Object"))
	FSoftClassPath ClassToCopy;

	/** The set of dependency and destination rules to use for advanced copy */
	UPROPERTY(EditAnywhere, Category = "Asset Tools", meta = (MetaClass = "/Script/AssetTools.AdvancedCopyCustomization"))
	FSoftClassPath AdvancedCopyCustomization;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Asset Tools"))
class ASSETTOOLS_API UAssetToolsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAssetToolsSettings() {};

	/** List of rules to use when advanced copying assets */
	UPROPERTY(config, EditAnywhere, Category = "Advanced Copy", Meta = (TitleProperty = "ClassToCopy"))
	TArray<FAdvancedCopyMap> AdvancedCopyCustomizations;
};
