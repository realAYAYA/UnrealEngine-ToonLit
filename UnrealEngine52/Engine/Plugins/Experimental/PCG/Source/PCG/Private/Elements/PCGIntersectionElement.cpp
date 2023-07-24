// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGIntersectionElement.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGIntersectionElement)

TArray<FPCGPinProperties> UPCGIntersectionSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGIntersectionSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);

	return PinProperties;
}

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

	const EPCGIntersectionDensityFunction DensityFunction = Settings->DensityFunction;
#if WITH_EDITOR
	const bool bKeepZeroDensityPoints = Settings->bKeepZeroDensityPoints;
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
#if WITH_EDITOR
		IntersectionData->bKeepZeroDensityPoints = bKeepZeroDensityPoints;
#endif

		// Update tagged data
		FPCGTaggedData& IntersectionTaggedData = Outputs[IntersectionTaggedDataIndex];
		IntersectionTaggedData.Data = IntersectionData;
		IntersectionTaggedData.Tags.Append(Input.Tags);
	}

	return true;
}
