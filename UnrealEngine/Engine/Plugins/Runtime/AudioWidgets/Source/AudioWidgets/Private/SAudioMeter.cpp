// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAudioMeter.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"
#include "DSP/Dsp.h"
#include "Fonts/FontMeasure.h"

SAudioMeter::SAudioMeter()
{
}

void SAudioMeter::Construct(const SAudioMeter::FArguments& InArgs)
{
	check(InArgs._Style);

	Orientation = InArgs._Orientation;

	BackgroundColor = InArgs._BackgroundColor;
	MeterBackgroundColor = InArgs._MeterBackgroundColor;
	MeterValueColor = InArgs._MeterValueColor;
	MeterPeakColor = InArgs._MeterPeakColor;
	MeterScaleColor = InArgs._MeterScaleColor;
	MeterScaleLabelColor = InArgs._MeterScaleLabelColor;
	MeterClippingColor = InArgs._MeterClippingColor;

	Style = InArgs._Style;

	MeterChannelInfoAttribute = InArgs._MeterChannelInfo;
}

int32 SAudioMeter::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// we draw the slider like a horizontal slider regardless of the orientation, and apply a render transform to make it display correctly.
	// However, the AllottedGeometry is computed as it will be rendered, so we have to use the "horizontal orientation" when doing drawing computations.
	const float AllottedWidth = Orientation == Orient_Horizontal ? AllottedGeometry.GetLocalSize().X : AllottedGeometry.GetLocalSize().Y;
	const float AllottedHeight = Orientation == Orient_Horizontal ? AllottedGeometry.GetLocalSize().Y : AllottedGeometry.GetLocalSize().X;

	FGeometry MeterGeometry = AllottedGeometry;

	// rotate the slider 90deg if it's vertical. The 0 side goes on the bottom, the 1 side on the top.
	if (Orientation == Orient_Vertical)
	{
		// Do this by translating along -X by the width of the geometry, then rotating 90 degrees CCW (left-hand coords)
		FSlateRenderTransform SlateRenderTransform = TransformCast<FSlateRenderTransform>(Concatenate(Inverse(FVector2D(AllottedWidth, 0)), FQuat2D(FMath::DegreesToRadians(-90.0f))));
		// create a child geometry matching this one, but with the render transform.
		MeterGeometry = AllottedGeometry.MakeChild(
			FVector2D(AllottedWidth, AllottedHeight),
			FSlateLayoutTransform(),
			SlateRenderTransform, FVector2D::ZeroVector);
	}
 
 	// We clamp to make sure that the slider cannot go out of the slider Length.
 	int32 DecibelsPerHash = Style->DecibelsPerHash;
 
 	int32 MinValueDb = FMath::Min((int32)Style->ValueRangeDb.X, (int32)Style->ValueRangeDb.Y);
 	int32 MaxValueDb = FMath::Max((int32)Style->ValueRangeDb.Y, (int32)Style->ValueRangeDb.Y);
 
 	// Snap the min/max values to the nearest hash 
 	MaxValueDb -= MaxValueDb % DecibelsPerHash;
 	MinValueDb -= MinValueDb % DecibelsPerHash;
 
 	float ValueDeltaDb = MaxValueDb - MinValueDb;

	// Draw the background image
	{
		FVector2D BackgroundImageTopLeft = FVector2D(0.0f, 0.0f);
		FVector2D BackgroundImageSize = FVector2D(AllottedWidth, AllottedHeight);

		// Draw background image
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			MeterGeometry.ToPaintGeometry(BackgroundImageTopLeft, BackgroundImageSize),
			&Style->MeterBackgroundImage,
			ESlateDrawEffect::None,
			Style->BackgroundImage.GetTint(InWidgetStyle) * BackgroundColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);

	}
 	++LayerId;

	float ScaleOffset = 0.0f;
	if (Style->bShowScale && Style->bScaleSide)
	{
		 ScaleOffset = GetScaleHeight();
	}

	TArray<FMeterChannelInfo> ChannelInfos = MeterChannelInfoAttribute.Get();
	int32 NumChannels = ChannelInfos.Num();

	// Draw the meter backgrounds
	for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
	{		
		FVector2D MeterBackgroundTopLeft = FVector2D(Style->MeterPadding.X, Style->MeterPadding.Y + ScaleOffset + ChannelIndex * (Style->MeterSize.Y + Style->MeterPadding.Y));
		FVector2D MeterBackgroundSize = FVector2D(AllottedWidth - 2.0f * Style->MeterPadding.X, Style->MeterSize.Y);

 		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			MeterGeometry.ToPaintGeometry(MeterBackgroundTopLeft, MeterBackgroundSize),
			&Style->MeterBackgroundImage,
			ESlateDrawEffect::None,
			Style->MeterBackgroundImage.GetTint(InWidgetStyle) * MeterBackgroundColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);
	}

	++LayerId;

	// Draw the meter value
	for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
	{
		float ChannelMeterValueDb = ChannelInfos[ChannelIndex].MeterValue;
		float ChannelMeterValue = FMath::Clamp((ChannelMeterValueDb - MinValueDb) / ValueDeltaDb, 0.0f, 1.0f);

		FVector2D MeterValueTopLeft = FVector2D(Style->MeterPadding.X, 
												Style->MeterPadding.Y + ScaleOffset + Style->MeterValuePadding + ChannelIndex * (Style->MeterSize.Y + Style->MeterPadding.Y));
		
		FVector2D MeterValueSize = FVector2D(ChannelMeterValue * (AllottedWidth - 2.0f * Style->MeterPadding.X), 
										     Style->MeterSize.Y - 2.0f * Style->MeterValuePadding);
 
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			MeterGeometry.ToPaintGeometry(MeterValueTopLeft, MeterValueSize),
			&Style->MeterValueImage,
			ESlateDrawEffect::None,
			Style->MeterValueImage.GetTint(InWidgetStyle) * MeterValueColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);
	}

	++LayerId;

	// Draw the meter peak value
	for (int32 ChannelIndex = 0; ChannelIndex < NumChannels; ++ChannelIndex)
	{	
		float ChannelPeakValueDb = ChannelInfos[ChannelIndex].PeakValue;
		float ChannelPeakValue = FMath::Clamp((ChannelPeakValueDb - MinValueDb) / ValueDeltaDb, 0.0f, 1.0f);

		float PeakPixel = ChannelPeakValue * (AllottedWidth - 2.0f * Style->MeterPadding.X);

		FVector2D MeterPeakValueTopLeft = FVector2D(Style->MeterPadding.X + PeakPixel - 0.5f * Style->PeakValueWidth, 
													Style->MeterPadding.Y + ScaleOffset + Style->MeterValuePadding + ChannelIndex * (Style->MeterSize.Y + Style->MeterPadding.Y));
		
		FVector2D MeterPeakValueSize = FVector2D(Style->PeakValueWidth, 
												 Style->MeterSize.Y - 2.0f * Style->MeterValuePadding);

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			MeterGeometry.ToPaintGeometry(MeterPeakValueTopLeft, MeterPeakValueSize),
			&Style->MeterPeakImage,
			ESlateDrawEffect::None,
			Style->MeterPeakImage.GetTint(InWidgetStyle) * MeterPeakColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);
	}
	
	++LayerId;

	// Draw the scale hash
	if (Style->bShowScale)
	{

		float ScaleHashHalfWidth = 0.5f * Style->ScaleHashWidth;
		FVector2D HashSize = FVector2D(Style->ScaleHashWidth, Style->ScaleHashHeight);

		const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		// Measure the min value label size so we can right-justify if needed
		FVector2D MinValueLabelSize = FontMeasureService->Measure(FString::FromInt(MinValueDb), Style->Font);

		// Get the size of the negative sign to use to offset the label text in horizontal mode
		FVector2D NegativeSignSize;
		if (Orientation == Orient_Horizontal)
		{
			NegativeSignSize = FontMeasureService->Measure(TEXT("-"), Style->Font);
		}

		int32 ValueDelta = MaxValueDb - MinValueDb;
		int32 CurrentHashValue = MaxValueDb;
		while (CurrentHashValue >= MinValueDb)
		{
			// Get the fractional value for hash mark
			const float CurrentHashMeterValuePercent = FMath::Clamp(((float)CurrentHashValue - (float)MinValueDb) / ValueDelta, 0.0f, 1.0f);

			// Draw hash
			float HashPixelCenter = CurrentHashMeterValuePercent * (AllottedWidth - 2.0f * Style->MeterPadding.X);

			FVector2D HashTopLeft;
			HashTopLeft.X = Style->MeterPadding.X + HashPixelCenter - ScaleHashHalfWidth;
			if (Style->bScaleSide)
			{
				HashTopLeft.Y = Style->MeterPadding.Y + ScaleOffset - Style->ScaleHashOffset - Style->ScaleHashHeight;
			}
			else
			{
				HashTopLeft.Y = (Style->MeterSize.Y + Style->MeterPadding.Y ) * NumChannels + Style->ScaleHashOffset;
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				MeterGeometry.ToPaintGeometry(HashTopLeft, HashSize),
				&Style->MeterPeakImage,
				ESlateDrawEffect::None,
				MeterScaleColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
			);
		
			FString LabelString = FString::FromInt(CurrentHashValue);

			bool bIsNegative = (CurrentHashValue < 0);

			FVector2D LabelSize; 
			if (Orientation == Orient_Horizontal && bIsNegative)
			{
				// We want to center the text on just the positive portion of the number
				FString LabelStringPositive = FString::FromInt(FMath::Abs(CurrentHashValue));
				LabelSize = FontMeasureService->Measure(LabelStringPositive, Style->Font);
			}
			else
			{
				LabelSize = FontMeasureService->Measure(LabelString, Style->Font);
			}
			
			FText LabelText = FText::FromString(LabelString);

			FGeometry TextGeometry = MeterGeometry;
			if (Orientation == Orient_Vertical)
			{
				FVector2D LabelTopLeft;
				
				LabelTopLeft.X = Style->MeterPadding.X + HashPixelCenter + 0.5f * LabelSize.Y;

				if (Style->bScaleSide)
				{
					LabelTopLeft.Y = Style->MeterPadding.Y - 2.0f + (MinValueLabelSize.X - LabelSize.X);
				}
				else
				{
					LabelTopLeft.Y = (Style->MeterSize.Y + Style->MeterPadding.Y) * NumChannels + Style->ScaleHashOffset + Style->ScaleHashHeight + 2.0f;
				}
				
				// Undo the rotation for vertical right before we do the rendering of the scale value 
				FSlateRenderTransform RotationTransform = FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(90.0f)));

				FSlateLayoutTransform ChildLayoutTransform(1.0f, TransformPoint(1.0f, LabelTopLeft));

				TextGeometry = MeterGeometry.MakeChild(
					LabelSize,
					ChildLayoutTransform,
					RotationTransform,
					FVector2D(0.0f, 0.0f)
				);

				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId,
					TextGeometry.ToOffsetPaintGeometry(FVector2D(0.0f, 0.0f)),
					LabelText,
					Style->Font,
					ESlateDrawEffect::None,
					MeterScaleLabelColor.Get().GetColor(InWidgetStyle)* InWidgetStyle.GetColorAndOpacityTint()
				);
			}
			else
			{
				FVector2D LabelTopLeft;

				// Center it based off the positive value width, then subtract from there the size of the negative sign
				LabelTopLeft.X = Style->MeterPadding.X + HashPixelCenter - 0.5f * LabelSize.X;
				if (bIsNegative)
				{
					LabelTopLeft.X -= NegativeSignSize.X;
				}

				if (Style->bScaleSide)
				{
					LabelTopLeft.Y = Style->MeterPadding.Y + ScaleOffset - LabelSize.Y - Style->ScaleHashOffset - Style->ScaleHashHeight;
				}
				else
				{
					LabelTopLeft.Y = (Style->MeterSize.Y + Style->MeterPadding.Y) * NumChannels + Style->ScaleHashOffset + Style->ScaleHashHeight + 2.0f;
				}

				// Draw text label
				FSlateDrawElement::MakeText(
					OutDrawElements,
					LayerId,
					TextGeometry.ToOffsetPaintGeometry(LabelTopLeft),
					LabelText,
					Style->Font,
					ESlateDrawEffect::None,
					MeterScaleLabelColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
				);

			}

			CurrentHashValue -= Style->DecibelsPerHash;
		}
	}

	return LayerId;
}

