// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "Engine/CompositeCurveTable.h"
#include "AssetTypeActions_CSVAssetBase.h"

class FMenuBuilder;

class FAssetTypeActions_CompositeCurveTable : public FAssetTypeActions_CurveTable
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CompositeCurveTable", "Composite Curve Table"); }
	virtual UClass* GetSupportedClass() const override { return UCompositeCurveTable::StaticClass(); }
	virtual bool IsImportedAsset() const override { return false; }
	// End IAssetTypeActions
};
