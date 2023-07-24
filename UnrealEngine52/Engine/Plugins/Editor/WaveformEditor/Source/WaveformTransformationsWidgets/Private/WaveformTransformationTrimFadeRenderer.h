// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WaveformTransformationRendererBase.h"

class FWaveformTransformationTrimFadeRenderer : public FWaveformTransformationRendererBase
{
public:
	FWaveformTransformationTrimFadeRenderer() = default;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnMouseButtonDown(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(SWidget& OwnerWidget, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;

	virtual void SetPropertyHandles(const TArray<TSharedRef<IPropertyHandle>>& InPropertyHandles) override;

private:
	int32 DrawTrimHandles(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	int32 DrawFadeCurves(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void GenerateFadeCurves(const FGeometry& AllottedGeometry);
	void UpdateInteractionRange();

	bool IsCursorInFadeInInteractionRange(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry) const;
	bool IsCursorInFadeOutInteractionRange(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry) const;

	FText GetPropertyEditedByCurrentInteraction() const;
	FVector2D GetLocalCursorPosition(const FPointerEvent& MouseEvent, const FGeometry& EventGeometry) const;
	double ConvertXRatioToTime(const float InRatio) const;

	float StartTimeHandleX = 0.f;
	float EndTimeHandleX = 0.f;
	uint32 FadeInStartX = 0;
	uint32 FadeInEndX = 0;
	uint32 FadeOutStartX = 0;
	uint32 FadeOutEndX = 0;
	TArray<FVector2D> FadeInCurvePoints;
	TArray<FVector2D> FadeOutCurvePoints;

	TRange<float> StartTimeInteractionXRange = TRange<float>::Inclusive(0.f, 0.f);
	TRange<float> EndTimeInteractionXRange = TRange<float>::Inclusive(0.f, 0.f);
	TRange<float> FadeInInteractionXRange = TRange<float>::Inclusive(0.f, 0.f);
	TRange<float> FadeOutInteractionXRange = TRange<float>::Inclusive(0.f, 0.f);

	const float InteractionPixelXDelta = 10;
	const float InteractionRatioYDelta = 0.07f;
	const float MouseWheelStep = 0.25f;

	double PixelsPerFrame = 0.0;

	enum class ETrimFadeInteractionType : uint8
	{
		None = 0,
		ScrubbingLeftHandle,
		ScrubbingRightHandle,
		ScrubbingFadeIn, 
		ScrubbingFadeOut
	} TrimFadeInteractionType = ETrimFadeInteractionType::None;

	ETrimFadeInteractionType GetInteractionTypeFromCursorPosition(const FVector2D& InLocalCursorPosition, const FGeometry& WidgetGeometry) const;
	void SetPropertyValueDependingOnInteractionType(const FPointerEvent& MouseEvent, const FGeometry& WidgetGeometry, const EPropertyValueSetFlags::Type Flags = EPropertyValueSetFlags::DefaultFlags);

	TSharedPtr<IPropertyHandle> StartTimeHandle;
	TSharedPtr<IPropertyHandle> EndTimeHandle;
	TSharedPtr<IPropertyHandle> StartFadeTimeHandle;
	TSharedPtr<IPropertyHandle> StartFadeCurveHandle;
	TSharedPtr<IPropertyHandle> EndFadeTimeHandle;
	TSharedPtr<IPropertyHandle> EndFadeCurveHandle;
};
