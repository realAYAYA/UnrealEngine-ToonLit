// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAttributeTransferElement.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGGather.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGAttributeTransferElement)

#define LOCTEXT_NAMESPACE "PCGAttributeTransferElement"

void UPCGAttributeTransferSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (SourceAttributeName_DEPRECATED != NAME_None)
	{
		SourceAttributeProperty.SetAttributeName(SourceAttributeName_DEPRECATED);
		SourceAttributeName_DEPRECATED = NAME_None;
	}

	if (TargetAttributeName_DEPRECATED != NAME_None)
	{
		TargetAttributeProperty.SetAttributeName(TargetAttributeName_DEPRECATED);
		TargetAttributeName_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
FName UPCGAttributeTransferSettings::GetDefaultNodeName() const
{
	return PCGAttributeTransferConstants::NodeName;
}

FText UPCGAttributeTransferSettings::GetDefaultNodeTitle() const
{
	return PCGAttributeTransferConstants::NodeTitle;
}

FText UPCGAttributeTransferSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip",
		"Transfer an attribute from a source metadata to a target data. Only supports Spatial to Spatial and Points to Points, and they need to match.\n\n"
		" - For Spatial data, number of entries in the metadata should be the same between source and target.\n"
		" - For Point data, number of points should be the same between source and target.\n\n"
		"The output will be the target data with the updated metadata."
	);
}

void UPCGAttributeTransferSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (DataVersion < FPCGCustomVersion::UpdateTransferAttributeWithSelectors
		&& TargetAttributeProperty.GetSelection() == EPCGAttributePropertySelection::Attribute
		&& TargetAttributeProperty.GetAttributeName() == PCGMetadataAttributeConstants::SourceAttributeName)
	{
		// Previous behavior of the output target for this node was:
		// None => SourceName
		TargetAttributeProperty.SetAttributeName(PCGMetadataAttributeConstants::SourceNameAttributeName);
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif

EPCGDataType UPCGAttributeTransferSettings::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);
	if (!InPin->IsOutputPin())
	{
		return Super::GetCurrentPinTypes(InPin);
	}

	// All pins narrow to same type, which is Point if any input is Point, otherwise Spatial
	const bool bAnyArePoint = (GetTypeUnionOfIncidentEdges(PCGAttributeTransferConstants::TargetLabel) == EPCGDataType::Point)
		|| (GetTypeUnionOfIncidentEdges(PCGAttributeTransferConstants::SourceLabel) == EPCGDataType::Point);

	return bAnyArePoint ? EPCGDataType::Point : EPCGDataType::Spatial;
}

FString UPCGAttributeTransferSettings::GetAdditionalTitleInformation() const
{
	const FName SourceAttributeName = SourceAttributeProperty.GetName();
	const FName TargetAttributeName = TargetAttributeProperty.GetName();

	if (SourceAttributeName != TargetAttributeName && TargetAttributeName != NAME_None)
	{
		return FString::Printf(TEXT("%s %s to %s"), *PCGAttributeTransferConstants::NodeName.ToString(), *SourceAttributeName.ToString(), *TargetAttributeName.ToString());
	}
	else
	{
		return FString::Printf(TEXT("%s %s"), *PCGAttributeTransferConstants::NodeName.ToString(), *SourceAttributeName.ToString());
	}
}

