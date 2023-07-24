// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/ObjectLibrary.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_ObjectLibrary.generated.h"

UCLASS()
class UAssetDefinition_ObjectLibrary : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ObjectLibrary", "Object Library"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(255, 63, 108)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UObjectLibrary::StaticClass(); }
	// UAssetDefinition End
};
