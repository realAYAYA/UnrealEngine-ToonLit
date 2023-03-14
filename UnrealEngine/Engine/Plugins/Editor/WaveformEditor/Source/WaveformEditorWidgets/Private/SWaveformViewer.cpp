// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWaveformViewer.h"

#include "WaveformEditorRenderData.h"
#include "WaveformEditorSlateTypes.h"

void SWaveformViewer::Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorRenderData> InRenderData, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator)
{
	RenderData = InRenderData;
	SampleData = RenderData->GetSampleData();

	RenderData->OnRenderDataUpdated.AddSP(this, &SWaveformViewer::OnRenderDataUpdated);

	TransportCoordinator = InTransportCoordinator;
	TransportCoordinator->OnDisplayRangeUpdated.AddSP(this, &SWaveformViewer::OnDisplayRangeUpdated);

	DisplayRange = TRange<float>::Inclusive(0.f, RenderData->GetOriginalWaveformDurationInSeconds());

	Style = InArgs._Style;
	check(Style);
	WaveformColor = Style->WaveformColor;
	MajorGridLineColor = Style->MajorGridLineColor;
	MinorGridLineColor = Style->MinorGridLineColor;
	BackgroundColor = Style->WaveformBackgroundColor;
	BackgroundBrush = Style->BackgroundBrush;
	DesiredWidth = Style->DesiredWidth;
	DesiredHeight = Style->DesiredHeight;
	SampleMarkersSize = Style->SampleMarkersSize;
	WaveformLineThickness = Style->WaveformLineThickness;
	ZeroCrossingLineColor = Style->ZeroCrossingLineColor;
	ZeroCrossingLineThickness = Style->ZeroCrossingLineThickness;
}
void SWaveformViewer::OnRenderDataUpdated()
{
	SampleData = RenderData->GetSampleData();
	bForceRedraw = true;
}

void SWaveformViewer::OnDisplayRangeUpdated(const TRange<float> NewDisplayRange)
{
	const float LengthInSeconds = RenderData->GetOriginalWaveformDurationInSeconds();
	DisplayRange.SetLowerBoundValue(NewDisplayRange.GetLowerBoundValue() * LengthInSeconds);
	DisplayRange.SetUpperBoundValue(NewDisplayRange.GetUpperBoundValue() * LengthInSeconds);
	bForceRedraw = true;
}

int32 SWaveformViewer::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const 
{
	float PixelWidth = MyCullingRect.GetSize().X;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(),
		&BackgroundBrush,
		ESlateDrawEffect::None, 
		BackgroundColor.GetSpecifiedColor()
	);

	if (PixelWidth > 0)
	{
		DrawGridLines(AllottedGeometry, OutDrawElements, LayerId);

		if (WaveformDrawMode == EWaveformDrawMode::BinnedPeaks)
		{
			TArray<FVector2D> BinDrawPoints;
			BinDrawPoints.SetNumUninitialized(2);

			for (const FSamplesBinCoordinates& BinCoordinates : CachedBinsDrawCoordinates)
			{
				BinDrawPoints[0] = BinCoordinates.PointA;
				BinDrawPoints[1] = BinCoordinates.PointB;

				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					BinDrawPoints,
					ESlateDrawEffect::None,
					WaveformColor.GetSpecifiedColor()
				);
			}

		}
		else if (WaveformDrawMode == EWaveformDrawMode::WaveformLine)
		{
			TArray<FVector2D> SamplesDrawCoordinates;
			const uint16 NChannels = RenderData->GetNumChannels();

			for (uint16 Channel = 0; Channel < NChannels; ++Channel)
			{
				for (int32 SampleIndex = Channel; SampleIndex < CachedSampleDrawCoordinates.Num(); SampleIndex += NChannels)
				{
					SamplesDrawCoordinates.Add(CachedSampleDrawCoordinates[SampleIndex]);
					
					const FVector2D SampleBoxSize = FVector2D(SampleMarkersSize, SampleMarkersSize);
					const FVector2D SampleBoxTransform = FVector2D(CachedSampleDrawCoordinates[SampleIndex] - (SampleBoxSize / 2.f));

					const FPaintGeometry SampleBoxGeometry = AllottedGeometry.ToPaintGeometry(
						SampleBoxSize,
						FSlateLayoutTransform(SampleBoxTransform)
					);

					FSlateDrawElement::MakeBox(
						OutDrawElements,
						LayerId,
						SampleBoxGeometry,
						&BackgroundBrush,
						ESlateDrawEffect::None,
						WaveformColor.GetSpecifiedColor()
					);
				}

				FSlateDrawElement::MakeLines(
					OutDrawElements,
					LayerId,
					AllottedGeometry.ToPaintGeometry(),
					SamplesDrawCoordinates,
					ESlateDrawEffect::None,
					WaveformColor.GetSpecifiedColor(),
					true,
					WaveformLineThickness
				);

				SamplesDrawCoordinates.Empty();
			}
		}

		++LayerId;
	}

	return LayerId;
}

FVector2D SWaveformViewer::ComputeDesiredSize(float) const
{
	return FVector2D(DesiredWidth, DesiredHeight);
}

