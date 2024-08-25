// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "CommonWidgetGroupBase.generated.h"

/**
 * Base class for CommonUI widget groups, currently only used for button groups
 */
UCLASS(Abstract, BlueprintType)
class COMMONUI_API UCommonWidgetGroupBase : public UObject
{
	GENERATED_BODY()

public:
	UCommonWidgetGroupBase();

	virtual TSubclassOf<UWidget> GetWidgetType() const { return UWidget::StaticClass(); }

	UFUNCTION(BlueprintCallable, Category = Group)
	void AddWidget(UWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = Group)
	void AddWidgets(const TArray<UWidget*>& Widgets);

	UFUNCTION(BlueprintCallable, Category = Group)
	void RemoveWidget(UWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = Group)
	void RemoveAll();

	template <typename WidgetT>
	void AddWidgets(const TArray<WidgetT>& Widgets)
	{
		for (UWidget* Widget : Widgets)
		{
			AddWidget(Widget);
		}
	}

protected:
	virtual void OnWidgetAdded(UWidget* NewWidget) PURE_VIRTUAL(UCommonWidgetGroupBase::OnWidgetAdded, );
	virtual void OnWidgetRemoved(UWidget* OldWidget) PURE_VIRTUAL(UCommonWidgetGroupBase::OnWidgetRemoved, );
	virtual void OnRemoveAll() PURE_VIRTUAL(UCommonWidgetGroupBase::OnRemoveAll, );
};
