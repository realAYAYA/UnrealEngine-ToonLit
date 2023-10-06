// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataElement.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataElement)

#define LOCTEXT_NAMESPACE "PCGMetadataElement"

namespace PCGMetadataOperationSettings
{
	const FName AttributeLabel = TEXT("Attribute");
	const FText AttributeTooltip = LOCTEXT("AttributeTooltip", "Optional Attribute Set to copy the value from. Not used if not connected.");
}

UPCGMetadataOperationSettings::UPCGMetadataOperationSettings()
{
	// Previous default object was: None for source and target attribute, density for point property
	// and Property to attribute
	// Recreate the same default
	InputSource.SetPointProperty(EPCGPointProperties::Density);
	OutputTarget.SetAttributeName(NAME_None);
}

FPCGElementPtr UPCGMetadataOperationSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataOperationElement>();
}

#if WITH_EDITOR
FText UPCGMetadataOperationSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Copy from the Input Source, taken from either the Attribute Set if connected otherwise from the input points, to Output Target attribute.");
}

bool UPCGMetadataOperationSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	return !InPin || (InPin->Properties.Label != PCGMetadataOperationSettings::AttributeLabel) || InPin->IsConnected();
}

void UPCGMetadataOperationSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (DataVersion < FPCGCustomVersion::UpdateAttributePropertyInputSelector
		&& OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute
		&& OutputTarget.GetAttributeName() == PCGMetadataAttributeConstants::SourceAttributeName)
	{
		// Previous behavior of the output target for this node was:
		// None => LastCreated
		OutputTarget.SetAttributeName(PCGMetadataAttributeConstants::LastCreatedAttributeName);
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGMetadataOperationSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> Properties;
	Properties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	Properties.Emplace(PCGMetadataOperationSettings::AttributeLabel, EPCGDataType::Param, /*bInAllowMultipleConnections=*/ false, /*bAllowMultipleData=*/ false, PCGMetadataOperationSettings::AttributeTooltip);
	return Properties;
}

void UPCGMetadataOperationSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Target_DEPRECATED != EPCGMetadataOperationTarget::PropertyToAttribute
		|| (SourceAttribute_DEPRECATED != NAME_None)
		|| (DestinationAttribute_DEPRECATED != NAME_None)
		|| (PointProperty_DEPRECATED != EPCGPointProperties::Density))
	{
		switch (Target_DEPRECATED)
		{
		case EPCGMetadataOperationTarget::PropertyToAttribute:
			InputSource.SetPointProperty(PointProperty_DEPRECATED);
			OutputTarget.SetAttributeName(DestinationAttribute_DEPRECATED);
			break;
		case EPCGMetadataOperationTarget::AttributeToProperty:
			InputSource.SetAttributeName(SourceAttribute_DEPRECATED);
			OutputTarget.SetPointProperty(PointProperty_DEPRECATED);
			break;
		case EPCGMetadataOperationTarget::AttributeToAttribute:
			InputSource.SetAttributeName(SourceAttribute_DEPRECATED);
			OutputTarget.SetAttributeName(DestinationAttribute_DEPRECATED);
			break;
		default:
			break;
		}

		// Default values.
		SourceAttribute_DEPRECATED = NAME_None;
		DestinationAttribute_DEPRECATED = NAME_None;
		Target_DEPRECATED = EPCGMetadataOperationTarget::PropertyToAttribute;
		PointProperty_DEPRECATED = EPCGPointProperties::Density;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR
}

