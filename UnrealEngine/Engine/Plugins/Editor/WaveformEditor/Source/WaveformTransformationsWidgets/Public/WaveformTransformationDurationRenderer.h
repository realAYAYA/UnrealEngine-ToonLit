// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "WaveformTransformationRendererBase.h"


namespace WaveformTransformationDurationHiglightParams
{
	const FLazyName BackgroundBrushName = TEXT("WhiteBrush");
	const FLinearColor BoxColor = FLinearColor(0.f, 0.f, 0.f, 0.7f);
}

class WAVEFORMTRANSFORMATIONSWIDGETS_API FWaveformTransformationDurationRenderer : public FWaveformTransformationRendererBase
{
public:
	explicit FWaveformTransformationDurationRenderer(const uint32 InOriginalWaveformNumFrames);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	void SetOriginalWaveformFrames(const uint32 NumFrames);

private:
	uint32 OriginalWaveformNumFrames = 0;
};