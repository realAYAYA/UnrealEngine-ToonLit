// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "AssetTypeCategories.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Sound/SoundEffectPreset.h"
#include "Sound/SoundEffectSource.h"
#include "Sound/SoundEffectSubmix.h"
#include "Templates/SharedPointer.h"
#include "UObject/StrongObjectPtr.h"

// Forward Declarations
class IToolkitHost;
class UClass;
class UObject;


class FAssetTypeActions_SoundEffectSubmixPreset : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSubmixPreset", "Submix Effect Preset"); }
	virtual FColor GetTypeColor() const override { return FColor(99, 63, 56); }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override { return USoundEffectSubmixPreset::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundEffectSourcePreset : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSourcePreset", "Source Effect Preset"); }
	virtual FColor GetTypeColor() const override { return FColor(72, 185, 187); }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override { return USoundEffectSourcePreset::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundEffectSourcePresetChain : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundSourcePresetChain", "Source Effect Preset Chain"); }
	virtual FColor GetTypeColor() const override { return FColor(51, 107, 142); }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override { return USoundEffectSourcePresetChain::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};

class FAssetTypeActions_SoundEffectPreset : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_SoundEffectPreset(USoundEffectPreset* InEffectPreset);

	//~ Begin FAssetTypeActions_Base
	virtual bool CanFilter() override { return EffectPreset->CanFilter(); }
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override { return EffectPreset->GetPresetColor(); }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> ToolkitHost) override;
	//~ End FAssetTypeActions_Base

private:
	TStrongObjectPtr<USoundEffectPreset> EffectPreset;
};
