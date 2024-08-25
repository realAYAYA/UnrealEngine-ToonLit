// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorValidator.h"

#include "EditorValidator_Load.generated.h"

class FText;
class UObject;

UCLASS()
class UEditorValidator_Load : public UEditorValidator
{
	GENERATED_BODY()

public:
	UEditorValidator_Load();

	virtual bool IsEnabled() const override;

	static bool GetLoadWarningsAndErrorsForPackage(const FString& PackageName, TArray<FString>& OutWarningsAndErrors);

protected:
	using Super::CanValidateAsset_Implementation; // -Woverloaded-virtual
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;
	
private:
	static TArray<FString> InMemoryReloadLogIgnoreList;
};
