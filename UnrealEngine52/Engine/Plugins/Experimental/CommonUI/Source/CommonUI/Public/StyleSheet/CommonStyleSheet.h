// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "CommonStyleSheet.generated.h"

class UCommonStyleSheetTypeBase;
class UCommonTextBlock;
class UWidget;

//////////////////////////////
// PROTOTYPE: DO NOT USE!!!
//////////////////////////////

UCLASS(BlueprintType)
class UCommonStyleSheet : public UDataAsset
{
	GENERATED_BODY()

public:
	void Apply(UWidget* Widget);

private:
	void TryApplyColorAndOpacity(UCommonTextBlock* TextBlock) const;
	void TryApplyLineHeightPercentage(UCommonTextBlock* TextBlock) const;
	void TryApplyFont(UCommonTextBlock* TextBlock) const;
	void TryApplyMargin(UCommonTextBlock* TextBlock) const;

	template <typename T>
	const T* FindStyleSheetProperty(const UCommonStyleSheet* StyleSheet) const;

private:
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Properties")
	TArray<TObjectPtr<UCommonStyleSheetTypeBase>> Properties;

	UPROPERTY(EditDefaultsOnly, Category = "Import")
	TArray<TObjectPtr<UCommonStyleSheet>> ImportedStyleSheets;

	bool bIsApplying = false;
};
