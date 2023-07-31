// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorTransportCoordinator.h"

#include "WaveformEditorRenderData.h"

/** Zoom Formula
E.g: I am receiving a playback ratio (PBR) in the range 0 to 1
To position the playhead accordingly, this is the formula to follow:

PH = (PBR - LB) / ZR

PH = Playhead Position
ZR = Zoom Ratio
LB = Lower Displayed Waveform Bound

from here, you can rotate as needed. 
e.g to find PBR
PBR = (PH * ZR) + LB

This formula can be used to position any element (e.g. a marker) given in a 0 to 1 position range where
0 is the beginning time of the waveform and 1 the end time
*/

FWaveformEditorTransportCoordinator::FWaveformEditorTransportCoordinator(TSharedRef<FWaveformEditorRenderData> InRenderData)
	: RenderData(InRenderData)
{
}

FReply FWaveformEditorTransportCoordinator::ReceiveMouseButtonDown(SWidget& WidgetOwner, const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (WidgetOwner.GetTypeAsString() == TEXT("SWaveformViewerOverlay"))
	{
		return HandleWaveformViewerOverlayInteraction(MouseEvent, Geometry);
	}

	if (WidgetOwner.GetTypeAsString() == TEXT("SWaveformEditorTimeRuler"))
	{
		return HandleTimeRulerInteraction(EReceivedInteractionType::MouseButtonDown, MouseEvent, WidgetOwner, Geometry);
	}

	return FReply::Unhandled();
}

FReply FWaveformEditorTransportCoordinator::ReceiveMouseButtonUp(SWidget& WidgetOwner, const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (WidgetOwner.GetTypeAsString() == TEXT("SWaveformEditorTimeRuler"))
	{
		return HandleTimeRulerInteraction(EReceivedInteractionType::MouseButtonUp, MouseEvent, WidgetOwner, Geometry);
	}

	return FReply::Unhandled();
}

FReply FWaveformEditorTransportCoordinator::ReceiveMouseMove(SWidget& WidgetOwner, const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (WidgetOwner.GetTypeAsString() == TEXT("SWaveformEditorTimeRuler"))
	{
		return HandleTimeRulerInteraction(EReceivedInteractionType::MouseMove, MouseEvent, WidgetOwner, Geometry);
	}

	return FReply::Unhandled();
}

FReply FWaveformEditorTransportCoordinator::HandleWaveformViewerOverlayInteraction(const FPointerEvent& MouseEvent, const FGeometry& Geometry)
{
	const bool HandleLeftMouseButton = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	if (HandleLeftMouseButton)
	{
		const float LocalWidth = Geometry.GetLocalSize().X;

		if (LocalWidth > 0.f)
		{
			const float NewPosition = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).X / LocalWidth;
			ScrubPlayhead(NewPosition, false);
		}
	}

	return FReply::Handled();
}

FReply FWaveformEditorTransportCoordinator::HandleTimeRulerInteraction(const EReceivedInteractionType MouseInteractionType, const FPointerEvent& MouseEvent, SWidget& TimeRulerWidget, const FGeometry& Geometry)
{
	const float LocalWidth = Geometry.GetLocalSize().X;

	if (LocalWidth > 0.f)
	{
		const FVector2D ScreenSpacePosition = MouseEvent.GetScreenSpacePosition();
		const FVector2D CursorPosition = Geometry.AbsoluteToLocal(ScreenSpacePosition);
		const float CursorXRatio = CursorPosition.X / LocalWidth;

		switch (MouseInteractionType)
		{
		default:
			break;
		case EReceivedInteractionType::MouseButtonDown:
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				return FReply::Handled().CaptureMouse(TimeRulerWidget.AsShared()).PreventThrottling();
			}
		case EReceivedInteractionType::MouseMove:
			if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
			{
				ScrubPlayhead(CursorXRatio, true);
				return FReply::Handled().CaptureMouse(TimeRulerWidget.AsShared());
			}
		case EReceivedInteractionType::MouseButtonUp:
			if (TimeRulerWidget.HasMouseCapture())
			{
				ScrubPlayhead(CursorXRatio, false);
				return FReply::Handled().ReleaseMouseCapture();
			}
		}
	}
	
	return FReply::Handled();
}

void FWaveformEditorTransportCoordinator::HandleRenderDataUpdate()
{
 	PlaybackRange = RenderData->GetTransformedWaveformBounds();
}