void SWaveformViewer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	uint32 DiscretePixelWidth = FMath::FloorToInt(AllottedGeometry.GetLocalSize().X);
	if (DiscretePixelWidth <= 0)
	{
		return;
	}

	if (DiscretePixelWidth != CachedPixelWidth || bForceRedraw)
	{
		CachedPixelWidth = DiscretePixelWidth;
		CachedPixelHeight = AllottedGeometry.GetLocalSize().Y;
		const float NumSamplesToDisplay = (DisplayRange.GetUpperBoundValue() - DisplayRange.GetLowerBoundValue()) * RenderData->GetSampleRate();
		WaveformDrawMode = NumSamplesToDisplay > CachedPixelWidth ? EWaveformDrawMode::BinnedPeaks : EWaveformDrawMode::WaveformLine;

		if (WaveformDrawMode == EWaveformDrawMode::BinnedPeaks)
		{
			WaveformDrawingUtils::GetBinnedPeaksFromWaveformRawData(CachedPeaks, CachedPixelWidth, SampleData.GetData(), RenderData->GetNumSamples(), RenderData->GetSampleRate(), RenderData->GetNumChannels(), DisplayRange.GetLowerBoundValue(), DisplayRange.GetUpperBoundValue());
			GenerateWaveformBinsCoordinates(CachedBinsDrawCoordinates, CachedPeaks, AllottedGeometry);
		}
		else
		{
			GenerateWaveformSamplesCoordinates(CachedSampleDrawCoordinates, AllottedGeometry);
		}
		
		bForceRedraw = false;
	}
	else if (CachedPixelHeight != AllottedGeometry.GetLocalSize().Y)
	{
		CachedPixelHeight = AllottedGeometry.GetLocalSize().Y;

		if (WaveformDrawMode == EWaveformDrawMode::BinnedPeaks)
		{
			GenerateWaveformBinsCoordinates(CachedBinsDrawCoordinates, CachedPeaks, AllottedGeometry);
		}
		else
		{
			GenerateWaveformSamplesCoordinates(CachedSampleDrawCoordinates, AllottedGeometry);
		}

	}
}

void SWaveformViewer::GenerateWaveformBinsCoordinates(TArray<FSamplesBinCoordinates>& OutDrawCoordinates, const TArray<WaveformDrawingUtils::SampleRange>& InWaveformPeaks, const FGeometry& InAllottedGeometry, const float VerticalZoomFactor /*= 1.f*/)
{
	const int NChannels = RenderData->GetNumChannels();

	if (!ensure(NChannels != 0))
	{
		return;
	}

	const uint32 PixelWidth = FMath::FloorToInt(InAllottedGeometry.GetLocalSize().X);
	check(PixelWidth * NChannels == InWaveformPeaks.Num());

	const float HeightScale = InAllottedGeometry.GetLocalSize().Y / (2.f * TNumericLimits<int16>::Max() * NChannels) * MaxWaveformHeight * VerticalZoomFactor;

	OutDrawCoordinates.SetNumUninitialized(InWaveformPeaks.Num());
	FSamplesBinCoordinates* OutCoordinatesData = OutDrawCoordinates.GetData();

	for (uint16 Channel = 0; Channel < NChannels; ++Channel)
	{
		const FChannelSlotBoundaries ChannelBoundaries(Channel, NChannels, InAllottedGeometry);

		for (uint32 Pixel = 0; Pixel < PixelWidth; ++Pixel)
		{
			uint32 PeakIndex = Pixel * NChannels + Channel;
			const WaveformDrawingUtils::SampleRange& SamplePeaks = InWaveformPeaks[PeakIndex];

			const float SampleMaxScaled = SamplePeaks.GetUpperBoundValue() * HeightScale > MinScaledSamplePeakValue ? SamplePeaks.GetUpperBoundValue() * HeightScale : MinWaveformBinHeight;
			const float SampleMinScaled = SamplePeaks.GetLowerBoundValue() * HeightScale < -1 * MinScaledSamplePeakValue ? SamplePeaks.GetLowerBoundValue() * HeightScale : -1.f * MinWaveformBinHeight;

			const float Top = FMath::Max(ChannelBoundaries.Center - SampleMaxScaled, ChannelBoundaries.Top + MaxDistanceFromChannelBoundaries);
			const float Bottom = FMath::Min(ChannelBoundaries.Center - SampleMinScaled, ChannelBoundaries.Bottom - MaxDistanceFromChannelBoundaries);

			OutCoordinatesData[PeakIndex].PointA = FVector2D(Pixel, Top);
			OutCoordinatesData[PeakIndex].PointB = FVector2D(Pixel, Bottom);
		}
	}
}

