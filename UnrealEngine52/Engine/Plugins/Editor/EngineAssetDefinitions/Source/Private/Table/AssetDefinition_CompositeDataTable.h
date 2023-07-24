// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/CompositeDataTable.h"

#include "Table/AssetDefinition_DataTable.h"
#include "AssetDefinition_CompositeDataTable.generated.h"

UCLASS()
class UAssetDefinition_CompositeDataTable : public UAssetDefinition_DataTable
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_CompositeDataTable", "Composite Data Table"); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UCompositeDataTable::StaticClass(); }
	virtual bool CanImport() const override { return false; }
	// UAssetDefinition End
};
