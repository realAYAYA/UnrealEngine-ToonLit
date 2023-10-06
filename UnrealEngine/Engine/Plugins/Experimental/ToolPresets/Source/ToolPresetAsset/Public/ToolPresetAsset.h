// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

//#include "Internationalization/Text.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "AssetToolsModule.h"
#include "EditorConfigBase.h"

#include "ToolPresetAsset.generated.h"

#define LOCTEXT_NAMESPACE "ToolPresetAsset"

USTRUCT(BlueprintType)
struct TOOLPRESETASSET_API FInteractiveToolPresetDefinition
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig))
	FString StoredProperties;

	UPROPERTY(EditAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig))
	FString Label;

	UPROPERTY(EditAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig))
	FString Tooltip;

	bool IsValid() const;
	void SetStoredPropertyData(TArray<UObject*>& Properties);
	void LoadStoredPropertyData(TArray<UObject*>& Properties);
};

USTRUCT(BlueprintType)
struct TOOLPRESETASSET_API FInteractiveToolPresetStore
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig))
	TArray<FInteractiveToolPresetDefinition> NamedPresets;

	UPROPERTY(VisibleAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig))
	FText ToolLabel;

	UPROPERTY(VisibleAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig))
	FSlateBrush ToolIcon;
};

/**
 * Implements an asset that can be used to store tool settings as a named preset
 */
UCLASS(BlueprintType, hidecategories=(Object), EditorConfig = "UInteractiveToolsPresetCollectionAsset_DefaultCollection")
class TOOLPRESETASSET_API UInteractiveToolsPresetCollectionAsset 
	: public UEditorConfigBase
{
	GENERATED_BODY()

public:

	// TODO: Currently there are no helper methods within this class, simply providing
	// raw access to the underlying arrays and maps. This is intentional.
	// Until the design of the preset concept is more firmly decided, it seems like a waste to
	// implement a bunch of methods that we don't know if we actually want/need at the end.
	// Once we are satisifed with the data structure, planned accessors and mutators will include
	// support for adding, removing, renaming, saving and retrieving presets.

	UPROPERTY(VisibleAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig) )
	TMap<FString, FInteractiveToolPresetStore > PerToolPresets;

	UPROPERTY(EditAnywhere, Category = "ToolPresetAsset", meta = (EditorConfig))
	FText CollectionLabel;
};


UCLASS(hidecategories = Object)
class TOOLPRESETASSET_API UInteractiveToolsPresetCollectionAssetFactory : public UFactory
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
			Preset->CollectionLabel = FText::FromName(Name);
		}
		return Preset;
	}

	uint32 GetMenuCategories() const override
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		return AssetTools.RegisterAdvancedAssetCategory("ToolPresets", LOCTEXT("AssetCategoryName", "Tool Presets"));
	}

	FString GetDefaultNewAssetName() const override
	{
		return TEXT("Tool Preset");
	}

	bool ShouldShowInNewMenu() const override { return true; }
};

#undef LOCTEXT_NAMESPACE