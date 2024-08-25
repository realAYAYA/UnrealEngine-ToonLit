// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonAnimatedSwitcher.h"


#include "CommonActivatableWidgetSwitcher.generated.h"

class UCommonActivatableWidget;

/**
 * An animated switcher that knows about CommonActivatableWidgets. It can also hold other Widgets.
 */
UCLASS()
class COMMONUI_API UCommonActivatableWidgetSwitcher : public UCommonAnimatedSwitcher
{
	GENERATED_BODY()

public:
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual void OnWidgetRebuilt() override;

	virtual void HandleOutgoingWidget() override;
	virtual void HandleSlateActiveIndexChanged(int32 ActiveIndex) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switcher")
	bool bClearFocusRestorationTargetOfDeactivatedWidgets = false;

private:
	void HandleOwningWidgetActivationChanged(const bool bIsActivated);

	void AttemptToActivateActiveWidget();
	void DeactivateActiveWidget();

	void BindOwningActivatableWidget();
	void UnbindOwningActivatableWidget();

	UCommonActivatableWidget* GetOwningActivatableWidget() const;

	TOptional<TWeakObjectPtr<UCommonActivatableWidget>> WeakOwningActivatableWidget;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Misc/Optional.h"
#include "UObject/WeakObjectPtrTemplates.h"
#endif