void FWaveformEditorTransportCoordinator::ScrubPlayhead(const float TargetPlayheadPosition, const bool bIsMoving)
{
	const float LowerPlaybackBoundPosition = (PlaybackRange.GetLowerBoundValue() - DisplayRange.GetLowerBoundValue()) / ZoomRatio;
	const float UpperPlaybackBoundPosition = (PlaybackRange.GetUpperBoundValue() - DisplayRange.GetLowerBoundValue()) / ZoomRatio;
	float ClampedTargetPosition = FMath::Clamp(TargetPlayheadPosition, LowerPlaybackBoundPosition, UpperPlaybackBoundPosition);
	float NewPosition = FMath::Clamp(ClampedTargetPosition, 0.f, 1.f);

	if (ClampedTargetPosition > PlayheadLockPosition)
	{
		const float BoundsDelta = (ClampedTargetPosition - PlayheadLockPosition) * ZoomRatio;
		const float NewDisplayUpBound = FMath::Clamp(DisplayRange.GetUpperBoundValue() + BoundsDelta, 0.f, 1.f);

		if (NewDisplayUpBound < 1.f)
		{
			const float NewDisplayLowBound = FMath::Clamp(DisplayRange.GetLowerBoundValue() + BoundsDelta, 0.f, 1.f);
			UpdateDisplayRange(NewDisplayLowBound, NewDisplayUpBound);
			NewPosition = PlayheadLockPosition;
		}
	}

	if (ClampedTargetPosition < 0.f)
	{
		const float BoundsDelta = ClampedTargetPosition * ZoomRatio;
		const float NewDisplayLowBound = FMath::Clamp(DisplayRange.GetLowerBoundValue() + BoundsDelta, 0.f, 1.f);

		if (NewDisplayLowBound > 0.f)
		{
			const float NewDisplayUpBound = FMath::Clamp(DisplayRange.GetUpperBoundValue() + BoundsDelta, 0.f, 1.f);
			UpdateDisplayRange(NewDisplayLowBound, NewDisplayUpBound);
			NewPosition = 0.f;
		}
	}

	MovePlayhead(NewPosition);

	if (OnPlayheadScrubUpdate.IsBound())
	{
		OnPlayheadScrubUpdate.Broadcast(GetSampleFromPlayheadPosition(NewPosition), RenderData->GetOriginalWaveformFrames(), bIsMoving);
	}

	bIsScrubbing = bIsMoving;
}
const bool FWaveformEditorTransportCoordinator::IsScrubbing() const
{
	return bIsScrubbing;
}

void FWaveformEditorTransportCoordinator::ReceivePlayBackRatio(const float NewRatio)
{
	if (bIsScrubbing)
	{
		return;
	}
		
	check(NewRatio >= 0.f && NewRatio <= 1.f);

	const float PlaybackRangeScale = PlaybackRange.Size<float>();
	const float ScaledPlaybackRatio = NewRatio * PlaybackRangeScale + PlaybackRange.GetLowerBoundValue();
	const float NewPlayheadPosition = (ScaledPlaybackRatio - DisplayRange.GetLowerBoundValue()) / ZoomRatio;

	MovePlayhead(NewPlayheadPosition);

	if (PlayheadPosition < 0.f)
	{
		//if sound is looping and we are zoomed, playhead position will be negative when new loop starts
		//hence we reset the view to go back at the beginning of the file
		MovePlayhead(0.f);
		UpdateDisplayRange(PlayheadPosition, ZoomRatio);
	}
	else if (PlayheadPosition >= PlayheadLockPosition)
	{
		const float NewDisplayUpBound = FMath::Clamp(ScaledPlaybackRatio + (ZoomRatio * (1.f - PlayheadLockPosition)), 0.f, 1.f);

		if (NewDisplayUpBound < 1.f)
		{
			//we only lock the playhead if the display upper bound is before the end of the waveform
			//otherwise we let the playhead go to the end
			MovePlayhead(PlayheadLockPosition);

			const float NewDisplayLowBound = FMath::Clamp(ScaledPlaybackRatio - (ZoomRatio * PlayheadLockPosition), 0.f, 1.f);
			UpdateDisplayRange(NewDisplayLowBound, NewDisplayUpBound);
		}
	}
}

void FWaveformEditorTransportCoordinator::OnZoomLevelChanged(const float NewLevel)
{
	UpdateZoomRatioAndDisplayRange(NewLevel);
}

