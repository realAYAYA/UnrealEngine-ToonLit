// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGProjectionElement.h"
#include "Data/PCGProjectionData.h"
#include "Helpers/PCGSettingsHelpers.h"

FPCGElementPtr UPCGProjectionSettings::CreateElement() const
{
	return MakeShared<FPCGProjectionElement>();
}

bool FPCGProjectionElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGProjectionElement::Execute);

	const UPCGProjectionSettings* Settings = Context->GetInputSettings<UPCGProjectionSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

#if WITH_EDITORONLY_DATA
	const bool bKeepZeroDensityPoints = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGProjectionSettings, bKeepZeroDensityPoints), Settings->bKeepZeroDensityPoints, Params);
#else
	const bool bKeepZeroDensityPoints = false;
#endif

	const UPCGSpatialData* FirstSpatialData = nullptr;
	UPCGProjectionData* ProjectionData = nullptr;
	int32 ProjectionTaggedDataIndex = -1;

	// TODO: it might not make sense to perform the projection if the first
	// data isn't a spatial data, otherwise, what would it really mean?
	for (FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

		if (!SpatialData)
		{
			Outputs.Add(Input);
			continue;
		}

		if (!FirstSpatialData)
		{
			FirstSpatialData = SpatialData;
			ProjectionTaggedDataIndex = Outputs.Num();
			Outputs.Add(Input);
			continue;
		}

		// Create a new projection
		ProjectionData = (ProjectionData ? ProjectionData : FirstSpatialData)->ProjectOn(SpatialData);

#if WITH_EDITORONLY_DATA
		ProjectionData->bKeepZeroDensityPoints = bKeepZeroDensityPoints;
#endif
		
		// Update the tagged data
		FPCGTaggedData& ProjectionTaggedData = Outputs[ProjectionTaggedDataIndex];
		ProjectionTaggedData.Data = ProjectionData;
		ProjectionTaggedData.Tags.Append(Input.Tags);
	}

	// Pass-through exclusions/settings
	Outputs.Append(Context->InputData.GetAllSettings());

	return true;
}