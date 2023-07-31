// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "CommonWidgetGroupBase.generated.h"

//@todo DanH: This is only used for buttons, so ditch the base. Also the vast majority of use cases are native, so it also shouldn't be a UObject

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