// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDensityRemap.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGHelpers.h"

#include "Math/RandomStream.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGDensityRemap)

#define LOCTEXT_NAMESPACE "PCGDensityRemapElement"

UPCGLinearDensityRemapSettings::UPCGLinearDensityRemapSettings()
{
	bUseSeed = true;
}

FPCGElementPtr UPCGLinearDensityRemapSettings::CreateElement() const
{
	return MakeShared<FPCGLinearDensityRemapElement>();
}

bool FPCGLinearDensityRemapElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLinearDensityRemapElement::Execute);

	const UPCGLinearDensityRemapSettings* Settings = Context->GetInputSettings<UPCGLinearDensityRemapSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const float SettingsRemapMin = Settings->RemapMin;
	const float SettingsRemapMax = Settings->RemapMax;
	const bool bMultiplyDensity = Settings->bMultiplyDensity;

	const int Seed = Context->GetSeed();

	const float RemapMin = FMath::Min(SettingsRemapMin, SettingsRemapMax);
	const float RemapMax = FMath::Max(SettingsRemapMin, SettingsRemapMax);

	// TODO: embarassingly parallel loop
	for (const FPCGTaggedData& Input : Inputs)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLinearDensityRemapElement::Execute::InputLoop);
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		if (!Input.Data || Cast<UPCGSpatialData>(Input.Data) == nullptr)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Input data missing or not of type Spatial"));
			continue;
		}

		const UPCGPointData* OriginalData = Cast<UPCGSpatialData>(Input.Data)->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoPointsInInput", "Unable to obtain points from input data"));
			continue;
		}

		const TArray<FPCGPoint>& Points = OriginalData->GetPoints();
		const int OriginalPointCount = Points.Num();

		UPCGPointData* SampledData = NewObject<UPCGPointData>();
		SampledData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& SampledPoints = SampledData->GetMutablePoints();
		Output.Data = SampledData;

		if (bMultiplyDensity)
		{
			FPCGAsync::AsyncPointProcessing(Context, OriginalPointCount, SampledPoints, [&Points, Seed, RemapMin, RemapMax](int32 Index, FPCGPoint& OutPoint)
			{
				OutPoint = Points[Index];
				FRandomStream RandomSource(PCGHelpers::ComputeSeed(Seed, OutPoint.Seed));
				OutPoint.Density *= RandomSource.FRandRange(RemapMin, RemapMax);
				return true;
			});
		}
		else
		{
			FPCGAsync::AsyncPointProcessing(Context, OriginalPointCount, SampledPoints, [&Points, Seed, RemapMin, RemapMax](int32 Index, FPCGPoint& OutPoint)
			{
				OutPoint = Points[Index];
				FRandomStream RandomSource(PCGHelpers::ComputeSeed(Seed, OutPoint.Seed));
				OutPoint.Density = RandomSource.FRandRange(RemapMin, RemapMax);
				return true;
			});
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