float SAudioMeter::GetScaleHeight() const
{
	const TSharedRef<FSlateFontMeasure> FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FVector2D LabelSize = FontMeasureService->Measure(FString::FromInt(-60), Style->Font);

	float ScaleHeight = Style->ScaleHashHeight + Style->ScaleHashOffset;
	if (Orientation == Orient_Vertical)
	{
		return ScaleHeight + LabelSize.X;
	}
	return ScaleHeight + LabelSize.Y;
}

FVector2D SAudioMeter::ComputeDesiredSize(float) const
{
	static const FVector2D SAudioMeterDesiredSize(50.0f, 50.0f);

	if (Style == nullptr)
	{
		return SAudioMeterDesiredSize;
	}

	TArray<FMeterChannelInfo> ChannelInfo = MeterChannelInfoAttribute.Get();
	int32 NumChannels = FMath::Max(ChannelInfo.Num(), 1);

	FVector2D Size = FVector2D(Style->MeterSize.X, (Style->MeterSize.Y + Style->MeterPadding.Y) * NumChannels);

	// Add the end padding
	Size.X += Style->MeterPadding.X;
	Size.Y += Style->MeterPadding.Y;

	// Add the width for the scale if it's been set to show
	if (Style->bShowScale)
	{
		Size.Y += GetScaleHeight();
	}

	// The thickness is going to be the thickness plus the scale image width
	if (Orientation == Orient_Vertical)
	{
		return FVector2D(Size.Y, Size.X);
	}

	return Size; 
}

