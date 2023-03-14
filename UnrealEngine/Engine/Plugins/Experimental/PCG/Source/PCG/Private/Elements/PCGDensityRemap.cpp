// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDensityRemap.h"
#include "PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Math/RandomStream.h"

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
	UPCGParamData* Params = Context->InputData.GetParams();

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	const float SettingsRemapMin = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGLinearDensityRemapSettings, RemapMin), Settings->RemapMin, Params);
	const float SettingsRemapMax = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGLinearDensityRemapSettings, RemapMax), Settings->RemapMax, Params);
	const bool bMultiplyDensity = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGLinearDensityRemapSettings, bMultiplyDensity), Settings->bMultiplyDensity, Params);

	const int Seed = PCGSettingsHelpers::ComputeSeedWithOverride(Settings, Context->SourceComponent, Params);

	const float RemapMin = FMath::Min(SettingsRemapMin, SettingsRemapMax);
	const float RemapMax = FMath::Max(SettingsRemapMin, SettingsRemapMax);

	// TODO: embarassingly parallel loop
	for (const FPCGTaggedData& Input : Inputs)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGLinearDensityRemapElement::Execute::InputLoop);
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		if (!Input.Data || Cast<UPCGSpatialData>(Input.Data) == nullptr)
		{
			PCGE_LOG(Error, "Invalid input data");
			continue;
		}

		const UPCGPointData* OriginalData = Cast<UPCGSpatialData>(Input.Data)->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG(Error, "Unable to get points from input");
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