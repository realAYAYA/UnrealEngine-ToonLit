// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "Internationalization/Text.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "AssetToolsModule.h"

#include "PresetAsset.generated.h"

#define LOCTEXT_NAMESPACE "PresetAsset"

USTRUCT(BlueprintType)
struct FInteractiveToolPresetDefintion
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = "PresetAsset")
	TArray< TObjectPtr<UObject> > Properties;

	UPROPERTY(EditAnywhere, Category = "PresetAsset")
	FString Label;

	UPROPERTY(EditAnywhere, Category = "PresetAsset")
	FString Tooltip;

	// TODO: Investigate how to store things like icons. This doesn't work as written, as FSlateBrush can't be serialized?
	//UPROPERTY(EditAnywhere, Category = "PresetAsset")
	//FSlateBrush Icon;
};

USTRUCT(BlueprintType)
struct FInteractiveToolPresetStore
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "PresetAsset")
	TMap< FString, FInteractiveToolPresetDefintion> NamedPresets;
};

/**
 * Implements an asset that can be used to store tool settings as a named preset
 */
UCLASS(BlueprintType, hidecategories=(Object))
class PRESETASSET_API UInteractiveToolsPresetCollectionAsset
	: public UObject
{
	GENERATED_BODY()

public:

	// TODO: Currently there are no helper methods within this class, simply providing
	// raw access to the underlying arrays and maps. This is intentional.
	// Until the design of the preset concept is more firmly decided, it seems like a waste to
	// implement a bunch of methods that we don't know if we actually want/need at the end.
	// Once we are satisifed with the data structure, planned accessors and mutators will include
	// support for adding, removing, renaming, saving and retrieving presets.

	UPROPERTY(VisibleAnywhere, Category = "PresetAsset")
	TMap<FString, FInteractiveToolPresetStore > PerToolPresets;

};


UCLASS(hidecategories = Object)
class PRESETASSET_API UInteractiveToolsPresetCollectionAssetFactory : public UFactory
{
	GENERATED_BODY()

public:

	UInteractiveToolsPresetCollectionAssetFactory()
	{
		SupportedClass = UInteractiveToolsPresetCollectionAsset::StaticClass();

		bCreateNew = true;
		bEditAfterNew = true;
	}

	FText GetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Tool Preset");
	}

	FText GetToolTip() const override
	{
		return LOCTEXT("Tooltip", "Tool Presets capture the state of tool settings for later reloading.");
	}

	UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override
	{
		UInteractiveToolsPresetCollectionAsset* Preset = nullptr;
		if (ensure(SupportedClass == Class))
		{
			ensure(0 != (RF_Public & Flags));
			Preset = NewObject<UInteractiveToolsPresetCollectionAsset>(InParent, Class, Name, Flags);
		}
		return Preset;
	}

	uint32 GetMenuCategories() const override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		return AssetTools.RegisterAdvancedAssetCategory("Presets", LOCTEXT("AssetCategoryName", "Presets"));
	}

	FString GetDefaultNewAssetName() const override
	{
		return TEXT("Tool Preset");
	}

	bool ShouldShowInNewMenu() const override { return true; }
};

#undef LOCTEXT_NAMESPACE