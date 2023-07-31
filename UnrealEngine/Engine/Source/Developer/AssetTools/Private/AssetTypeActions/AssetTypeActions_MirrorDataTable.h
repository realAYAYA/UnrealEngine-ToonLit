// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_DataTable.h"
#include "Animation/MirrorDataTable.h"

class FAssetTypeActions_MirrorDataTable : public FAssetTypeActions_DataTable
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_MirrorDataTable", "Mirror Data Table"); }
	virtual UClass* GetSupportedClass() const override { return UMirrorDataTable::StaticClass(); }
	virtual bool IsImportedAsset() const override { return false; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
	// End IAssetTypeActions
};
