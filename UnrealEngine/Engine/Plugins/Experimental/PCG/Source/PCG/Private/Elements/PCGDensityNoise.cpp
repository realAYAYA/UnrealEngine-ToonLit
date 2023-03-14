// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDensityNoise.h"

#include "PCGHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#include "Math/RandomStream.h"

UPCGDensityNoiseSettings::UPCGDensityNoiseSettings()
{
	bUseSeed = true;
}

FPCGElementPtr UPCGDensityNoiseSettings::CreateElement() const
{
	return MakeShared<FPCGDensityNoiseElement>();
}

bool FPCGDensityNoiseElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDensityNoiseElement::Execute);

	const UPCGDensityNoiseSettings* Settings = Context->GetInputSettings<UPCGDensityNoiseSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGParamData* Params = Context->InputData.GetParams();

	const EPCGDensityNoiseMode DensityMode = PCG_GET_OVERRIDEN_VALUE(Settings, DensityMode, Params);
	const float DensityNoiseMin = PCG_GET_OVERRIDEN_VALUE(Settings, DensityNoiseMin, Params);
	const float DensityNoiseMax = PCG_GET_OVERRIDEN_VALUE(Settings, DensityNoiseMax, Params);
	const bool bInvertSourceDensity = PCG_GET_OVERRIDEN_VALUE(Settings, bInvertSourceDensity, Params);

	// Precompute a seed based on the settings one and the component one
	const int Seed = PCGSettingsHelpers::ComputeSeedWithOverride(Settings, Context->SourceComponent, Params);

	ProcessPoints(Context, Inputs, Outputs, [&](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	{
		OutPoint = InPoint;
		FRandomStream RandomSource(PCGHelpers::ComputeSeed(Seed, OutPoint.Seed));

		const float DensityNoise = RandomSource.FRandRange(DensityNoiseMin, DensityNoiseMax);

		// This inversion was previously calculated as OutPoint.Density *= 1.f - FMath::Abs(OutPoint.Density * 2.f - 1.f);
		const float SourceDensity = bInvertSourceDensity ? (1.0f - OutPoint.Density) : OutPoint.Density;

		float UnclampedDensity = 0;

		if (DensityMode == EPCGDensityNoiseMode::Minimum)
		{
			UnclampedDensity = FMath::Min(SourceDensity, DensityNoise);
		}
		else if (DensityMode == EPCGDensityNoiseMode::Maximum)
		{
			UnclampedDensity = FMath::Max(SourceDensity, DensityNoise);
		}
		else if (DensityMode == EPCGDensityNoiseMode::Add)
		{
			UnclampedDensity = SourceDensity + DensityNoise;
		}
		else if (DensityMode == EPCGDensityNoiseMode::Multiply)
		{
			UnclampedDensity = SourceDensity * DensityNoise;
		}
		else //if (DensityMode == EPCGDensityNoiseMode::Set)
		{
			UnclampedDensity = DensityNoise;
		}

		OutPoint.Density = FMath::Clamp(UnclampedDensity, 0.f, 1.f);

		return true;
	});

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	return true;
}
