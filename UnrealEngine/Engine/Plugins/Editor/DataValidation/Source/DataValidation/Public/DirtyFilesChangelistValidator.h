// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidatorBase.h"
#include "DirtyFilesChangelistValidator.generated.h"

class UPackage;

/**
* Validates there is no unsaved files in the changelist about to be submitted.
*/
UCLASS()
class DATAVALIDATION_API UDirtyFilesChangelistValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};
