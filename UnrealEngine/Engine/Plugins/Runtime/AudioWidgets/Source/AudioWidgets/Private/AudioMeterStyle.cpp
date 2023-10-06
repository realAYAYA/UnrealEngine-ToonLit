// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioMeterStyle.h"
#include "Styling/StyleDefaults.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AudioMeterStyle)

FAudioMeterStyle::FAudioMeterStyle()
	: MeterSize(FVector2D(250.0f, 25.0f))
	, MeterPadding(FVector2D(10.0f, 5.0f))
	, MeterValuePadding(3.0f)
	, PeakValueWidth(2.0f)
	, ValueRangeDb(FVector2D(-60, 10))
	, bShowScale(true)
	, bScaleSide(true)
	, ScaleHashOffset(5.0f)
	, ScaleHashWidth(1.0f)
	, ScaleHashHeight(10.0f)
	, DecibelsPerHash(5)
	, Font(FStyleDefaults::GetFontInfo(5))
{
}

void FAudioMeterStyle::GetResources(TArray< const FSlateBrush* >& OutBrushes) const
{
	OutBrushes.Add(&MeterValueImage);
	OutBrushes.Add(&MeterBackgroundImage);
	OutBrushes.Add(&MeterPeakImage);
}

const FName FAudioMeterStyle::TypeName(TEXT("FAudioMeterStyle"));

const FAudioMeterStyle& FAudioMeterStyle::GetDefault()
{
	static FAudioMeterStyle Default;
	return Default;
}