void SWaveformViewer::GenerateWaveformSamplesCoordinates(TArray<FVector2D>& OutDrawCoordinates, const FGeometry& InAllottedGeometry, const float VerticalZoomFactor /*= 1.f*/)
{
	
	OutDrawCoordinates.Empty();
	const uint16 NChannels = RenderData->GetNumChannels();

	if (!ensure(NChannels != 0))
	{
		return;
	}

	const uint32 TotalWaveformFrames = RenderData->GetNumSamples() / NChannels;
	double NumFramesToDisplay = (DisplayRange.GetUpperBoundValue() - DisplayRange.GetLowerBoundValue()) * RenderData->GetSampleRate();
	double FramesPixelDistance = InAllottedGeometry.Size.X / NumFramesToDisplay;
	
	TArray<FChannelSlotBoundaries> ChannelSlotsBoundaries;

	for (uint16 Channel = 0; Channel < NChannels; ++Channel)
	{
		ChannelSlotsBoundaries.Emplace(Channel, NChannels, InAllottedGeometry);
	}

	for (double FrameX = GridMetrics.FirstMajorTickX; FrameX < InAllottedGeometry.Size.X + FramesPixelDistance; FrameX += FramesPixelDistance)
	{
		const double XRatio = FrameX / InAllottedGeometry.Size.X;
		const double FrameTime = DisplayRange.GetLowerBoundValue() + DisplayRange.Size<double>() * XRatio;
		const uint32 FrameIndex = FMath::RoundToInt(FrameTime * RenderData->GetSampleRate());
		

		if (FrameIndex >= 0 && FrameIndex < TotalWaveformFrames)
		{
			for (uint16 Channel = 0; Channel < NChannels; ++Channel)
			{
				const FChannelSlotBoundaries& ChannelBoundaries = ChannelSlotsBoundaries[Channel];
				const uint32 SampleIndex = FrameIndex * NChannels + Channel;
				const int16 SampleValue = RenderData->GetSampleData()[SampleIndex];

				const float SampleValueRatio = (float)SampleValue / TNumericLimits<int16>::Max() * MaxWaveformHeight * VerticalZoomFactor;
				const float TopBoundary = ChannelBoundaries.Top + MaxDistanceFromChannelBoundaries;
				const float BottomBoundary = ChannelBoundaries.Bottom - MaxDistanceFromChannelBoundaries;
				const float SampleY = FMath::Clamp((SampleValueRatio * ChannelBoundaries.Height / 2.f) + ChannelBoundaries.Center, TopBoundary, BottomBoundary);

				const FVector2D SampleCoordinates(FrameX, SampleY);

				OutDrawCoordinates.Add(SampleCoordinates);
			}
		}
	}
}

void SWaveformViewer::DrawGridLines(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	const double MinorGridXStep = GridMetrics.MajorGridXStep / GridMetrics.NumMinorGridDivisions;

	for (double CurrentMajorLineX = GridMetrics.FirstMajorTickX; CurrentMajorLineX < AllottedGeometry.Size.X; CurrentMajorLineX += GridMetrics.MajorGridXStep)
	{
		const double MajorLineX = CurrentMajorLineX;

		LinePoints[0] = FVector2D(MajorLineX, 0.f);
		LinePoints[1] = FVector2D(MajorLineX, AllottedGeometry.Size.Y);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			MajorGridLineColor.GetSpecifiedColor(),
			false);


		for (int32 MinorLineIndex = 1; MinorLineIndex < GridMetrics.NumMinorGridDivisions; ++MinorLineIndex)
		{
			const double MinorLineX = MajorLineX + MinorGridXStep * MinorLineIndex;

			LinePoints[0] = FVector2D(MinorLineX, 0.);
			LinePoints[1] = FVector2D(MinorLineX, AllottedGeometry.Size.Y);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				++LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				MinorGridLineColor.GetSpecifiedColor(),
				false);

		}
	}

	const uint16 NChannels = RenderData->GetNumChannels();

	for (uint16 Channel = 0; Channel < NChannels; ++Channel)
	{
		const FChannelSlotBoundaries ChannelBoundaries(Channel, NChannels, AllottedGeometry);

		LinePoints[0] = FVector2D(0.f, ChannelBoundaries.Center);
		LinePoints[1] = FVector2D(AllottedGeometry.Size.X, ChannelBoundaries.Center);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			ZeroCrossingLineColor.GetSpecifiedColor(),
			false,
			ZeroCrossingLineThickness);

	}


}

void SWaveformViewer::UpdateGridMetrics(const FWaveEditorGridMetrics& InMetrics)
{
	GridMetrics = InMetrics;
}

void SWaveformViewer::OnStyleUpdated(const FWaveformEditorWidgetStyleBase* UpdatedStyle)
{
	check(UpdatedStyle);
	check(Style);

	if (UpdatedStyle != Style)
	{
		return;
	}

	WaveformColor = Style->WaveformColor;
	MajorGridLineColor = Style->MajorGridLineColor;
	MinorGridLineColor = Style->MinorGridLineColor;
	ZeroCrossingLineColor = Style->ZeroCrossingLineColor;
	ZeroCrossingLineThickness = Style->ZeroCrossingLineThickness;
	BackgroundColor = Style->WaveformBackgroundColor;
	BackgroundBrush = Style->BackgroundBrush;
	SampleMarkersSize = Style->SampleMarkersSize;
	WaveformLineThickness = Style->WaveformLineThickness;
	DesiredWidth = Style->DesiredWidth;
	DesiredHeight = Style->DesiredHeight;
}
