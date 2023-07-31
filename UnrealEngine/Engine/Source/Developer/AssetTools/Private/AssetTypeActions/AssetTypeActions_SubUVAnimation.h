// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Particles/SubUVAnimation.h"

class FAssetTypeActions_SubUVAnimation : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FColor GetTypeColor() const override { return FColor(255,255,255); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SubUVAnimation", "Sub UV Animation"); }
	virtual UClass* GetSupportedClass() const override { return USubUVAnimation::StaticClass(); }
	// End IAssetTypeActions
};
