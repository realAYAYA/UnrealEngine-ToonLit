// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDensityRemapElement.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"

#include "PCGPoint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGDensityRemapElement)

FPCGElementPtr UPCGDensityRemapSettings::CreateElement() const
{
	return MakeShared<FPCGDensityRemapElement>();
}

bool FPCGDensityRemapElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDensityRemapElement::Execute);

	const UPCGDensityRemapSettings* Settings = Context->GetInputSettings<UPCGDensityRemapSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const float InRangeMin = Settings->InRangeMin;
	const float InRangeMax = Settings->InRangeMax;
	const float OutRangeMin = Settings->OutRangeMin;
	const float OutRangeMax = Settings->OutRangeMax;
	const bool bExcludeValuesOutsideInputRange = Settings->bExcludeValuesOutsideInputRange;

	// used to determine if a density value lies between TrueMin and TrueMax
	const float InRangeTrueMin = FMath::Min(InRangeMin, InRangeMax);
	const float InRangeTrueMax = FMath::Max(InRangeMin, InRangeMax);

	const float InRangeDifference = InRangeMax - InRangeMin;
	const float OutRangeDifference = OutRangeMax - OutRangeMin;

	float Slope;
	float Intercept;

	// When InRange is a point leave the Slope at 0 so that Density = Intercept
	if (InRangeDifference == 0)
	{
		Slope = 0;
		Intercept = (OutRangeMin + OutRangeMax) / 2.f;
	}
	else
	{
		Slope = OutRangeDifference / InRangeDifference;
		Intercept = OutRangeMin;
	}

	ProcessPoints(Context, Inputs, Outputs, [bExcludeValuesOutsideInputRange, InRangeTrueMin, InRangeTrueMax, Slope, InRangeMin, Intercept](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	{
		OutPoint = InPoint;
		const float SourceDensity = InPoint.Density;

		if (!bExcludeValuesOutsideInputRange || (SourceDensity >= InRangeTrueMin && SourceDensity <= InRangeTrueMax))
		{
			const float UnclampedDensity = Slope * (SourceDensity - InRangeMin) + Intercept;
			OutPoint.Density = FMath::Clamp(UnclampedDensity, 0.f, 1.f);
		}

		return true;
	});

	return true;
}
