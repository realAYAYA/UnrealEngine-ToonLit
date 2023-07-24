// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "AudioModulationStyle.h"


class FAssetTypeActions_SoundModulationParameter : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundModulationParameter", "Modulation Parameter"); }
	virtual FColor GetTypeColor() const override { return UAudioModulationStyle::GetParameterColor(); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }

	virtual const TArray<FText>& GetSubMenus() const override;
};
