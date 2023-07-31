// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "AssetTypeCategories.h"
#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"

class UClass;

class FAssetTypeActions_SoundConcurrency : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundConcurrency", "Sound Concurrency"); }
	virtual FColor GetTypeColor() const override { return FColor(77, 100, 139); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	virtual const TArray<FText>& GetSubMenus() const override;
};
