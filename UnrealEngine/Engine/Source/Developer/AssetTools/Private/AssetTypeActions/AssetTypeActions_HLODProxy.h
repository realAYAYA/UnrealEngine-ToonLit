// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "Engine/HLODProxy.h"

class FAssetTypeActions_HLODProxy : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_HLODProxy", "HLOD Proxy"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 200, 200); }
	virtual UClass* GetSupportedClass() const override { return UHLODProxy::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool CanLocalize() const override { return false; }
};