bool FPCGMetadataOperationElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataOperationElement::Execute);

	const UPCGMetadataOperationSettings* Settings = Context->GetInputSettings<UPCGMetadataOperationSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData> SourceAttributeInputs = Context->InputData.GetInputsByPin(PCGMetadataOperationSettings::AttributeLabel);

	const UPCGParamData* SourceAttributeSet = (!SourceAttributeInputs.IsEmpty() ? Cast<UPCGParamData>(SourceAttributeInputs[0].Data) : nullptr);

	if (SourceAttributeSet && Settings->InputSource.GetSelection() != EPCGAttributePropertySelection::Attribute)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidInputSource", "{0} pin is connected but Input Source is not set to get an attribute."), FText::FromName(PCGMetadataOperationSettings::AttributeLabel)));
		return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data);

		if (!SpatialInput)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data type, must be of type Spatial"));
			continue;
		}

		const UPCGPointData* OriginalData = SpatialInput->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnableToGetPointData", "Unable to get point data from input"));
			continue;
		}

		const UPCGData* SourceData = SourceAttributeSet ? static_cast<const UPCGData*>(SourceAttributeSet) : static_cast<const UPCGData*>(OriginalData);
		const UPCGMetadata* SourceMetadata = SourceAttributeSet ? SourceAttributeSet->Metadata : OriginalData->Metadata;

		if (!SourceMetadata)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingMetadata", "Input has no metadata"));
			continue;
		}

		FPCGAttributePropertyInputSelector InputSource = Settings->InputSource.CopyAndFixLast(SourceData);
		// Deprecation, old behavior was if the Target was None, we took LastCreated in the SourceData
		FPCGAttributePropertyOutputSelector OutputTarget = Settings->OutputTarget.CopyAndFixSource(&InputSource, SourceData);

		const FName LocalSourceAttribute = InputSource.GetName();
		const FName LocalDestinationAttribute = OutputTarget.GetName();

		if (InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute && !SourceMetadata->HasAttribute(LocalSourceAttribute))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InputMissingAttribute", "Input does not have the '{0}' attribute"), FText::FromName(LocalSourceAttribute)));
			continue;
		}

		const TArray<FPCGPoint>& Points = OriginalData->GetPoints();
		const int OriginalPointCount = Points.Num();

		UPCGPointData* SampledData = NewObject<UPCGPointData>();
		SampledData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& SampledPoints = SampledData->GetMutablePoints();

		Output.Data = SampledData;

		// Copy points and then apply the operation
		SampledPoints = Points;

		// If it is attribute to attribute, just copy the attributes, if they exist and are valid
		// Only do that if it is really attribute to attribute, without any extra accessor. Any extra accessor will behave as a property.
		const bool bInputHasAnyExtra = !InputSource.GetExtraNames().IsEmpty();
		const bool bOutputHasAnyExtra = !OutputTarget.GetExtraNames().IsEmpty();
		if (!bInputHasAnyExtra && !bOutputHasAnyExtra && Settings->InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute && OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (!SourceAttributeSet && LocalSourceAttribute == LocalDestinationAttribute)
			{
				// Nothing to do if we try to copy an attribute into itself in the original point data.
				continue;
			}
			
			const FPCGMetadataAttributeBase* OriginalAttribute = SourceMetadata->GetConstAttribute(LocalSourceAttribute);
			check(OriginalAttribute);

			// We only copy entries/values if we copy from the input spatial metadata
			// If it is from the source attribute set, we don't copy (and all points will have the same default value, value from the attribute set)
			const bool bCopyEntriesAndValues = (SourceAttributeSet == nullptr);
			if (!SampledData->Metadata->CopyAttribute(OriginalAttribute, LocalDestinationAttribute, /*bKeepParent=*/ false, /*bCopyEntries=*/ bCopyEntriesAndValues, /*bCopyValues=*/ bCopyEntriesAndValues))
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("FailedCopyToNewAttribute", "Failed to copy to new attribute '{0}'"), FText::FromName(LocalDestinationAttribute)));
			}

			continue;
		}

		TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(SourceData, InputSource);
		TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(SourceData, InputSource);

		if (!InputAccessor.IsValid() || !InputKeys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("FailedToCreateInputAccessor", "Failed to create input accessor or iterator"));
			continue;
		}

		// If the target is an attribute, only create a new one if the attribute doesn't already exist or we have any extra.
		// If it exist or have any extra, it will try to write to it.
		if (!bOutputHasAnyExtra && OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute && !SampledData->Metadata->HasAttribute(LocalDestinationAttribute))
		{
			auto CreateAttribute = [SampledData, LocalDestinationAttribute](auto Dummy)
			{
				using AttributeType = decltype(Dummy);
				return PCGMetadataElementCommon::ClearOrCreateAttribute(SampledData->Metadata, LocalDestinationAttribute, AttributeType{}) != nullptr;
			};
			
			if (!PCGMetadataAttribute::CallbackWithRightType(InputAccessor->GetUnderlyingType(), CreateAttribute))
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("FailedToCreateNewAttribute", "Failed to create new attribute '{0}'"), FText::FromName(LocalDestinationAttribute)));
				continue;
			}
		}

		TUniquePtr<IPCGAttributeAccessor> OutputAccessor = PCGAttributeAccessorHelpers::CreateAccessor(SampledData, OutputTarget);
		TUniquePtr<IPCGAttributeAccessorKeys> OutputKeys = PCGAttributeAccessorHelpers::CreateKeys(SampledData, OutputTarget);

		if (!OutputAccessor.IsValid() || !OutputKeys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("FailedToCreateOutputAccessor", "Failed to create output accessor or iterator"));
			continue;
		}

		if (OutputAccessor->IsReadOnly())
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("OutputAccessorIsReadOnly", "Attribute/Property '{0}' is read only."), OutputTarget.GetDisplayText()));
			continue;
		}

		// Final verification, if we can put the value of input into output
		if (!PCG::Private::IsBroadcastable(InputAccessor->GetUnderlyingType(), OutputAccessor->GetUnderlyingType()))
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotBroadcastTypes", "Cannot broadcast input type into output type"));
			continue;
		}

		// At this point, we are ready.
		auto Operation = [&InputAccessor, &InputKeys, &OutputAccessor, &OutputKeys](auto Dummy)
		{
			using OutputType = decltype(Dummy);
			OutputType Value{};

			EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::AllowBroadcast;

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

				InputAccessor->GetRange<OutputType>(View, StartIndex, *InputKeys, Flags);
				OutputAccessor->SetRange<OutputType>(View, StartIndex, *OutputKeys, Flags);
			}

			return true;
		};

		if (!PCGMetadataAttribute::CallbackWithRightType(OutputAccessor->GetUnderlyingType(), Operation))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("ErrorGettingSettingValues", "Error while getting/setting values"));
			continue;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
