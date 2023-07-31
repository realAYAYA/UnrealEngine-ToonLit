// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonAnimatedSwitcher.h"

#include "Misc/Optional.h"
#include "UObject/WeakObjectPtrTemplates.h"

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

private:
	void HandleOwningWidgetActivationChanged(const bool bIsActivated);

	void AttemptToActivateActiveWidget();
	void DeactivateActiveWidget();

	void BindOwningActivatableWidget();
	void UnbindOwningActivatableWidget();

	UCommonActivatableWidget* GetOwningActivatableWidget() const;

	TOptional<TWeakObjectPtr<UCommonActivatableWidget>> WeakOwningActivatableWidget;
};
