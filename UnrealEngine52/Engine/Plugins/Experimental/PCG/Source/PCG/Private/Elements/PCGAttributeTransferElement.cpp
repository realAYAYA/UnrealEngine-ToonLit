// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAttributeTransferElement.h"

#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGAttributeTransferElement)

#define LOCTEXT_NAMESPACE "PCGAttributeTransferElement"

#if WITH_EDITOR
FName UPCGAttributeTransferSettings::GetDefaultNodeName() const
{
	return PCGAttributeTransferConstants::NodeName;
}

FText UPCGAttributeTransferSettings::GetDefaultNodeTitle() const
{
	return PCGAttributeTransferConstants::NodeTitle;
}
#endif

FName UPCGAttributeTransferSettings::AdditionalTaskName() const
{
	if (SourceAttributeName != TargetAttributeName && TargetAttributeName != NAME_None)
	{
		return FName(FString::Printf(TEXT("%s %s to %s"), *PCGAttributeTransferConstants::NodeName.ToString(), *SourceAttributeName.ToString(), *TargetAttributeName.ToString()));
	}
	else
	{
		return FName(FString::Printf(TEXT("%s %s"), *PCGAttributeTransferConstants::NodeName.ToString(), *SourceAttributeName.ToString()));
	}
}

TArray<FPCGPinProperties> UPCGAttributeTransferSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGAttributeTransferConstants::TargetLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/ false);
	PinProperties.Emplace(PCGAttributeTransferConstants::SourceLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/ false);

	return PinProperties;
}

FPCGElementPtr UPCGAttributeTransferSettings::CreateElement() const
{
	return MakeShared<FPCGAttributeTransferElement>();
}

bool FPCGAttributeTransferElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAttributeTransferElement::Execute);

	check(Context);

	const UPCGAttributeTransferSettings* Settings = Context->GetInputSettings<UPCGAttributeTransferSettings>();
	check(Settings);

	TArray<FPCGTaggedData> SourceInputs = Context->InputData.GetInputsByPin(PCGAttributeTransferConstants::SourceLabel);
	TArray<FPCGTaggedData> TargetInputs = Context->InputData.GetInputsByPin(PCGAttributeTransferConstants::TargetLabel);

	if (SourceInputs.Num() != 1 || TargetInputs.Num() != 1)
	{
		PCGE_LOG(Warning, LogOnly, LOCTEXT("WrongNumberOfInputs", "Source input contains {0} data elements, Target inputs contain {1} data elements, but both should contain precisely 1 data element"));
		return true;
	}

	const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(SourceInputs[0].Data);
	const UPCGSpatialData* TargetData = Cast<UPCGSpatialData>(TargetInputs[0].Data);

	if (!SourceData || !TargetData || SourceData->IsA<UPCGPointData>() != TargetData->IsA<UPCGPointData>())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnsupportedTypes", "Only support Spatial to Spatial data or Point to Point data"));
		return true;
	}

	if (!SourceData->Metadata)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("SourceMissingMetadata", "Source does not have metadata"));
		return true;
	}

	if (!SourceData->Metadata->HasAttribute(Settings->SourceAttributeName))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("SourceMissingAttribute", "Source does not have the attribute '{0}'"), FText::FromName(Settings->SourceAttributeName)));
		return true;
	}

	UPCGData* OutputData = nullptr;

	// We only supports spatial to spatial or point to points, with the same number of points
	if (SourceData->IsA<UPCGPointData>() && TargetData->IsA<UPCGPointData>())
	{
		OutputData = TransferPointToPoint(Context, CastChecked<UPCGPointData>(SourceData), CastChecked<UPCGPointData>(TargetData));
	}
	else
	{
		OutputData = TransferSpatialToSpatial(Context, SourceData, TargetData);
	}

	if (OutputData)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = OutputData;
	}

	return true;
}

