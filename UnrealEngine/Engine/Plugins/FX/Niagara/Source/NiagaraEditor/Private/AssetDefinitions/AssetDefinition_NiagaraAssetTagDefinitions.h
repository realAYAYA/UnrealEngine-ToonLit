// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "NiagaraAssetTagDefinitions.h"
#include "AssetDefinition_NiagaraAssetTagDefinitions.generated.h"

UCLASS()
class NIAGARAEDITOR_API UAssetDefinition_NiagaraAssetTagDefinitions : public UAssetDefinitionDefault
{
	GENERATED_BODY()

	UAssetDefinition_NiagaraAssetTagDefinitions();
	
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_NiagaraAssetTagDefinitions", "Niagara Asset Tag Definitions"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor::White; }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UNiagaraAssetTagDefinitions::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};
