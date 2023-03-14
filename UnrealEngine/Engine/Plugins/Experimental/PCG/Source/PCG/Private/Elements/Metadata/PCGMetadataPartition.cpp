// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataPartition.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGSettingsHelpers.h"

FPCGElementPtr UPCGMetadataPartitionSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataPartitionElement>();
}

bool FPCGMetadataPartitionElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataPartitionElement::Execute);
	check(Context);

	const UPCGMetadataPartitionSettings* Settings = Context->GetInputSettings<UPCGMetadataPartitionSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	const FName PartitionAttribute = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGMetadataPartitionSettings, PartitionAttribute), Settings->PartitionAttribute, Params);

	for (const FPCGTaggedData& Input : Inputs)
	{
		const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data);
		check(SpatialInput);

		if (!SpatialInput->ConstMetadata())
		{
			PCGE_LOG(Error, "Input does not have metadata");
			continue;
		}

		const FName LocalPartitionAttribute = ((PartitionAttribute != NAME_None) ? PartitionAttribute : SpatialInput->ConstMetadata()->GetLatestAttributeNameOrNone());

		if (!SpatialInput->ConstMetadata()->HasAttribute(LocalPartitionAttribute))
		{
			PCGE_LOG(Error, "Input does not have the %s attribute", *LocalPartitionAttribute.ToString());
			continue;
		}

		// Currently, we support only to parse points, so we'll do so here
		const UPCGPointData* InputData = SpatialInput->ToPointData(Context);

		if (!InputData)
		{
			PCGE_LOG(Error, "Unable to get points out of spatial data");
			continue;
		}

		const TArray<FPCGPoint>& SourcePoints = InputData->GetPoints();
		const UPCGMetadata* SourceMetadata = InputData->ConstMetadata();
		const FPCGMetadataAttributeBase* AttributeBase = SourceMetadata->GetConstAttribute(LocalPartitionAttribute);
		check(AttributeBase);

		TMap<PCGMetadataValueKey, UPCGPointData*> ValueToData;

		// Get all value keys (-1 + 0 - N)
		int64 MetadataValueKeyCount = AttributeBase->GetValueKeyOffsetForChild();

		// For every value key, check if it should be merged with the default value
		// Keep track of that mapping (should be only one, but that might not be true in uncompressed data)
		TArray<PCGMetadataValueKey> ValueKeyMapping;
		ValueKeyMapping.Reserve(MetadataValueKeyCount);

		for (PCGMetadataValueKey ValueKey = 0; ValueKey < MetadataValueKeyCount; ++ValueKey)
		{
			if (AttributeBase->IsEqualToDefaultValue(ValueKey))
			{
				ValueKeyMapping.Add(-1);
			}
			else
			{
				ValueKeyMapping.Add(ValueKey);
			}
		}

		TArray<UPCGPointData*> PartitionedData;
		PartitionedData.SetNum(1 + ValueKeyMapping.Num());
		for (int32 Index = 0; Index < PartitionedData.Num(); ++Index)
		{
			PartitionedData[Index] = nullptr;
		}

		// Loop on points, find matching data pointer, allocate if null, etc.and add point to it
		for (const FPCGPoint& Point : SourcePoints)
		{
			PCGMetadataValueKey ValueKey = AttributeBase->GetValueKey(Point.MetadataEntry);
			// Remap
			if (ValueKey != PCGDefaultValueKey)
			{
				ValueKey = ValueKeyMapping[ValueKey];
			}

			const int32 PartitionDataIndex = 1 + ValueKey;

			if (!PartitionedData[PartitionDataIndex])
			{
				PartitionedData[PartitionDataIndex] = NewObject<UPCGPointData>();
				PartitionedData[PartitionDataIndex]->InitializeFromData(InputData);
			}

			PartitionedData[PartitionDataIndex]->GetMutablePoints().Add(Point);
		}

		// Finally, push back the partitioned data to the outputs
		for (UPCGPointData* PartitionData : PartitionedData)
		{
			if (PartitionData)
			{
				FPCGTaggedData& Output = Outputs.Emplace_GetRef();
				Output.Data = PartitionData;
				Output.Tags = Input.Tags;
			}
		}
	}

	return true;
}