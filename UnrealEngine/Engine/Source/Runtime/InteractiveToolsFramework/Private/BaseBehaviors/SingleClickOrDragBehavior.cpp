// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseBehaviors/SingleClickOrDragBehavior.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SingleClickOrDragBehavior)



void USingleClickOrDragInputBehavior::Initialize(IClickBehaviorTarget* ClickTargetIn, IClickDragBehaviorTarget* DragTargetIn)
{
	this->ClickTarget = ClickTargetIn;
	this->DragTarget = DragTargetIn;
}


FInputCaptureRequest USingleClickOrDragInputBehavior::WantsCapture(const FInputDeviceState& Input)
{
	bImmediatelyBeginDragInBeginCapture = false;
	if (IsPressed(Input) && (ModifierCheckFunc == nullptr || ModifierCheckFunc(Input)) )
	{
		FInputRayHit ClickHitResult = ClickTarget->IsHitByClick(GetDeviceRay(Input));
		if (ClickHitResult.bHit)
		{
			return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any, ClickHitResult.HitDepth);
		}
		else if (bBeginDragIfClickTargetNotHit)
		{
			FInputRayHit DragHitResult = DragTarget->CanBeginClickDragSequence(GetDeviceRay(Input));
			if (DragHitResult.bHit)
			{
				bImmediatelyBeginDragInBeginCapture = true;
				return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any, DragHitResult.HitDepth);
			}
		}
	}
	return FInputCaptureRequest::Ignore();
}


FInputCaptureUpdate USingleClickOrDragInputBehavior::BeginCapture(const FInputDeviceState& Input, EInputCaptureSide eSide)
{
	Modifiers.UpdateModifiers(Input, ClickTarget);
	Modifiers.UpdateModifiers(Input, DragTarget);

	ensure(Input.IsFromDevice(EInputDevices::Mouse));	// todo: handle other devices
	MouseDownPosition2D = Input.Mouse.Position2D;	
	MouseDownRay = Input.Mouse.WorldRay;
	CaptureSide = eSide;
	bInDrag = false;

	// If the WantsCapture() hit-test did not hit the target, then start the drag action directly, instead of starting
	// a click and then switching to a drag (could alternately repeat the hit-test here...)
	if (bImmediatelyBeginDragInBeginCapture)
	{
		bInDrag = true;
		OnClickDragPressInternal(Input, CaptureSide);
	}

	return FInputCaptureUpdate::Begin(this, EInputCaptureSide::Any);
}


FInputCaptureUpdate USingleClickOrDragInputBehavior::UpdateCapture(const FInputDeviceState& Input, const FInputCaptureData& Data)
{
	Modifiers.UpdateModifiers(Input, ClickTarget);
	Modifiers.UpdateModifiers(Input, DragTarget);

	// check if mouse has moved far enough that we want to swap to drag behavior
	if (bInDrag == false)
	{
		float MouseMovement = static_cast<float>(FVector2D::Distance(Input.Mouse.Position2D, MouseDownPosition2D));
		if (MouseMovement > ClickDistanceThreshold)
		{
			FInputDeviceState StartInput = Input;
			StartInput.Mouse.Position2D = MouseDownPosition2D;		// attempt to reconstruct the input state that would have existed in WantsCapture/BeginCapture
			StartInput.Mouse.WorldRay = MouseDownRay;
			FInputRayHit DragHitResult = DragTarget->CanBeginClickDragSequence(GetDeviceRay(StartInput));
			if (DragHitResult.bHit)
			{
				bInDrag = true;
				OnClickDragPressInternal(StartInput, CaptureSide);
			}
		}
	}

	if (IsReleased(Input))
	{
		if (bInDrag)
		{
			OnClickDragReleaseInternal(Input, Data);
			bInDrag = false;
		}
		else   // click path
		{
			if (ClickTarget->IsHitByClick(GetDeviceRay(Input)).bHit)
			{
				OnClickedInternal(Input, Data);
			}
		}

		return FInputCaptureUpdate::End();
	}
	else
	{
		if (bInDrag)
		{
			OnClickDragInternal(Input, Data);
		}
		return FInputCaptureUpdate::Continue();
	}
}


void USingleClickOrDragInputBehavior::ForceEndCapture(const FInputCaptureData& data)
{
	if (bInDrag)
	{
		DragTarget->OnTerminateDragSequence();
		bInDrag = false;
	}

	// nothing to do
}


void USingleClickOrDragInputBehavior::OnClickedInternal(const FInputDeviceState& input, const FInputCaptureData& data)
{
	ClickTarget->OnClicked(GetDeviceRay(input));
}



void USingleClickOrDragInputBehavior::OnClickDragPressInternal(const FInputDeviceState& Input, EInputCaptureSide Side)
{
	DragTarget->OnClickPress(GetDeviceRay(Input));
}

void USingleClickOrDragInputBehavior::OnClickDragInternal(const FInputDeviceState& Input, const FInputCaptureData& Data)
{
	DragTarget->OnClickDrag(GetDeviceRay(Input));
}

void USingleClickOrDragInputBehavior::OnClickDragReleaseInternal(const FInputDeviceState& Input, const FInputCaptureData& Data)
{
	DragTarget->OnClickRelease(GetDeviceRay(Input));
}

