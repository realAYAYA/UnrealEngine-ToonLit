// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "GameFramework/ForceFeedbackAttenuation.h"

class FAssetTypeActions_ForceFeedbackAttenuation : public FAssetTypeActions_Base
{
public:

	FAssetTypeActions_ForceFeedbackAttenuation(EAssetTypeCategories::Type InAssetCategoryBit)
		: AssetCategoryBit(InAssetCategoryBit)
	{ }

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ForceFeedbackAttenuation", "Force Feedback Attenuation"); }
	virtual FColor GetTypeColor() const override { return FColor(175, 0, 0); }
	virtual UClass* GetSupportedClass() const override { return UForceFeedbackAttenuation::StaticClass(); }
	virtual uint32 GetCategories() override { return AssetCategoryBit; }

private:
	EAssetTypeCategories::Type AssetCategoryBit;
};