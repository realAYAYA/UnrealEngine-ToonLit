// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationDurationRenderer.h"

FWaveformTransformationDurationRenderer::FWaveformTransformationDurationRenderer(const uint32 InOriginalWaveformNumFrames)
	: OriginalWaveformNumFrames(InOriginalWaveformNumFrames)
{
}

int32 FWaveformTransformationDurationRenderer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const float StartTimeRatio = (float)TransformationWaveInfo.StartFrameOffset / OriginalWaveformNumFrames / TransformationWaveInfo.NumChannels;
	const uint32 EndSample = TransformationWaveInfo.StartFrameOffset + TransformationWaveInfo.NumAvilableSamples;
	const float EndTimeRatio = (float)EndSample / OriginalWaveformNumFrames / TransformationWaveInfo.NumChannels;

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

void FWaveformTransformationDurationRenderer::SetOriginalWaveformFrames(const uint32 NumFrames)
{
	OriginalWaveformNumFrames = NumFrames;
}
