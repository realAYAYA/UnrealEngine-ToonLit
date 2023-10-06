// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "AudioModulationStyle.h"


class FAssetTypeActions_SoundControlBus : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundControlBus", "Control Bus"); }
	virtual FColor GetTypeColor() const override { return UAudioModulationStyle::GetControlBusColor(); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }

	virtual const TArray<FText>& GetSubMenus() const override;
};
