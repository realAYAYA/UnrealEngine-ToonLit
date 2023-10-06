// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationTrimFadeRenderer.h"

#include "Layout/Geometry.h"
#include "PropertyHandle.h"
#include "Rendering/DrawElements.h"
#include "WaveformTransformationTrimFade.h"
#include "Widgets/SWidget.h"

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
	const float FadeInFrames = GetPropertyValue<float>(StartFadeTimeHandle) * TransformationWaveInfo.SampleRate;
	const uint32 FadeInPixelLenght = FadeInFrames * PixelsPerFrame;
	FadeInStartX = FMath::RoundToInt32(StartTimeHandleX);
	FadeInEndX = FMath::RoundToInt32(FMath::Clamp(StartTimeHandleX + FadeInPixelLenght, StartTimeHandleX, EndTimeHandleX));
	
	const uint32 DisplayedFadeInPixelLenght = FadeInEndX - FadeInStartX;
	FadeInCurvePoints.SetNumUninitialized(DisplayedFadeInPixelLenght);

	for (uint32 Pixel = 0; Pixel < DisplayedFadeInPixelLenght; ++Pixel)
	{
		const double FadeFraction = (float)Pixel / FadeInPixelLenght;
		const double CurveValue = Pixel != FadeInPixelLenght - 1 ? 1.f - FMath::Pow(FadeFraction, GetPropertyValue<float>(StartFadeCurveHandle)) : 0.f;

		const uint32 XCoordinate = Pixel + FadeInStartX;
		FadeInCurvePoints[Pixel] = FVector2D(XCoordinate, CurveValue * AllottedGeometry.Size.Y);
	}

	const float FadeOutFrames = GetPropertyValue<float>(EndFadeTimeHandle) * TransformationWaveInfo.SampleRate;
	const float FadeOutPixelLength = FadeOutFrames * PixelsPerFrame;
	FadeOutStartX = FMath::RoundToInt32(FMath::Clamp(EndTimeHandleX - FadeOutPixelLength, StartTimeHandleX, EndTimeHandleX));
	FadeOutEndX = FMath::RoundToInt32(EndTimeHandleX);
	
	const uint32 DisplayedFadeOutPixelLength = FadeOutEndX - FadeOutStartX;
	FadeOutCurvePoints.SetNumUninitialized(DisplayedFadeOutPixelLength);
	const uint32 FadeOutPixelOffset = FadeOutPixelLength - DisplayedFadeOutPixelLength;

	for (uint32 Pixel = 0; Pixel < DisplayedFadeOutPixelLength; ++Pixel)
	{
		const double FadeFraction = (float)(Pixel + FadeOutPixelOffset) / FadeOutPixelLength;
		const double CurveValue = Pixel != FadeOutPixelLength - 1 ? FMath::Pow(FadeFraction, GetPropertyValue<float>(EndFadeCurveHandle)) : 1.f;

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

void FWaveformTransformationTrimFadeRenderer::SetPropertyHandles(const TArray<TSharedRef<IPropertyHandle>>& InPropertyHandles)
{
	FWaveformTransformationRendererBase::SetPropertyHandles(InPropertyHandles);

	StartTimeHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartTime));
	EndTimeHandle  = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndTime));
	StartFadeTimeHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartFadeTime));
	StartFadeCurveHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartFadeCurve));
	EndFadeTimeHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndFadeTime));
	EndFadeCurveHandle = GetPropertyHandle(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndFadeCurve));
}

