// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_AnimBlueprint.h"

class FAssetTypeActions_AnimBlueprintInterface : public FAssetTypeActions_AnimBlueprint
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimLayerInterface", "Animation Layer Interface"); }
	virtual void BuildBackendFilter(FARFilter& InFilter) override;
	virtual FName GetFilterName() const override;
};