TArray<FPCGPinProperties> UPCGAttributeTransferSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& TargetPinProperty = PinProperties.Emplace_GetRef(PCGAttributeTransferConstants::TargetLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/ false);
	TargetPinProperty.SetRequiredPin();

	PinProperties.Emplace(PCGAttributeTransferConstants::SourceLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/ false);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGAttributeTransferSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Spatial);

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

	if (Context->Node && !Context->Node->IsInputPinConnected(PCGAttributeTransferConstants::SourceLabel))
	{
		// If Source pin is unconnected then we no-op and pass through all data from Target pin.
		Context->OutputData = PCGGather::GatherDataForPin(Context->InputData, PCGAttributeTransferConstants::TargetLabel);
		return true;
	}

	const UPCGAttributeTransferSettings* Settings = Context->GetInputSettings<UPCGAttributeTransferSettings>();
	check(Settings);

	TArray<FPCGTaggedData> SourceInputs = Context->InputData.GetInputsByPin(PCGAttributeTransferConstants::SourceLabel);
	TArray<FPCGTaggedData> TargetInputs = Context->InputData.GetInputsByPin(PCGAttributeTransferConstants::TargetLabel);

	if (SourceInputs.Num() != 1 || TargetInputs.Num() != 1)
	{
		PCGE_LOG(Warning, LogOnly, FText::Format(LOCTEXT("WrongNumberOfInputs", "Source input contains {0} data elements, Target inputs contain {1} data elements, but both should contain precisely 1 data element"), SourceInputs.Num(), TargetInputs.Num()));
		return true;
	}

	const UPCGSpatialData* SourceData = Cast<const UPCGSpatialData>(SourceInputs[0].Data);
	const UPCGSpatialData* TargetData = Cast<const UPCGSpatialData>(TargetInputs[0].Data);

	if (!SourceData || !TargetData || SourceData->IsA<UPCGPointData>() != TargetData->IsA<UPCGPointData>())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnsupportedTypes", "Only support Spatial to Spatial data or Point to Point data"));
		return true;
	}

	const UPCGPointData* SourcePointData = Cast<const UPCGPointData>(SourceData);
	const UPCGPointData* TargetPointData = Cast<const UPCGPointData>(TargetData);

	const bool bIsPointData = SourcePointData && TargetPointData;

	// TODO: Provide some kind of interpolation when point counts are not equal?
	if (bIsPointData && SourcePointData->GetPoints().Num() != TargetPointData->GetPoints().Num())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("MismatchingPointCounts", "Source and target do not have the same number of points"));
		return true;
	}

	if (!SourceData->Metadata)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("SourceMissingMetadata", "Source does not have metadata"));
		return true;
	}

	const FPCGAttributePropertyInputSelector SourceAttributeProperty = Settings->SourceAttributeProperty.CopyAndFixLast(SourceData);
	const FPCGAttributePropertyOutputSelector TargetAttributeProperty = Settings->TargetAttributeProperty.CopyAndFixSource(&SourceAttributeProperty, SourceData);

	const FName SourceAttributeName = SourceAttributeProperty.GetName();
	const FName TargetAttributeName = TargetAttributeProperty.GetName();

	if (SourceAttributeProperty.GetSelection() == EPCGAttributePropertySelection::Attribute && !SourceData->Metadata->HasAttribute(SourceAttributeName))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("SourceMissingAttribute", "Source does not have the attribute '{0}'"), FText::FromName(SourceAttributeName)));
		return true;
	}

	UPCGSpatialData* OutputData = TargetData->DuplicateData();
	check(OutputData->Metadata);

	auto AppendOutputData = [Context, OutputData, &TargetInputs]()
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Add_GetRef(TargetInputs[0]);
		Output.Data = OutputData;
	};

	// If it is attribute to attribute, just copy the attributes, if they exist and are valid
	// Only do that if it is really attribute to attribute, without any extra accessor. Any extra accessor will behave as a property.
	const bool bInputHasAnyExtra = !SourceAttributeProperty.GetExtraNames().IsEmpty();
	const bool bOutputHasAnyExtra = !TargetAttributeProperty.GetExtraNames().IsEmpty();
	if (!bInputHasAnyExtra && !bOutputHasAnyExtra && SourceAttributeProperty.GetSelection() == EPCGAttributePropertySelection::Attribute && TargetAttributeProperty.GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		if (SourceData == TargetData && SourceAttributeName == TargetAttributeName)
		{
			// Nothing to do if we try to copy an attribute into itself
			AppendOutputData();
			return true;
		}

		const FPCGMetadataAttributeBase* SourceAttribute = SourceData->Metadata->GetConstAttribute(SourceAttributeName);
		check(SourceAttribute);

		if (bIsPointData)
		{
			UPCGPointData* OutPointData = CastChecked<UPCGPointData>(OutputData);

			const TArray<FPCGPoint>& SourcePoints = SourcePointData->GetPoints();
			TArray<FPCGPoint>& TargetPoints = OutPointData->GetMutablePoints();

			// Making sure the target attribute doesn't exist in the target
			if (OutPointData->Metadata->HasAttribute(TargetAttributeName))
			{
				OutPointData->Metadata->DeleteAttribute(TargetAttributeName);
			}

			FPCGMetadataAttributeBase* TargetAttribute = OutPointData->Metadata->CopyAttribute(SourceAttribute, TargetAttributeName, /*bKeepParent=*/ false, /*bCopyEntries=*/ false, /*bCopyValues=*/ true);

			if (!TargetAttribute)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingTargetAttribute", "Error while creating target attribute '{0}'"), FText::FromName(TargetAttributeName)));
				return true;
			}

			// For Point -> Point, the entry keys may not match the source points, so we explicitly write for all the target points
			for (int32 I = 0; I < SourcePoints.Num(); ++I)
			{
				PCGMetadataEntryKey& TargetKey = TargetPoints[I].MetadataEntry;
				OutPointData->Metadata->InitializeOnSet(TargetKey);
				check(TargetKey != PCGInvalidEntryKey);
				TargetAttribute->SetValueFromValueKey(TargetKey, SourceAttribute->GetValueKey(SourcePoints[I].MetadataEntry));
			}
		}
		else
		{
			// If we are copying Spatial -> Spatial, we can just copy the attribute across
			if (!OutputData->Metadata->CopyAttribute(SourceAttribute, TargetAttributeName, /*bKeepParent=*/ false, /*bCopyEntries=*/ true, /*bCopyValues=*/ true))
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedCopyToNewAttribute", "Failed to copy to new attribute '{0}'"), FText::FromName(TargetAttributeName)));
			}
		}

		AppendOutputData();

		return true;
	}

	TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(SourceData, SourceAttributeProperty);
	TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(SourceData, SourceAttributeProperty);

	if (!InputAccessor.IsValid() || !InputKeys.IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("FailedToCreateInputAccessor", "Failed to create input accessor or iterator"));
		return true;
	}

	// If the target is an attribute, only create a new one if the attribute doesn't already exist or we have any extra.
	// If it exist or have any extra, it will try to write to it.
	if (!bOutputHasAnyExtra && TargetAttributeProperty.GetSelection() == EPCGAttributePropertySelection::Attribute && !OutputData->Metadata->HasAttribute(TargetAttributeName))
	{
		auto CreateAttribute = [OutputData, TargetAttributeName](auto Dummy)
		{
			using AttributeType = decltype(Dummy);
			return PCGMetadataElementCommon::ClearOrCreateAttribute<AttributeType>(OutputData->Metadata, TargetAttributeName) != nullptr;
		};
		
		if (!PCGMetadataAttribute::CallbackWithRightType(InputAccessor->GetUnderlyingType(), CreateAttribute))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("FailedToCreateNewAttribute", "Failed to create new attribute '{0}'"), FText::FromName(TargetAttributeName)));
			return true;
		}
	}

	TUniquePtr<IPCGAttributeAccessor> OutputAccessor = PCGAttributeAccessorHelpers::CreateAccessor(OutputData, TargetAttributeProperty);
	TUniquePtr<IPCGAttributeAccessorKeys> OutputKeys = PCGAttributeAccessorHelpers::CreateKeys(OutputData, TargetAttributeProperty);

	if (!OutputAccessor.IsValid() || !OutputKeys.IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("FailedToCreateOutputAccessor", "Failed to create output accessor or iterator"));
		return true;
	}

	if (OutputAccessor->IsReadOnly())
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("OutputAccessorIsReadOnly", "Attribute/Property '{0}' is read only."), TargetAttributeProperty.GetDisplayText()));
		return true;
	}

	// At this point, we are ready.
	auto Operation = [&InputAccessor, &InputKeys, &OutputAccessor, &OutputKeys, Context](auto Dummy)
	{
		using OutputType = decltype(Dummy);
		OutputType Value{};

		EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::AllowBroadcast | EPCGAttributeAccessorFlags::AllowConstructible;

		const int32 NumberOfElements = OutputKeys->GetNum();
		constexpr int32 ChunkSize = 256;

		TArray<OutputType, TInlineAllocator<ChunkSize>> TempValues;
		TempValues.SetNum(ChunkSize);

		const int32 NumberOfIterations = (NumberOfElements + ChunkSize - 1) / ChunkSize;

		for (int32 i = 0; i < NumberOfIterations; ++i)
		{
			const int32 StartIndex = i * ChunkSize;
			const int32 Range = FMath::Min(NumberOfElements - StartIndex, ChunkSize);
			TArrayView<OutputType> View(TempValues.GetData(), Range);

			if (!InputAccessor->GetRange<OutputType>(View, StartIndex, *InputKeys, Flags)
				|| !OutputAccessor->SetRange<OutputType>(View, StartIndex, *OutputKeys, Flags))
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("ConversionFailed", "Source attribute/property cannot be converted to target attribute/property"));
				return false;
			}
		}

		return true;
	};

	if (!PCGMetadataAttribute::CallbackWithRightType(OutputAccessor->GetUnderlyingType(), Operation))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("ErrorGettingSettingValues", "Error while getting/setting values"));
		return true;
	}

	AppendOutputData();

	return true;
}

#undef LOCTEXT_NAMESPACE
