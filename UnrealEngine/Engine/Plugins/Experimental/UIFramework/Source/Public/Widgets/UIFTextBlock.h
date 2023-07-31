// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIFWidget.h"

#include "UIFTextBlock.generated.h"

class UTextBlock;

/**
 *
 */
UCLASS(DisplayName = "TextBlock UIFramework")
class UIFRAMEWORK_API UUIFrameworkTextBlock : public UUIFrameworkWidget
{
	GENERATED_BODY()

public:
	UUIFrameworkTextBlock();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetText(FText Text);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FText GetText() const
	{
		return Text;
	}

	virtual void LocalOnUMGWidgetCreated() override;

private:
	UFUNCTION()
	void OnRep_Text();

private:
	UPROPERTY(/*ExposeOnSpawn, */ReplicatedUsing=OnRep_Text)
	FText Text;
};