float FWaveformEditorTransportCoordinator::ConvertAbsoluteRatioToZoomed(const float InAbsoluteRatio) const
{
	return (InAbsoluteRatio - DisplayRange.GetLowerBoundValue()) / ZoomRatio;
}

float FWaveformEditorTransportCoordinator::ConvertZoomedRatioToAbsolute(const float InZoomedRatio) const
{
	return InZoomedRatio * ZoomRatio + DisplayRange.GetLowerBoundValue();
}

TRange<float> FWaveformEditorTransportCoordinator::GetDisplayRange() const
{
	return DisplayRange;
}

void FWaveformEditorTransportCoordinator::MovePlayhead(const float InPlayheadPosition)
{
	PlayheadPosition = InPlayheadPosition;
}

void FWaveformEditorTransportCoordinator::UpdateZoomRatioAndDisplayRange(const float NewZoomRatio)
{
	const float ClampedZoomRatio = FMath::Clamp(NewZoomRatio, UE_KINDA_SMALL_NUMBER, 1.f);
	const float ZoomRatioDelta = ZoomRatio - ClampedZoomRatio;
 	const float DeltaOutsideBoundaries = DisplayRange.GetUpperBoundValue() - (DisplayRange.GetUpperBoundValue() - ZoomRatioDelta);
	
	const float DeltaStepL = DeltaOutsideBoundaries * PlayheadPosition;
	const float DeltaStepR = DeltaOutsideBoundaries * (1.f - PlayheadPosition);

	float MinDisplayRangeValue = DeltaStepL + DisplayRange.GetLowerBoundValue();
	float MaxDisplayRangeValue = DisplayRange.GetUpperBoundValue() - DeltaStepR;

	const float CurrentPlaybackPosition = (ZoomRatio * PlayheadPosition) + DisplayRange.GetLowerBoundValue();
	
	//if Min < 0 or Max > 1 we can't zoom out further on that side
	//hence we shift the zooming entirely on the opposite side
	if (MinDisplayRangeValue < 0.f)
	{
		const float ExceedingDelta = FMath::Abs(MinDisplayRangeValue);
		MaxDisplayRangeValue = MaxDisplayRangeValue + ExceedingDelta;
		MinDisplayRangeValue = 0.f;
	}
	else if (MaxDisplayRangeValue > 1.f)
	{
		const float ExceedingDelta = MaxDisplayRangeValue - 1.f;
		MinDisplayRangeValue = MinDisplayRangeValue - ExceedingDelta;
		MaxDisplayRangeValue = 1.f;
	}

	const float NewPlayheadPosition = (CurrentPlaybackPosition - MinDisplayRangeValue) / ClampedZoomRatio;
	ZoomRatio = ClampedZoomRatio;
	UpdateDisplayRange(MinDisplayRangeValue, MaxDisplayRangeValue);
	MovePlayhead(NewPlayheadPosition);
}

void FWaveformEditorTransportCoordinator::Stop()
{
	ReceivePlayBackRatio(0.f);
	UpdateDisplayRange(0.f, ZoomRatio);
}

void FWaveformEditorTransportCoordinator::UpdateDisplayRange(const float MinValue, const float MaxValue)
{
	check(MinValue < MaxValue);

	DisplayRange.SetLowerBoundValue(MinValue);
	DisplayRange.SetUpperBoundValue(MaxValue);

	if (OnDisplayRangeUpdated.IsBound())
	{
		OnDisplayRangeUpdated.Broadcast(DisplayRange);
	}
}

bool FWaveformEditorTransportCoordinator::IsRatioWithinDisplayRange(const float Ratio) const
{
	return DisplayRange.Contains(Ratio);
}

uint32 FWaveformEditorTransportCoordinator::GetSampleFromPlayheadPosition(const float InPlayheadPosition) const
{
	check(InPlayheadPosition >= 0.f && InPlayheadPosition <= 1.f);
	const float PlayBackRatio = ((InPlayheadPosition * ZoomRatio) + DisplayRange.GetLowerBoundValue() - PlaybackRange.GetLowerBoundValue()) / PlaybackRange.Size<float>();
	const uint32 TargetSample = FMath::RoundToInt32(PlayBackRatio * RenderData->GetOriginalWaveformFrames());
	return TargetSample;
}

const float FWaveformEditorTransportCoordinator::GetPlayheadPosition() const
{
	return PlayheadPosition;
}