bool SAudioMeter::ComputeVolatility() const
{
	return true; 
}

void SAudioMeter::SetMeterChannelInfo(const TAttribute<TArray<FMeterChannelInfo>>& InMeterChannelInfo)
{
	SetAttribute(MeterChannelInfoAttribute, InMeterChannelInfo, EInvalidateWidgetReason::Paint);
}

TArray<FMeterChannelInfo> SAudioMeter::GetMeterChannelInfo() const
{
	return MeterChannelInfoAttribute.Get();
}

void SAudioMeter::SetOrientation(EOrientation InOrientation)
{
	if (Orientation != InOrientation)
	{
		Orientation = InOrientation;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SAudioMeter::SetBackgroundColor(FSlateColor InBackgroundColor)
{
	SetAttribute(BackgroundColor, TAttribute<FSlateColor>(InBackgroundColor), EInvalidateWidgetReason::Paint);
}

void SAudioMeter::SetMeterBackgroundColor(FSlateColor InMeterBackgroundColor)
{
	SetAttribute(MeterBackgroundColor, TAttribute<FSlateColor>(InMeterBackgroundColor), EInvalidateWidgetReason::Paint);
}

void SAudioMeter::SetMeterValueColor(FSlateColor InMeterValueColor)
{
	SetAttribute(MeterValueColor, TAttribute<FSlateColor>(InMeterValueColor), EInvalidateWidgetReason::Paint);
}

void SAudioMeter::SetMeterPeakColor(FSlateColor InMeterPeakColor)
{
	SetAttribute(MeterPeakColor, TAttribute<FSlateColor>(InMeterPeakColor), EInvalidateWidgetReason::Paint);
}

void SAudioMeter::SetMeterClippingColor(FSlateColor InMeterClippingColor)
{
	SetAttribute(MeterClippingColor, TAttribute<FSlateColor>(InMeterClippingColor), EInvalidateWidgetReason::Paint);
}

void SAudioMeter::SetMeterScaleColor(FSlateColor InMeterScaleColor)
{
	SetAttribute(MeterScaleColor, TAttribute<FSlateColor>(InMeterScaleColor), EInvalidateWidgetReason::Paint);
}

void SAudioMeter::SetMeterScaleLabelColor(FSlateColor InMeterScaleLabelColor)
{
	SetAttribute(MeterScaleLabelColor, TAttribute<FSlateColor>(InMeterScaleLabelColor), EInvalidateWidgetReason::Paint);
}
