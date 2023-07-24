// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGMergeElement.h"

#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMergeElement)

#define LOCTEXT_NAMESPACE "PCGMergeElement"

#if WITH_EDITOR
FText UPCGMergeSettings::GetNodeTooltipText() const
{
	return LOCTEXT("MergeNodeTooltip", "Merges multiple data sources into a single data output.");
}
#endif

TArray<FPCGPinProperties> UPCGMergeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, /*bAllowMultipleConnections=*/true);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMergeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point, /*bAllowMultipleConnections=*/false);

	return PinProperties;
}

FPCGElementPtr UPCGMergeSettings::CreateElement() const
{
	return MakeShared<FPCGMergeElement>();
}

bool FPCGMergeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMergeElement::Execute);
	check(Context);

	const UPCGMergeSettings* Settings = Context->GetInputSettings<UPCGMergeSettings>();
	check(Settings);

	const bool bMergeMetadata = Settings->bMergeMetadata;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	if (Sources.IsEmpty())
	{
		return true;
	}

	UPCGPointData* TargetPointData = nullptr;
	FPCGTaggedData* TargetTaggedData = nullptr;

	// Prepare data & metadata
	// Done in two passes for futureproofing - expecting changes in the metadata attribute creation vs. usage in points
	for (const FPCGTaggedData& Source : Sources)
	{
		const UPCGPointData* SourcePointData = Cast<const UPCGPointData>(Source.Data);

		if (!SourcePointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnsupportedDataType", "Unsupported data type in merge"));
			continue;
		}

		if (!TargetPointData)
		{
			TargetPointData = NewObject<UPCGPointData>();
			TargetPointData->InitializeFromData(SourcePointData, nullptr, bMergeMetadata);

			TargetTaggedData = &(Outputs.Emplace_GetRef(Source));
			TargetTaggedData->Data = TargetPointData;
		}
		else
		{
			if (bMergeMetadata)
			{
				TargetPointData->Metadata->AddAttributes(SourcePointData->Metadata);
			}
			
			check(TargetTaggedData);
			TargetTaggedData->Tags.Append(Source.Tags); // TODO: only unique? if yes, fix union too
		}
	}

	// No valid input types
	if (!TargetPointData)
	{
		return true;
	}

	TArray<FPCGPoint>& TargetPoints = TargetPointData->GetMutablePoints();
	
	for(int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
	{
		const UPCGPointData* SourcePointData = Cast<const UPCGPointData>(Sources[SourceIndex].Data);

		if (!SourcePointData)
		{
			continue;
		}

		int32 PointOffset = TargetPoints.Num();
		TargetPoints.Append(SourcePointData->GetPoints());

		if ((!bMergeMetadata || SourceIndex != 0) && !SourcePointData->GetPoints().IsEmpty())
		{
			TArrayView<FPCGPoint> TargetPointsSubset = MakeArrayView(&TargetPoints[PointOffset], SourcePointData->GetPoints().Num());
			for (FPCGPoint& Point : TargetPointsSubset)
			{
				Point.MetadataEntry = PCGInvalidEntryKey;
			}

			if (bMergeMetadata && TargetPointData->Metadata && SourcePointData->Metadata && SourcePointData->Metadata->GetAttributeCount() > 0)
			{
				TargetPointData->Metadata->SetPointAttributes(MakeArrayView(SourcePointData->GetPoints()), SourcePointData->Metadata, TargetPointsSubset);
			}
		}
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
