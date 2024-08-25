// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGUnionElement.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGUnionElement)

TArray<FPCGPinProperties> UPCGUnionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial);
	InputPinProperty.SetRequiredPin();

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGUnionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);

	return PinProperties;
}

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

	const EPCGUnionType Type = Settings->Type;
	const EPCGUnionDensityFunction DensityFunction = Settings->DensityFunction;

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

	return true;
}
