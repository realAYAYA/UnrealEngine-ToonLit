// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Types/UIFEvents.h"
#include "UIFWidget.h"

#include "UIFSimpleButton.generated.h"

/**
 *
 */
UCLASS(DisplayName = "Simple Button UIFramework")
class UIFRAMEWORK_API UUIFrameworkSimpleButton : public UUIFrameworkWidget
{
	GENERATED_BODY()

public:
	UUIFrameworkSimpleButton();

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

	UFUNCTION(Server, Reliable)
	void ServerClick();

private:
	UPROPERTY(ReplicatedUsing=OnRep_Text)
	FText Text;

public:
	FUIFrameworkClickEvent OnClick;
};
