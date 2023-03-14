// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationTrimFadeRenderer.h"

FWaveformTransformationTrimFadeRenderer::FWaveformTransformationTrimFadeRenderer(const TObjectPtr<UWaveformTransformationTrimFade> TransformationToRender)
{
	check(TransformationToRender);
	TrimFadeTransform = TransformationToRender;
}

int32 FWaveformTransformationTrimFadeRenderer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	LayerId = DrawTrimHandles(AllottedGeometry, OutDrawElements, LayerId);
	LayerId = DrawFadeCurves(AllottedGeometry, OutDrawElements, LayerId);

	return LayerId;
}

int32 FWaveformTransformationTrimFadeRenderer::DrawTrimHandles(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	const bool bRenderLowerBound = StartTimeHandleX >= 0.f;
	const bool bRenderUpperBound = EndTimeHandleX <= AllottedGeometry.Size.X;

	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	if (bRenderLowerBound)
	{
		LinePoints[0] = FVector2D(StartTimeHandleX, 0.f);
		LinePoints[1] = FVector2D(StartTimeHandleX, AllottedGeometry.Size.Y);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Green,
			false
		);
	}

	if (bRenderUpperBound)
	{
		LinePoints[0] = FVector2D(EndTimeHandleX, 0.f);
		LinePoints[1] = FVector2D(EndTimeHandleX, AllottedGeometry.Size.Y);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			FLinearColor::Red,
			false
		);
	}

	return LayerId;
}

int32 FWaveformTransformationTrimFadeRenderer::DrawFadeCurves(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
	if (FadeInCurvePoints.Num() > 0)
	{
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FadeInCurvePoints,
			ESlateDrawEffect::None,
			FLinearColor::Yellow
		);

	}

	if (FadeOutCurvePoints.Num() > 0)
	{
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			FadeOutCurvePoints,
			ESlateDrawEffect::None,
			FLinearColor::Yellow
		);

	}

	return LayerId;
}

void FWaveformTransformationTrimFadeRenderer::GenerateFadeCurves(const FGeometry& AllottedGeometry)
{
	const float FadeInFrames = TrimFadeTransform->StartFadeTime * TransformationWaveInfo.SampleRate;
	const uint32 FadeInPixelLenght = FadeInFrames * PixelsPerFrame;
	FadeInStartX = FMath::RoundToInt32(StartTimeHandleX);
	FadeInEndX = FMath::RoundToInt32(FMath::Clamp(StartTimeHandleX + FadeInPixelLenght, StartTimeHandleX, EndTimeHandleX));
	
	const uint32 DisplayedFadeInPixelLenght = FadeInEndX - FadeInStartX;
	FadeInCurvePoints.SetNumUninitialized(DisplayedFadeInPixelLenght);

	for (uint32 Pixel = 0; Pixel < DisplayedFadeInPixelLenght; ++Pixel)
	{
		const double FadeFraction = (float)Pixel / FadeInPixelLenght;
		const double CurveValue = Pixel != FadeInPixelLenght - 1 ? 1.f - FMath::Pow(FadeFraction, TrimFadeTransform->StartFadeCurve) : 0.f;

		const uint32 XCoordinate = Pixel + FadeInStartX;
		FadeInCurvePoints[Pixel] = FVector2D(XCoordinate, CurveValue * AllottedGeometry.Size.Y);
	}

	const float FadeOutFrames = TrimFadeTransform->EndFadeTime * TransformationWaveInfo.SampleRate;
	const float FadeOutPixelLength = FadeOutFrames * PixelsPerFrame;
	FadeOutStartX = FMath::RoundToInt32(FMath::Clamp(EndTimeHandleX - FadeOutPixelLength, StartTimeHandleX, EndTimeHandleX));
	FadeOutEndX = FMath::RoundToInt32(EndTimeHandleX);
	
	const uint32 DisplayedFadeOutPixelLength = FadeOutEndX - FadeOutStartX;
	FadeOutCurvePoints.SetNumUninitialized(DisplayedFadeOutPixelLength);
	const uint32 FadeOutPixelOffset = FadeOutPixelLength - DisplayedFadeOutPixelLength;

	for (uint32 Pixel = 0; Pixel < DisplayedFadeOutPixelLength; ++Pixel)
	{
		const double FadeFraction = (float)(Pixel + FadeOutPixelOffset) / FadeOutPixelLength;
		const double CurveValue = Pixel != FadeOutPixelLength - 1 ? FMath::Pow(FadeFraction, TrimFadeTransform->EndFadeCurve) : 1.f;

		const uint32 XCoordinate = Pixel + FadeOutStartX;
		FadeOutCurvePoints[Pixel] = FVector2D(XCoordinate, CurveValue * AllottedGeometry.Size.Y);
	}
}

