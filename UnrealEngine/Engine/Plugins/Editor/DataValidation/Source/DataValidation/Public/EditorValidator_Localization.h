// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidatorBase.h"
#include "EditorValidator_Localization.generated.h"

/*
* Validates that localized assets (within the L10N folder) conform to a corresponding source asset of the correct type.
* Localized assets that fail this validation will never be loaded as localized variants at runtime.
*/
UCLASS()
class DATAVALIDATION_API UEditorValidator_Localization : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	UEditorValidator_Localization();

protected:
	virtual bool CanValidateAsset_Implementation(UObject* InAsset) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(UObject* InAsset, TArray<FText>& ValidationErrors) override;

	const TArray<FString>* FindOrCacheCulturesForLocalizedRoot(const FString& InLocalizedRootPath);

	TMap<FString, TArray<FString>> CachedCulturesForLocalizedRoots;
};
