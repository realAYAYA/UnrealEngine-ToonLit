// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputProcessor.h"

// Macro to avoid repeated code in broadcasting delegates out with timestamps
// Also broadcasts to delegates bound to AnyKey
#define BROADCAST_REGISTERED_KEY_DELEGATES(StoreVariable, EventVariable, DelegateType) \
{ \
	const double EvaluationTime = FPlatformTime::Seconds(); \
    if (TInputDelegateStore<DelegateType>::DelegateArrayType* SpecificKeyDelegates = StoreVariable.FindDelegateArray(EventVariable.GetKey())) \
	{ \
		for (TTimestampedDelegateStore<DelegateType>& DelegateEntry : *SpecificKeyDelegates) \
		{ \
    		float DeltaTime; \
    		const DelegateType& Delegate = DelegateEntry.GetDelegate(EvaluationTime, DeltaTime); \
    		bool bSuccess = Delegate.ExecuteIfBound(DeltaTime, EventVariable); \
		} \
	} \
	if (TInputDelegateStore<DelegateType>::DelegateArrayType* AnyKeyDelegates = StoreVariable.FindDelegateArray(EKeys::AnyKey)) \
	{ \
		for (TTimestampedDelegateStore<DelegateType>& DelegateEntry : *AnyKeyDelegates) \
		{ \
			float DeltaTime; \
    		const DelegateType& Delegate = DelegateEntry.GetDelegate(EvaluationTime, DeltaTime); \
    		bool bSuccess = Delegate.ExecuteIfBound(DeltaTime, EventVariable); \
		} \
	} \
}

// Macro to avoid repeated code in broadcasting delegates out with timestamps
#define BROADCAST_REGISTERED_POINTER_DELEGATES(StoreVariable, EventVariable, DelegateType) \
{ \
	if (TInputDelegateStore<DelegateType>::DelegateArrayType* DelegateArray = StoreVariable.FindDelegateArray(EventVariable.GetEffectingButton())) \
	{ \
		const double EvaluationTime = FPlatformTime::Seconds(); \
		for (TTimestampedDelegateStore<DelegateType>& DelegateEntry : *DelegateArray) \
		{ \
    		float DeltaTime; \
    		const DelegateType& Delegate = DelegateEntry.GetDelegate(EvaluationTime, DeltaTime); \
    		bool bSuccess = Delegate.ExecuteIfBound(DeltaTime, EventVariable); \
		} \
	} \
}

void FVCamInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
}

bool FVCamInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Prevent Key Down firing multiple times while held
	if (!InKeyEvent.IsRepeat())
	{
		BROADCAST_REGISTERED_KEY_DELEGATES(KeyDownDelegateStore, InKeyEvent, FKeyInputDelegate);
	}

	if (InKeyEvent.GetKey().IsGamepadKey() && bShouldConsumeGamepadInput)
	{
		return true;
	}
	return false;
}

bool FVCamInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	BROADCAST_REGISTERED_KEY_DELEGATES(KeyUpDelegateStore, InKeyEvent, FKeyInputDelegate);

	if (InKeyEvent.GetKey().IsGamepadKey() && bShouldConsumeGamepadInput)
	{
		return true;
	}
	return false;
}

bool FVCamInputProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp,
	const FAnalogInputEvent& InAnalogInputEvent)
{
	BROADCAST_REGISTERED_KEY_DELEGATES(AnalogDelegateStore, InAnalogInputEvent, FAnalogInputDelegate);
	
	if (InAnalogInputEvent.GetKey().IsGamepadKey() && bShouldConsumeGamepadInput)
	{
		return true;
	}
	return bShouldConsumeGamepadInput;
}

bool FVCamInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	BROADCAST_REGISTERED_POINTER_DELEGATES(MouseMoveDelegateStore, MouseEvent, FPointerInputDelegate);
	return false;
}

bool FVCamInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	BROADCAST_REGISTERED_POINTER_DELEGATES(MouseButtonDownDelegateStore, MouseEvent, FPointerInputDelegate);
	return false;
}

bool FVCamInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	BROADCAST_REGISTERED_POINTER_DELEGATES(MouseButtonUpDelegateStore, MouseEvent, FPointerInputDelegate);
	return false;
}

bool FVCamInputProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	BROADCAST_REGISTERED_POINTER_DELEGATES(MouseButtonDoubleClickDelegateStore, MouseEvent, FPointerInputDelegate);
	return false;
}

// Currently only handles Wheel events
bool FVCamInputProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent,
	const FPointerEvent* InGestureEvent)
{
	if (!InGestureEvent)
	{
		BROADCAST_REGISTERED_POINTER_DELEGATES(MouseWheelDelegateStore, InWheelEvent, FPointerInputDelegate);
	}
	return false;
}

bool FVCamInputProcessor::HandleMotionDetectedEvent(FSlateApplication& SlateApp, const FMotionEvent& MotionEvent)
{
	return false;
}
