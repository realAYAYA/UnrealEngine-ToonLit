// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "InterchangeImportTestPlan.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

class FAssetTypeActions_InterchangeImportTestPlan : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return LOCTEXT("AssetTypeActions_InterchangeImportTestPlan", "Interchange Import Test Plan"); }
	virtual FColor GetTypeColor() const override { return FColor(125, 0, 200); }
	virtual UClass* GetSupportedClass() const override { return UInterchangeImportTestPlan::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
};

#undef LOCTEXT_NAMESPACE
