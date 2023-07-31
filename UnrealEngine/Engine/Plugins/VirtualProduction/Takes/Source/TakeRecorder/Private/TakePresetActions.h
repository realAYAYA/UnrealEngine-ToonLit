// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "AssetTypeActions_Base.h"

class IToolkitHost;

class FTakePresetActions
	: public FAssetTypeActions_Base
{
public:
	virtual uint32 GetCategories() override;
	virtual FText GetName() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual FColor GetTypeColor() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;
	virtual bool ShouldForceWorldCentric() override;
	virtual bool CanLocalize() const override { return false; }
};
