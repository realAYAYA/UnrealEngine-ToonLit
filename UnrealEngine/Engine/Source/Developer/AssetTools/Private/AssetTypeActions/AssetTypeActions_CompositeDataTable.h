// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_DataTable.h"
#include "Engine/CompositeDataTable.h"

class FMenuBuilder;

class FAssetTypeActions_CompositeDataTable : public FAssetTypeActions_DataTable
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_CompositeDataTable", "Composite Data Table"); }
	virtual UClass* GetSupportedClass() const override { return UCompositeDataTable::StaticClass(); }
	virtual bool IsImportedAsset() const override { return false; }
	// End IAssetTypeActions
};
