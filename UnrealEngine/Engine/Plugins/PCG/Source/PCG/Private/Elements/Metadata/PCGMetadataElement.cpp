// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataElement.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGMetadataHelpers.h"

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

FString UPCGMetadataOperationSettings::GetAdditionalTitleInformation() const
{
#if WITH_EDITOR
	if (bCopyAllAttributes)
	{
		return LOCTEXT("NoteTitleAllAttributes", "All Attributes").ToString();
	}

	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGMetadataOperationSettings, InputSource)) || IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGMetadataOperationSettings, OutputTarget)))
	{
		return FString();
	}
#endif

	return FString::Printf(TEXT("%s -> %s"), *InputSource.GetDisplayText().ToString(), *OutputTarget.GetDisplayText().ToString());
}

#if WITH_EDITOR
EPCGChangeType UPCGMetadataOperationSettings::GetChangeTypeForProperty(const FName& InPropertyName) const
{
	return Super::GetChangeTypeForProperty(InPropertyName) | EPCGChangeType::Cosmetic;
}
#endif // WITH_EDITOR

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
	FPCGPinProperties& InputPinProperty = Properties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	InputPinProperty.SetRequiredPin();

	Properties.Emplace(PCGMetadataOperationSettings::AttributeLabel, EPCGDataType::Param, /*bInAllowMultipleConnections=*/ false, /*bAllowMultipleData=*/ false, PCGMetadataOperationSettings::AttributeTooltip);
	return Properties;
}

void UPCGMetadataOperationSettings::PostLoad()
{
	Super::PostLoad();
}

bool FPCGMetadataOperationElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataOperationElement::Execute);

	const UPCGMetadataOperationSettings* Settings = Context->GetInputSettings<UPCGMetadataOperationSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData> SourceAttributeInputs = Context->InputData.GetInputsByPin(PCGMetadataOperationSettings::AttributeLabel);

	const UPCGParamData* SourceAttributeSet = (!SourceAttributeInputs.IsEmpty() ? Cast<UPCGParamData>(SourceAttributeInputs[0].Data) : nullptr);

	if (SourceAttributeSet && Settings->InputSource.GetSelection() != EPCGAttributePropertySelection::Attribute && !Settings->bCopyAllAttributes)
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

		// Early out when trying to copy all attributes from self, since nothing will happen
		if (!SourceAttributeSet && Settings->bCopyAllAttributes)
		{
			PCGE_LOG(Verbose, LogOnly, LOCTEXT("TrivialCopy", "Copying all attributes on itself is a trivial operation."));
			continue;
		}

		UPCGPointData* SampledData = CastChecked<UPCGPointData>(OriginalData->DuplicateData());
		bool bSuccess = false;
		if (Settings->bCopyAllAttributes)
		{
			bSuccess = PCGMetadataHelpers::CopyAllAttributes(SourceData, SampledData, Context);
		}
		else
		{
			PCGMetadataHelpers::FPCGCopyAttributeParams Params{};
			Params.SourceData = SourceData;
			Params.TargetData = SampledData;
			Params.InputSource = Settings->InputSource;
			Params.OutputTarget = Settings->OutputTarget;
			Params.OptionalContext = Context;
			Params.bSameOrigin = !SourceAttributeSet;

			bSuccess = PCGMetadataHelpers::CopyAttribute(Params);
		}

		if (bSuccess)
		{
			Output.Data = SampledData;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
