// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCollapseElement.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"

FPCGElementPtr UPCGCollapseSettings::CreateElement() const
{
	return MakeShared<FPCGCollapseElement>();
}

bool FPCGCollapseElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCollapseElement::Execute);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		if (!Input.Data || Cast<const UPCGSpatialData>(Input.Data) == nullptr)
		{
			continue;
		}

		// Currently we support collapsing to point data only, but at some point in the future that might be different
		Output.Data = Cast<const UPCGSpatialData>(Input.Data)->ToPointData(Context);
	}

	return true;
}