// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidatorBase.h"
#include "AssetValidator_AssetReferenceRestrictions.generated.h"

UCLASS()
class UAssetValidator_AssetReferenceRestrictions : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UAssetValidator_AssetReferenceRestrictions();

protected:
	//~UEditorValidatorBase interface
	virtual bool CanValidateAsset_Implementation(UObject* InAsset) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors) override;
	//~End of UEditorValidatorBase interface
};
