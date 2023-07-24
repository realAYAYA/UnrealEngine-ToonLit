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

class FWaveformTransformationDurationRenderer : public FWaveformTransformationRendererBase
{
public:
	explicit FWaveformTransformationDurationRenderer(const uint32 InOriginalWaveformFrames)
		: OriginalWaveformFrames(InOriginalWaveformFrames)
	{
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const float StartTimeRatio = (float)TransformationWaveInfo.StartFrameOffset / OriginalWaveformFrames / TransformationWaveInfo.NumChannels;
		const uint32 EndSample = TransformationWaveInfo.StartFrameOffset + TransformationWaveInfo.NumAvilableSamples;
		const float EndTimeRatio = (float)EndSample / OriginalWaveformFrames / TransformationWaveInfo.NumChannels;

		const bool bRenderLeftBox = StartTimeRatio >= 0.f;
		const bool bRenderRightBox = EndTimeRatio <= 1.f;

		if (bRenderLeftBox)
		{
			const float RightMarginX = StartTimeRatio * AllottedGeometry.Size.X;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2D(RightMarginX, AllottedGeometry.Size.Y), FSlateLayoutTransform()),
				FAppStyle::GetBrush(WaveformTransformationDurationHiglightParams::BackgroundBrushName),
				ESlateDrawEffect::None,
				WaveformTransformationDurationHiglightParams::BoxColor);

		}

		if (bRenderRightBox)
		{
			const float LeftMarginX = EndTimeRatio * AllottedGeometry.Size.X;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.Size.X, AllottedGeometry.Size.Y), FSlateLayoutTransform(FVector2D(LeftMarginX, 0.f))),
				FAppStyle::GetBrush(WaveformTransformationDurationHiglightParams::BackgroundBrushName),
				ESlateDrawEffect::None,
				WaveformTransformationDurationHiglightParams::BoxColor);

		}

		return LayerId;
	}

	void SetOriginalWaveformFrames(const uint32 NumFrames) { OriginalWaveformFrames = NumFrames; }
private:
	uint32 OriginalWaveformFrames = 0;
};