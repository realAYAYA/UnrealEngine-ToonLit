// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Notifications/INotificationWidget.h"

class FNiagaraParameterNotificationWidgetProvider : public INotificationWidget
{
public:
	FNiagaraParameterNotificationWidgetProvider(TSharedRef<SWidget> WidgetToDisplay) : NotificationWidget(WidgetToDisplay) {}

	virtual ~FNiagaraParameterNotificationWidgetProvider() {}

	virtual void OnSetCompletionState(SNotificationItem::ECompletionState State) override {}
	virtual TSharedRef<SWidget> AsWidget() override { return NotificationWidget; }

private:
	TSharedRef<SWidget> NotificationWidget;
};