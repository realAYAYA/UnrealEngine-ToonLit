// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/MinElement.h"
#include "Input/Events.h"
#include "Misc/FrameRate.h"
#include "Math/Axis.h"
#include "Math/Vector2D.h"

struct FCurveSnapMetrics
{
	FCurveSnapMetrics()
	{
		bSnapOutputValues = 0;
		bSnapInputValues = 0;
	}

	/** Whether we are snapping to the output snap interval */
	uint8 bSnapOutputValues : 1;

	/** Whether we are snapping to the input snap rate */
	uint8 bSnapInputValues : 1;

	/** Grid lines to snap to */
	TArray<double> AllGridLines;

	/** The input snap rate */
	FFrameRate InputSnapRate;

	/** Snap the specified input time to the input snap rate if necessary */
	FORCEINLINE double SnapInputSeconds(double InputTime)
	{
		return bSnapInputValues && InputSnapRate.IsValid() ? (InputTime * InputSnapRate).RoundToFrame() / InputSnapRate : InputTime;
	}
	
	/** Snap the specified output value to the output snap interval if necessary */
	FORCEINLINE double SnapOutput(double OutputValue)
	{
		return bSnapOutputValues ? *Algo::MinElement(AllGridLines, 
			[OutputValue](double Val1, double Val2) { return FMath::Abs(Val1 - OutputValue) < FMath::Abs(Val2 - OutputValue); }
		) : OutputValue;
	}
};

/**
 * Utility struct that acts as a way to control snapping to a specific axis based on UI settings, or shift key.
 */
struct FCurveEditorAxisSnap
{
	/**
	 * Snapping is not stateless but we want to manage it through the central area. This allows
	 * state to be passed into from the calling area but still centralize the logic of handling it.
	*/
	struct FSnapState
	{
		FSnapState()
		{
			Reset();
		}

		void Reset()
		{
			MouseLockVector = FVector2D::UnitVector;
			MousePosOnShiftStart = FVector2D::ZeroVector;
			bHasPassedThreshold = false;
			bHasStartPosition = false;
		}

		FVector2D MousePosOnShiftStart;
		FVector2D MouseLockVector;
		bool bHasPassedThreshold;
		bool bHasStartPosition;
	};

	/** Can be set to either X, Y, or None to control which axis GetSnappedPosition snaps to. User can override None by pressing shift. */
	EAxisList::Type RestrictedAxisList;

	FCurveEditorAxisSnap()
	{
		RestrictedAxisList = EAxisList::None;
	}

	FVector2D GetSnappedPosition(FVector2D InitialPosition, FVector2D LastPosition, FVector2D CurrentPosition, const FPointerEvent& MouseEvent, FSnapState& InOutSnapState, const bool bIgnoreAxisLock = false)
	{
		check(RestrictedAxisList == EAxisList::Type::None || RestrictedAxisList == EAxisList::Type::X || RestrictedAxisList == EAxisList::Type::Y);

		// If we're ignoring axis lock (such as for UI) we allow them to use shift anyways
		bool bCanUseShift = bIgnoreAxisLock || RestrictedAxisList == EAxisList::Type::None;

		FVector2D MouseLockVector = FVector2D::UnitVector;
		if (bCanUseShift)
		{
			// Snapping works by determining which direction the mouse is moving in while shift is being held
			// down. Once you have moved far enough to pass the threshold, you're locked into that direction
			// but if you release shift, it will reset the direction and allow you to pick a new direction.
			if (MouseEvent.IsShiftDown())
			{
				if (!InOutSnapState.bHasStartPosition)
				{
					InOutSnapState.MousePosOnShiftStart = LastPosition;
					InOutSnapState.bHasStartPosition = true;
				}
				// If they have passed the threshold they should have a lock vector they're snapped to.
				if (!InOutSnapState.bHasPassedThreshold)
				{
					InOutSnapState.MouseLockVector = MouseLockVector;
					
					// They have not passed the threshold yet, let's see if they've passed it now.
					FVector2D DragDelta = CurrentPosition - InOutSnapState.MousePosOnShiftStart;
					if (DragDelta.Size() > 0.001f)
					{
						InOutSnapState.bHasPassedThreshold = true;
						InOutSnapState.MouseLockVector = FVector2D::UnitVector;
						if (FMath::Abs(DragDelta.X) > FMath::Abs(DragDelta.Y))
						{
							InOutSnapState.MouseLockVector.Y = 0;
						}
						else
						{
							InOutSnapState.MouseLockVector.X = 0;
						}
					}
				}
			}
			else
			{
				// If they don't have shift pressed just disable the lock.
				InOutSnapState.Reset();
			}

			MouseLockVector = InOutSnapState.MouseLockVector;
		}
		else if (!bIgnoreAxisLock && RestrictedAxisList == EAxisList::Type::X)
		{
			MouseLockVector.Y = 0;
		}
		else if (!bIgnoreAxisLock && RestrictedAxisList == EAxisList::Type::Y)
		{
			MouseLockVector.X = 0;
		}

		return InitialPosition + (CurrentPosition - InitialPosition) * MouseLockVector;
	}
};
