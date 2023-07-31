// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGIntersectionElement.h"
#include "Helpers/PCGSettingsHelpers.h"

FPCGElementPtr UPCGIntersectionSettings::CreateElement() const
{
	return MakeShared<FPCGIntersectionElement>();
}

bool FPCGIntersectionElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGIntersectionElement::Execute);

	const UPCGIntersectionSettings* Settings = Context->GetInputSettings<UPCGIntersectionSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();

	const EPCGIntersectionDensityFunction DensityFunction = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGIntersectionSettings, DensityFunction), Settings->DensityFunction, Params);
#if WITH_EDITORONLY_DATA
	const bool bKeepZeroDensityPoints = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGIntersectionSettings, bKeepZeroDensityPoints), Settings->bKeepZeroDensityPoints, Params);
#else
	const bool bKeepZeroDensityPoints = false;
#endif

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const UPCGSpatialData* FirstSpatialData = nullptr;
	UPCGIntersectionData* IntersectionData = nullptr;
	int32 IntersectionTaggedDataIndex = -1;

	for (FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		// Non-spatial data we're not going to touch
		if (!SpatialData)
		{
			Outputs.Add(Input);
			continue;
		}

		if (!FirstSpatialData)
		{
			FirstSpatialData = SpatialData;
			IntersectionTaggedDataIndex = Outputs.Num();
			Outputs.Add(Input);
			continue;
		}

		// Create a new intersection
		IntersectionData = (IntersectionData ? IntersectionData : FirstSpatialData)->IntersectWith(SpatialData);
		// Propagate settings
		IntersectionData->DensityFunction = DensityFunction;
#if WITH_EDITORONLY_DATA
		IntersectionData->bKeepZeroDensityPoints = bKeepZeroDensityPoints;
#endif

		// Update tagged data
		FPCGTaggedData& IntersectionTaggedData = Outputs[IntersectionTaggedDataIndex];
		IntersectionTaggedData.Data = IntersectionData;
		IntersectionTaggedData.Tags.Append(Input.Tags);
	}

	// Pass-through settings & exclusions
	Outputs.Append(Context->InputData.GetAllSettings());

	return true;
}