UPCGSpatialData* FPCGAttributeTransferElement::TransferSpatialToSpatial(FPCGContext* Context, const UPCGSpatialData* SourceData, const UPCGSpatialData* TargetData) const
{
	check(Context && SourceData && TargetData);

	const UPCGAttributeTransferSettings* Settings = Context->GetInputSettings<UPCGAttributeTransferSettings>();
	check(Settings);

	// When transferring attribute from spatial to spatial, copy the attribute entries and values.
	// They need to match their number of entries
	if (SourceData->Metadata->GetItemCountForChild() != TargetData->Metadata->GetItemCountForChild())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MismatchingEntryCounts", "Source and target do not have the same number of metadata entries"));
		return nullptr;
	}

	UPCGSpatialData* NewSpatialData = TargetData->DuplicateData();

	const FName TargetAttributeName = (Settings->TargetAttributeName == NAME_None) ? Settings->SourceAttributeName : Settings->TargetAttributeName;

	// Making sure the target attribute doesn't exists in the target
	if (NewSpatialData->Metadata->HasAttribute(TargetAttributeName))
	{
		NewSpatialData->Metadata->DeleteAttribute(TargetAttributeName);
	}

	NewSpatialData->Metadata->CopyAttribute(SourceData->Metadata, Settings->SourceAttributeName, TargetAttributeName);

	if (!NewSpatialData->Metadata->HasAttribute(TargetAttributeName))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingAttribute", "Error while creating target attribute '{0}'"), FText::FromName(TargetAttributeName)));
		return nullptr;
	}

	return NewSpatialData;
}

UPCGPointData* FPCGAttributeTransferElement::TransferPointToPoint(FPCGContext* Context, const UPCGPointData* SourceData, const UPCGPointData* TargetData) const
{
	check(Context && SourceData && TargetData);

	const UPCGAttributeTransferSettings* Settings = Context->GetInputSettings<UPCGAttributeTransferSettings>();
	check(Settings);

	// When transferring attribute point to point, iterate over the source point and set the attribute to the target point.
	// They need to match their number of points
	if (SourceData->GetPoints().Num() != TargetData->GetPoints().Num())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MismatchingPointCounts", "Source and target do not have the same number of points"));
		return nullptr;
	}

	UPCGPointData* NewPointData = Cast<UPCGPointData>(TargetData->DuplicateData());

	const TArray<FPCGPoint>& SourcePoints = SourceData->GetPoints();
	TArray<FPCGPoint>& TargetPoints = NewPointData->GetMutablePoints();

	const FName TargetAttributeName = (Settings->TargetAttributeName == NAME_None) ? Settings->SourceAttributeName : Settings->TargetAttributeName;

	// Making sure the target attribute doesn't exist in the target
	if (NewPointData->Metadata->HasAttribute(TargetAttributeName))
	{
		NewPointData->Metadata->DeleteAttribute(TargetAttributeName);
	}

	const FPCGMetadataAttributeBase* SourceAttribute = SourceData->Metadata->GetConstAttribute(Settings->SourceAttributeName);
	FPCGMetadataAttributeBase* TargetAttribute = NewPointData->Metadata->CopyAttribute(SourceAttribute, TargetAttributeName, /*bKeepParent=*/ false, /*bCopyEntries=*/ false, /*bCopyValues=*/ true);

	if (!TargetAttribute)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingTargetAttribute", "Error while creating target attribute '{0}'"), FText::FromName(TargetAttributeName)));
		return nullptr;
	}

	for (int32 i = 0; i < SourcePoints.Num(); ++i)
	{
		PCGMetadataEntryKey& TargetKey = TargetPoints[i].MetadataEntry;
		NewPointData->Metadata->InitializeOnSet(TargetKey);

		check(TargetKey != PCGInvalidEntryKey);

		TargetAttribute->SetValueFromValueKey(TargetKey, SourceAttribute->GetValueKey(SourcePoints[i].MetadataEntry));
	}

	return NewPointData;
}

#undef LOCTEXT_NAMESPACE
