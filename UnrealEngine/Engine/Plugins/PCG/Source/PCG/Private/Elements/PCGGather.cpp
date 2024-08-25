// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGGather.h"

#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGGatherElement"

TArray<FPCGPinProperties> UPCGGatherSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);
	InputPinProperty.SetRequiredPin();
	PinProperties.Emplace(PCGPinConstants::DefaultDependencyOnlyLabel, EPCGDataType::Any, /*bAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true, LOCTEXT("DependencyPinTooltip", "Data passed to this pin will be used to order execution but will otherwise not contribute to the results of this node."));

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGGatherSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGGatherSettings::CreateElement() const
{
	return MakeShared<FPCGGatherElement>();
}

namespace PCGGather
{
	FPCGDataCollection GatherDataForPin(const FPCGDataCollection& InputData, const FName InputLabel, const FName OutputLabel)
	{
		TArray<FPCGTaggedData> GatheredData = InputData.GetInputsByPin(InputLabel);
		FPCGDataCollection Output;

		if (GatheredData.IsEmpty())
		{
			return Output;
		}
	
		if (GatheredData.Num() == InputData.TaggedData.Num())
		{
			Output = InputData;
		}
		else
		{
			Output.TaggedData = MoveTemp(GatheredData);
		}

		for(FPCGTaggedData& TaggedData : Output.TaggedData)
		{
			TaggedData.Pin = OutputLabel;
		}

		return Output;
	}
}

bool FPCGGatherElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGGatherElement::Execute);

	Context->OutputData = PCGGather::GatherDataForPin(Context->InputData);

	return true;
}

#undef LOCTEXT_NAMESPACE