// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGUnionElement.h"
#include "Helpers/PCGSettingsHelpers.h"

FPCGElementPtr UPCGUnionSettings::CreateElement() const
{
	return MakeShared<FPCGUnionElement>();
}

bool FPCGUnionElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGUnionElement::Execute);

	const UPCGUnionSettings* Settings = Context->GetInputSettings<UPCGUnionSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();

	const EPCGUnionType Type = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGUnionSettings, Type), Settings->Type, Params);
	const EPCGUnionDensityFunction DensityFunction = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGUnionSettings, DensityFunction), Settings->DensityFunction, Params);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const UPCGSpatialData* FirstSpatialData = nullptr;
	UPCGUnionData* UnionData = nullptr;
	int32 UnionTaggedDataIndex = -1;

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
			UnionTaggedDataIndex = Outputs.Num();
			Outputs.Add(Input);
			continue;
		}

		FPCGTaggedData& UnionTaggedData = Outputs[UnionTaggedDataIndex];

		// Create union or add to it
		if (!UnionData)
		{
			UnionData = FirstSpatialData->UnionWith(SpatialData);
			UnionData->SetType(Type);
			UnionData->SetDensityFunction(DensityFunction);

			UnionTaggedData.Data = UnionData;
		}
		else
		{
			UnionData->AddData(SpatialData);
			UnionTaggedData.Tags.Append(Input.Tags);
		}
		
		UnionTaggedData.Data = UnionData;
	}

	// Finally, pass-through settings
	Outputs.Append(Context->InputData.GetAllSettings());

	return true;
}