FCursorReply FWaveformTransformationTrimFadeRenderer::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	const FVector2D LocalCursorPosition = GetLocalCursorPosition(CursorEvent, MyGeometry);

	if (TrimFadeInteractionType == ETrimFadeInteractionType::ScrubbingFadeIn || TrimFadeInteractionType == ETrimFadeInteractionType::ScrubbingFadeOut)
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}

	if (IsCursorInFadeInInteractionRange(LocalCursorPosition, MyGeometry) || IsCursorInFadeOutInteractionRange(LocalCursorPosition, MyGeometry))
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}

	if (TrimFadeInteractionType == ETrimFadeInteractionType::ScrubbingLeftHandle || TrimFadeInteractionType == ETrimFadeInteractionType::ScrubbingRightHandle || StartTimeInteractionXRange.Contains(LocalCursorPosition.X) || EndTimeInteractionXRange.Contains(LocalCursorPosition.X))
	{
		return FCursorReply::Cursor(EMouseCursor::ResizeLeftRight);
	}

	return FCursorReply::Unhandled();
}

FReply FWaveformTransformationTrimFadeRenderer::OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalCursorPosition = GetLocalCursorPosition(MouseEvent, MyGeometry);

	if (IsCursorInFadeInInteractionRange(LocalCursorPosition, MyGeometry))
	{		
		const float FadeCurveDelta = MouseEvent.GetWheelDelta() * MouseWheelStep;
		TrimFadeTransform->StartFadeCurve = FMath::Clamp(TrimFadeTransform->StartFadeCurve + FadeCurveDelta, 0.f, 10.f);
		NotifyTransformationPropertyChanged(TrimFadeTransform, GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartFadeCurve), EPropertyChangeType::ValueSet);
		return FReply::Handled();
	}

	if (IsCursorInFadeOutInteractionRange(LocalCursorPosition, MyGeometry))
	{
		const float FadeCurveDelta = MouseEvent.GetWheelDelta() * MouseWheelStep;
		TrimFadeTransform->EndFadeCurve = FMath::Clamp(TrimFadeTransform->EndFadeCurve + FadeCurveDelta, 0.f, 10.f);
		NotifyTransformationPropertyChanged(TrimFadeTransform, GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndFadeCurve), EPropertyChangeType::ValueSet);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FVector2D FWaveformTransformationTrimFadeRenderer::GetLocalCursorPosition(const FPointerEvent& MouseEvent, const FGeometry& EventGeometry) const
{
	const FVector2D ScreenSpacePosition = MouseEvent.GetScreenSpacePosition();
	return  EventGeometry.AbsoluteToLocal(ScreenSpacePosition);
}

float FWaveformTransformationTrimFadeRenderer::ConvertXRatioToTime(const float InRatio) const
{
	const float NumFrames = TransformationWaveInfo.NumAvilableSamples / TransformationWaveInfo.NumChannels;
	const float FrameSelected = NumFrames * InRatio;
	return FrameSelected / TransformationWaveInfo.SampleRate;
}

void FWaveformTransformationTrimFadeRenderer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{ 
	if (!TrimFadeTransform)
	{
		return;
	}

	const float NumFrames = TransformationWaveInfo.NumAvilableSamples / TransformationWaveInfo.NumChannels;
	const double FirstFrame = FMath::Clamp((TrimFadeTransform->StartTime * TransformationWaveInfo.SampleRate) , 0.f, NumFrames);
	const double EndFrame = FMath::Clamp((TrimFadeTransform->EndTime * TransformationWaveInfo.SampleRate), FirstFrame, NumFrames);

	PixelsPerFrame = FMath::Max(AllottedGeometry.GetLocalSize().X / NumFrames, 0.0);

	StartTimeHandleX = FirstFrame * PixelsPerFrame;
	EndTimeHandleX = EndFrame * PixelsPerFrame;

	GenerateFadeCurves(AllottedGeometry);
	UpdateInteractionRange();
}

void FWaveformTransformationTrimFadeRenderer::UpdateInteractionRange()
{
	StartTimeInteractionXRange.SetLowerBoundValue(StartTimeHandleX - InteractionPixelXDelta);
	StartTimeInteractionXRange.SetUpperBoundValue(StartTimeHandleX + InteractionPixelXDelta);
	EndTimeInteractionXRange.SetLowerBoundValue(EndTimeHandleX - InteractionPixelXDelta);
	EndTimeInteractionXRange.SetUpperBoundValue(EndTimeHandleX + InteractionPixelXDelta);
	FadeInInteractionXRange.SetLowerBoundValue(FadeInEndX - InteractionPixelXDelta);
	FadeInInteractionXRange.SetUpperBoundValue(FadeInEndX + InteractionPixelXDelta);
	FadeOutInteractionXRange.SetLowerBoundValue(FadeOutStartX - InteractionPixelXDelta);
	FadeOutInteractionXRange.SetUpperBoundValue(FadeOutStartX + InteractionPixelXDelta);
}

