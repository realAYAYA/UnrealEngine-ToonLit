// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidator.h"

#include "EditorValidator_SourceControl.generated.h"

class FText;
class UObject;

UCLASS()
class UEditorValidator_SourceControl : public UEditorValidator
{
	GENERATED_BODY()

public:
	UEditorValidator_SourceControl();

protected:
	using Super::CanValidateAsset_Implementation; // -Woverloaded-virtual
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
};