FReply FWaveformTransformationTrimFadeRenderer::OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FVector2D LocalCursorPosition = GetLocalCursorPosition(MouseEvent, MyGeometry);

	if (IsCursorInFadeInInteractionRange(LocalCursorPosition, MyGeometry))
	{		
		const float FadeCurveDelta = MouseEvent.GetWheelDelta() * MouseWheelStep;
		const float FadeCurveValue = FMath::Clamp(GetPropertyValue<float>(StartFadeCurveHandle) + FadeCurveDelta, 0.f, 10.f);

		EPropertyValueSetFlags::Type PropertyFlags = EPropertyValueSetFlags::InteractiveChange;
		StartFadeCurveHandle->SetValue(FadeCurveValue, PropertyFlags);

		return FReply::Handled();
	}

	if (IsCursorInFadeOutInteractionRange(LocalCursorPosition, MyGeometry))
	{
		const float FadeCurveDelta = MouseEvent.GetWheelDelta() * MouseWheelStep;
		const float FadeCurveValue = FMath::Clamp(GetPropertyValue<float>(EndFadeCurveHandle) + FadeCurveDelta, 0.f, 10.f);
		
		EPropertyValueSetFlags::Type PropertyFlags = EPropertyValueSetFlags::InteractiveChange;
		EndFadeCurveHandle->SetValue(FadeCurveValue, PropertyFlags);
		
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FVector2D FWaveformTransformationTrimFadeRenderer::GetLocalCursorPosition(const FPointerEvent& MouseEvent, const FGeometry& EventGeometry) const
{
	const FVector2D ScreenSpacePosition = MouseEvent.GetScreenSpacePosition();
	return  EventGeometry.AbsoluteToLocal(ScreenSpacePosition);
}

double FWaveformTransformationTrimFadeRenderer::ConvertXRatioToTime(const float InRatio) const
{
	const float NumFrames = TransformationWaveInfo.NumAvilableSamples / TransformationWaveInfo.NumChannels;
	const float FrameSelected = NumFrames * InRatio;
	return FrameSelected / TransformationWaveInfo.SampleRate;
}

void FWaveformTransformationTrimFadeRenderer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{ 
	const float NumFrames = TransformationWaveInfo.NumAvilableSamples / TransformationWaveInfo.NumChannels;
	const double FirstFrame = FMath::Clamp((GetPropertyValue<double>(StartTimeHandle) * TransformationWaveInfo.SampleRate) , 0.f, NumFrames);
	const double EndFrame = FMath::Clamp((GetPropertyValue<double>(EndTimeHandle) * TransformationWaveInfo.SampleRate), FirstFrame, NumFrames);

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
			BeginTransaction(TEXT("PropertyEditor"), FText::Format(NSLOCTEXT("PropertyEditor", "SetNumericPropertyTransaction", "Edit {0}"), GetPropertyEditedByCurrentInteraction()), nullptr);
			return FReply::Handled().CaptureMouse(OwnerWidget.AsShared()).PreventThrottling();
		}

	}

	return FReply::Unhandled();
}


FReply FWaveformTransformationTrimFadeRenderer::OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && TrimFadeInteractionType != ETrimFadeInteractionType::None)
	{
		SetPropertyValueDependingOnInteractionType(MouseEvent, MyGeometry, EPropertyValueSetFlags::InteractiveChange | EPropertyValueSetFlags::NotTransactable);

		return FReply::Handled().CaptureMouse(OwnerWidget.AsShared());
	}

	return FReply::Unhandled();
}

FReply FWaveformTransformationTrimFadeRenderer::OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (TrimFadeInteractionType != ETrimFadeInteractionType::None)
	{
		SetPropertyValueDependingOnInteractionType(MouseEvent, MyGeometry);
		EndTransaction();
		TrimFadeInteractionType = ETrimFadeInteractionType::None;
		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}

void FWaveformTransformationTrimFadeRenderer::SetPropertyValueDependingOnInteractionType(const FPointerEvent& MouseEvent, const FGeometry& WidgetGeometry, const EPropertyValueSetFlags::Type Flags /*= EPropertyValueSetFlags::DefaultFlags*/)
{
	const FVector2D LocalCursorPosition = GetLocalCursorPosition(MouseEvent, WidgetGeometry);
	const float LocalCursorXRatio = FMath::Clamp(LocalCursorPosition.X / WidgetGeometry.GetLocalSize().X, 0.f, 1.f);
	const double SelectedTime = ConvertXRatioToTime(LocalCursorXRatio);

	TSharedPtr<IPropertyHandle> PropertyHandle;

	switch (TrimFadeInteractionType)
	{
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::None:
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingLeftHandle:
		StartTimeHandle->SetValue(SelectedTime, Flags);
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingRightHandle:
		EndTimeHandle->SetValue(SelectedTime, Flags);
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingFadeIn:
		{
			float StartFadeTimeValue = FMath::Clamp(SelectedTime - GetPropertyValue<double>(StartTimeHandle), 0.f, TNumericLimits<float>().Max());
			StartFadeTimeHandle->SetValue(StartFadeTimeValue, Flags);
		}
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingFadeOut:
		{	
			float EndFadeTimeValue = FMath::Clamp(GetPropertyValue<double>(EndTimeHandle) - SelectedTime, 0.f, TNumericLimits<float>().Max());
			EndFadeTimeHandle->SetValue(EndFadeTimeValue, Flags);
		}
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

FText FWaveformTransformationTrimFadeRenderer::GetPropertyEditedByCurrentInteraction() const
{
	FText OutPropertyName = FText::FromName(NAME_None);

	switch (TrimFadeInteractionType)
	{
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::None:
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingLeftHandle:
		OutPropertyName = FText::FromName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartTime));
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingRightHandle:
		OutPropertyName = FText::FromName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndTime));
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingFadeIn:
		OutPropertyName = FText::FromName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartFadeTime));
		break;
	case FWaveformTransformationTrimFadeRenderer::ETrimFadeInteractionType::ScrubbingFadeOut:
		OutPropertyName = FText::FromName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndFadeTime));
		break;
	default:
		break;
	}

	return MoveTemp(OutPropertyName);

}