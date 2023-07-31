// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_SoundBase.h"

class USoundCueTemplate;
struct FToolMenuContext;

class FAssetTypeActions_SoundCueTemplate : public FAssetTypeActions_SoundBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundCueTemplate", "Sound Cue Template"); }
	virtual FColor GetTypeColor() const override { return FColor::Red; }
	virtual UClass* GetSupportedClass() const override;
	virtual bool CanFilter() override { return true; }
	virtual const TArray<FText>& GetSubMenus() const override;
};

class FAssetActionExtender_SoundCueTemplate
{
public:
	static void RegisterMenus();
private:
	static void ExecuteCreateSoundCueTemplate(const FToolMenuContext& MenuContext);
	/** Converts the provided SoundCue Template to a fully-modifiable SoundCue */
	static void ExecuteCopyToSoundCue(const FToolMenuContext& MenuContext);
};