FReply FWaveformTransformationTrimFadeRenderer::OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalCursorPosition = GetLocalCursorPosition(MouseEvent, MyGeometry);

	const bool bIsLeftMouseButton = MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton);

	if (bIsLeftMouseButton)
	{
		TrimFadeInteractionType = GetInteractionTypeFromCursorPosition(LocalCursorPosition, MyGeometry);

		if (TrimFadeInteractionType != ETrimFadeInteractionType::None)
		{
			return FReply::Handled().CaptureMouse(OwnerWidget.AsShared()).PreventThrottling();
		}
	}

	return FReply::Unhandled();
}


FReply FWaveformTransformationTrimFadeRenderer::OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && TrimFadeInteractionType != ETrimFadeInteractionType::None)
	{
		SetPropertyValueDependingOnInteractionType(MouseEvent, MyGeometry, EPropertyChangeType::Interactive);

		return FReply::Handled().CaptureMouse(OwnerWidget.AsShared());
	}

	return FReply::Unhandled();
}

FReply FWaveformTransformationTrimFadeRenderer::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (TrimFadeInteractionType != ETrimFadeInteractionType::None)
	{
		SetPropertyValueDependingOnInteractionType(MouseEvent, MyGeometry, EPropertyChangeType::ValueSet);

		TrimFadeInteractionType = ETrimFadeInteractionType::None;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void FWaveformTransformationTrimFadeRenderer::SetPropertyValueDependingOnInteractionType(const FPointerEvent& MouseEvent, const FGeometry& WidgetGeometry, const EPropertyChangeType::Type DesiredChangeType)
{
	const FVector2D LocalCursorPosition = GetLocalCursorPosition(MouseEvent, WidgetGeometry);
	const float LocalCursorXRatio = FMath::Clamp(LocalCursorPosition.X / WidgetGeometry.GetLocalSize().X, 0.f, 1.f);
	const float SelectedTime = ConvertXRatioToTime(LocalCursorXRatio);

	switch (TrimFadeInteractionType)
	{
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::None:
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingLeftHandle:
		TrimFadeTransform->StartTime = SelectedTime;
		NotifyTransformationPropertyChanged(TrimFadeTransform, GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartTime), DesiredChangeType);
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingRightHandle:
		TrimFadeTransform->EndTime = SelectedTime;
		NotifyTransformationPropertyChanged(TrimFadeTransform, GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndTime), DesiredChangeType);
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingFadeIn:
		TrimFadeTransform->StartFadeTime = FMath::Clamp(SelectedTime - TrimFadeTransform->StartTime, 0.f, TNumericLimits<float>().Max());
		NotifyTransformationPropertyChanged(TrimFadeTransform, GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartFadeTime), DesiredChangeType);
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingFadeOut:
		TrimFadeTransform->EndFadeTime = FMath::Clamp(TrimFadeTransform->EndTime - SelectedTime, 0.f, TNumericLimits<float>().Max());
		NotifyTransformationPropertyChanged(TrimFadeTransform, GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndFadeTime), DesiredChangeType);
		break;
	default:
		break;
	}
}


FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType FWaveformTransformationTrimFadeRenderer::GetInteractionTypeFromCursorPosition(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry) const
{
	if (IsCursorInFadeInInteractionRange(InLocalCursorPosition, WidgetGeometry))
	{
		return ETrimFadeInteractionType::ScrubbingFadeIn;
	}

	if (IsCursorInFadeOutInteractionRange(InLocalCursorPosition, WidgetGeometry))
	{
		return ETrimFadeInteractionType::ScrubbingFadeOut;
	}

	if (StartTimeInteractionXRange.Contains(InLocalCursorPosition.X))
	{
		return ETrimFadeInteractionType::ScrubbingLeftHandle;
	}

	if (EndTimeInteractionXRange.Contains(InLocalCursorPosition.X))
	{
		return ETrimFadeInteractionType::ScrubbingRightHandle;
	}

	return ETrimFadeInteractionType::None;
}

bool FWaveformTransformationTrimFadeRenderer::IsCursorInFadeInInteractionRange(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry) const
{
	return FadeInInteractionXRange.Contains(InLocalCursorPosition.X)
		&& InLocalCursorPosition.Y < WidgetGeometry.GetLocalSize().Y* InteractionRatioYDelta;
}

bool FWaveformTransformationTrimFadeRenderer::IsCursorInFadeOutInteractionRange(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry) const
{
	return FadeOutInteractionXRange.Contains(InLocalCursorPosition.X)
		&& InLocalCursorPosition.Y < WidgetGeometry.GetLocalSize().Y* InteractionRatioYDelta;
}