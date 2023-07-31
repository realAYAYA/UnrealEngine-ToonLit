// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	 GestureRecognizer - handles detecting when gestures happen
=============================================================================*/

#include "GestureRecognizer.h"
#include "GameFramework/PlayerInput.h"

void FGestureRecognizer::DetectGestures(const FVector (&Touches)[EKeys::NUM_TOUCH_KEYS], UPlayerInput* PlayerInput, float DeltaTime)
{
	// how many fingers currently held down?
	int32 TouchCount = 0;
	for (int32 TouchIndex = 0; TouchIndex < EKeys::NUM_TOUCH_KEYS; TouchIndex++)
	{
		if (Touches[TouchIndex].Z != 0)
		{
			TouchCount++;
		}
	}

	// Further processing is only needed if there were or are active touches.
	if (PreviousTouchCount != 0 || TouchCount != 0)
	{
		// place new anchor points
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(AnchorPoints); Index++)
		{
			if (PreviousTouchCount < Index + 1 && TouchCount >= Index + 1)
			{
				AnchorPoints[Index] = FVector2D(Touches[Index]);
			}
		}

		// handle different types of two finger gestures
		if (TouchCount >= 2)
		{
			float* CurrentAlpha = CurrentGestureValues.Find(EKeys::Gesture_Pinch);

			//#jira UE-51262 values for pinch input produce very different results for same area on android device
			FVector2D CurrentPinchPoint_Start = FVector2D(Touches[0]);
			FVector2D CurrentPinchPoint_End = FVector2D(Touches[1]);
			const float PinchThreshold = 10000.0f;

			if (CurrentAlpha == nullptr)
			{
				//if the pinch points are close enough to those from the last pinch, continue with the previous starting distance
				if (AnchorDistanceSq == 0 ||
					(LastPinchPoint_Start - CurrentPinchPoint_Start).SizeSquared() > PinchThreshold || 
					(LastPinchPoint_End - CurrentPinchPoint_End).SizeSquared() > PinchThreshold)
				{
					// remember the starting distance
					SetAnchorDistanceSquared(CurrentPinchPoint_Start, CurrentPinchPoint_End);
				}

				// alpha of 1 is initial pinch anchor distance
				CurrentGestureValues.Add(EKeys::Gesture_Pinch, 1.0f);
				CurrentAlpha = CurrentGestureValues.Find(EKeys::Gesture_Pinch);
			}

			// calculate current alpha
			float NewDistanceSq = (CurrentPinchPoint_Start - CurrentPinchPoint_End).SizeSquared();
			*CurrentAlpha = NewDistanceSq / AnchorDistanceSq;
			HandleGesture(PlayerInput, EKeys::Gesture_Pinch, true, false);

			LastPinchPoint_Start = CurrentPinchPoint_Start;
			LastPinchPoint_End = CurrentPinchPoint_End;

			// calculate the angle of the vector between the touch points
			float NewAngle = FMath::Atan2(Touches[0].Y - Touches[1].Y, Touches[0].X - Touches[1].X);
			NewAngle = FRotator::ClampAxis(FMath::RadiansToDegrees(NewAngle));

			float* CurrentAngle = CurrentGestureValues.Find(EKeys::Gesture_Rotate);
			if (CurrentAngle == nullptr)
			{
				// save the starting angle
				StartAngle = NewAngle;

				// use 0 as the initial angle value; subsequent angles will be relative
				CurrentGestureValues.Add(EKeys::Gesture_Rotate, 0.0f);
				HandleGesture(PlayerInput, EKeys::Gesture_Rotate, true, false);
			}
			else
			{
				*CurrentAngle = NewAngle - StartAngle;

				// Gestures are only processed for IE_Pressed events, so treat this like another "start"
				HandleGesture(PlayerInput, EKeys::Gesture_Rotate, true, false);
			}
		}

		// Handle pinch and rotate release.
		if (PreviousTouchCount >= 2 && TouchCount < 2)
		{
			HandleGesture(PlayerInput, EKeys::Gesture_Pinch, false, true);
			HandleGesture(PlayerInput, EKeys::Gesture_Rotate, false, true);
		}

		if (PreviousTouchCount == 0 && TouchCount == 1)
		{
			// initialize the flick
			FlickTime = 0.0f;
		}
		else if (PreviousTouchCount == 1 && TouchCount == 1)
		{
			// remember the position for when we let go
			FlickCurrent = FVector2D(Touches[0].X, Touches[0].Y);
			FlickTime += DeltaTime;
		}
		else if (PreviousTouchCount >= 1 && TouchCount == 0)
		{
			// must be a fast flick
			if (FlickTime < 0.25f && (FlickCurrent - AnchorPoints[0]).SizeSquared() > 10000.f)
			{
				// this is the angle from +X in screen space, meaning right is 0, up is 90, left is 180, down is 270
				float Angle = FMath::Atan2(-(FlickCurrent.Y - AnchorPoints[0].Y), FlickCurrent.X - AnchorPoints[0].X);
				Angle = FRotator::ClampAxis(FMath::RadiansToDegrees(Angle));

				// flicks are one-shot, so we start and end in the same frame
				CurrentGestureValues.Add(EKeys::Gesture_Flick, Angle);
				HandleGesture(PlayerInput, EKeys::Gesture_Flick, true, true);
			}
		}
	}	

	// remember for next frame
	PreviousTouchCount = TouchCount;
}

void FGestureRecognizer::SetAnchorDistanceSquared(const FVector2D FirstPoint, const FVector2D SecontPoint)
{
	AnchorDistanceSq = (FirstPoint - SecontPoint).SizeSquared();
}

void FGestureRecognizer::HandleGesture(UPlayerInput* PlayerInput, FKey Gesture, bool bStarted, bool bEnded)
{
	float* Value = CurrentGestureValues.Find(Gesture);
	if (Value)
	{
		const EInputEvent GestureEvent = (bStarted ? IE_Pressed : bEnded ? IE_Released : IE_Repeat);
		PlayerInput->InputGesture(Gesture, GestureEvent, *Value);

		// remove it if the gesture is complete
		if (bEnded)
		{
			CurrentGestureValues.Remove(Gesture);
		}
	}
}

