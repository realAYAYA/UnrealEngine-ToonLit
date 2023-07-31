// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Types/UIFSlotBase.h"
#include "Types/UIFEvents.h"
#include "UIFWidget.h"

#include "UIFButton.generated.h"

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
	UFUNCTION(Server, Reliable)
	void ServerClick();

	UFUNCTION()
	void OnRep_Slot();

public:
	FUIFrameworkClickEvent OnClick;

private:
	UPROPERTY(/*ExposeOnSpawn, */ReplicatedUsing = OnRep_Slot)
	FUIFrameworkSimpleSlot Slot;
};
