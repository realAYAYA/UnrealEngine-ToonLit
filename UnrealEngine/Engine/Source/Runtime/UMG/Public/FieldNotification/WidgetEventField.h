// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WidgetEventField.generated.h"

/**
 * Generic structure to notify when an event occurs.
 * 
 * class UMyWidget : public UWidget
 * {
 *   UPROPERTY(FieldNotify)
 *   FWidgetEventField SomeEvent;
 *
 *   void OnSomeEvent()
 *   {
 *     BroadcastFieldValueChanged(FFieldNotificationClassDescriptor::Text);
 *   }
 * };
};
 */
USTRUCT(BlueprintType)
struct FWidgetEventField
{
	GENERATED_BODY()
};
