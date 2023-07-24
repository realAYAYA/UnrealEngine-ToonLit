// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/CompositeCurveTable.h"

#include "Table/AssetDefinition_CurveTable.h"
#include "AssetDefinition_CompositeCurveTable.generated.h"

UCLASS()
class UAssetDefinition_CompositeCurveTable : public UAssetDefinition_CurveTable
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CompositeCurveTable", "Composite Curve Table"); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UCompositeCurveTable::StaticClass(); }
	virtual bool CanImport() const override { return false; }
	// UAssetDefinition End
};
