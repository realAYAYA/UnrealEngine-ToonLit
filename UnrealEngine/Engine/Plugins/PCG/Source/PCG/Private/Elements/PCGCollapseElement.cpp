// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCollapseElement.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCollapseElement)

#if WITH_EDITOR
bool UPCGCollapseSettings::GetCompactNodeIcon(FName& OutCompactNodeIcon) const
{
	OutCompactNodeIcon = PCGNodeConstants::Icons::CompactNodeConvert;
	return true;
}
#endif

TArray<FPCGPinProperties> UPCGCollapseSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial);
	InputPinProperty.SetRequiredPin();

	return PinProperties;
}

FPCGElementPtr UPCGCollapseSettings::CreateElement() const
{
	return MakeShared<FPCGCollapseElement>();
}

TArray<FPCGPinProperties> UPCGConvertToPointDataSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Param);
	InputPinProperty.SetRequiredPin();

	return PinProperties;
}

bool FPCGCollapseElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCollapseElement::Execute);
	check(Context);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		if (!Input.Data)
		{
			continue;
		}

		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		if (const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data))
		{
			// Currently we support collapsing to point data only, but at some point in the future that might be different
			Output.Data = Cast<const UPCGSpatialData>(Input.Data)->ToPointData(Context);
		}
		else if (const UPCGParamData* ParamData = Cast<UPCGParamData>(Input.Data))
		{
			const UPCGMetadata* ParamMetadata = ParamData->Metadata;
			const int64 ParamItemCount = ParamMetadata->GetLocalItemCount();

			if (ParamItemCount == 0)
			{
				continue;
			}

			UPCGPointData* PointData = NewObject<UPCGPointData>();
			check(PointData->Metadata);
			PointData->Metadata->Initialize(ParamMetadata);

			TArray<FPCGPoint>& Points = PointData->GetMutablePoints();
			Points.SetNum(ParamItemCount);

			for (int PointIndex = 0; PointIndex < ParamItemCount; ++PointIndex)
			{
				Points[PointIndex].MetadataEntry = PointIndex;
			}

			Output.Data = PointData;
		}
	}

	return true;
}
