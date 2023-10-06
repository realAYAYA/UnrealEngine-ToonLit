// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIFPlayerComponent.h"

#include "UIFPresenter.generated.h"

struct FUIFrameworkGameLayerSlot;
class FUIFrameworkModule;
class UWidget;

/**
 * 
 */
 UCLASS(Abstract, Within=UIFrameworkPlayerComponent)
class UIFRAMEWORK_API UUIFrameworkPresenter : public UObject
{
	GENERATED_BODY()

public:
	virtual void AddToViewport(UWidget* UMGWidget, const FUIFrameworkGameLayerSlot& Slot)
	{

	}
	virtual void RemoveFromViewport(FUIFrameworkWidgetId WidgetId)
	{

	}
};


/**
 *
 */
 UCLASS()
class UIFRAMEWORK_API UUIFrameworkGameViewportPresenter : public UUIFrameworkPresenter
 {
	 GENERATED_BODY()

public:
	virtual void AddToViewport(UWidget* UMGWidget, const FUIFrameworkGameLayerSlot& Slot) override;
	virtual void RemoveFromViewport(FUIFrameworkWidgetId WidgetId) override;

	virtual void BeginDestroy() override;

private:
	struct FWidgetPair
	{
		FWidgetPair() = default;
		FWidgetPair(UWidget* Widget, FUIFrameworkWidgetId WidgetId);
		TWeakObjectPtr<UWidget> UMGWidget;
		FUIFrameworkWidgetId WidgetId;
	};
	TArray<FWidgetPair> Widgets;
};
