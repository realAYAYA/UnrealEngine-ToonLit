// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Button.h"
#include "Types/UIFSlotBase.h"
#include "Types/UIFEvents.h"
#include "UIFWidget.h"

#include "UIFButton.generated.h"

struct FUIFrameworkWidgetId;

/**
 *
 */
UCLASS(DisplayName = "Button UIFramework")
class UIFRAMEWORK_API UUIFrameworkButton : public UUIFrameworkWidget
{
	GENERATED_BODY()

public:
	UUIFrameworkButton();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetContent(FUIFrameworkSimpleSlot Content);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FUIFrameworkSimpleSlot GetContent() const
	{
		return Slot;
	}

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) override;
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) override;
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId) override;

protected:
	virtual void LocalOnUMGWidgetCreated() override;

private:
	UFUNCTION()
	void HandleClick();

	UFUNCTION(Server, Reliable)
	void ServerClick(APlayerController* PlayerController);

	UFUNCTION()
	void OnRep_Slot();

public:
	FUIFrameworkClickEvent OnClick;

private:
	UPROPERTY(/*ExposeOnSpawn, */ReplicatedUsing = OnRep_Slot)
	FUIFrameworkSimpleSlot Slot;
};

UCLASS()
class UIFRAMEWORK_API UUIFrameworkButtonWidget : public UButton
{
	GENERATED_BODY()

public:
	UUIFrameworkButtonWidget();
};
