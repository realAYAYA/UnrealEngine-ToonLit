// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FPCGCommonAssetTypeActions : public FAssetTypeActions_Base
{
public:
	// ~IAssetTypeActions Implementation
	virtual uint32 GetCategories() override;
	virtual FColor GetTypeColor() const override;
};