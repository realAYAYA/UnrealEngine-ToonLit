// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDeleteAttributesElement.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGDeleteAttributesElement"

namespace PCGAttributeFilterConstants
{
	const FName NodeName = TEXT("DeleteAttributes");
	const FText NodeTitle = LOCTEXT("NodeTitle", "Delete Attributes");
	const FText NodeTitleAlias = LOCTEXT("NodeTitleAlias", "Filter Attributes By Name");
}

UPCGDeleteAttributesSettings::UPCGDeleteAttributesSettings()
{
	if (PCGHelpers::IsNewObjectAndNotDefault(this))
	{
		Operation = EPCGAttributeFilterOperation::DeleteSelectedAttributes;
	}
}

void UPCGDeleteAttributesSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (!AttributesToKeep_DEPRECATED.IsEmpty())
	{
		SelectedAttributes.Empty();
		Operation = EPCGAttributeFilterOperation::KeepSelectedAttributes;
		// Can't use FString::Join since it is an array of FName
		for (int i = 0; i < AttributesToKeep_DEPRECATED.Num(); ++i)
		{
			if (i != 0)
			{
				SelectedAttributes += TEXT(",");
			}

			SelectedAttributes += AttributesToKeep_DEPRECATED[i].ToString();
		}

		AttributesToKeep_DEPRECATED.Empty();
	}
#endif // WITH_EDITOR
}

#if WITH_EDITOR
FName UPCGDeleteAttributesSettings::GetDefaultNodeName() const
{
	return PCGAttributeFilterConstants::NodeName;
}

FText UPCGDeleteAttributesSettings::GetDefaultNodeTitle() const
{
	return PCGAttributeFilterConstants::NodeTitle;
}

TArray<FText> UPCGDeleteAttributesSettings::GetNodeTitleAliases() const
{
	return { PCGAttributeFilterConstants::NodeTitleAlias };
}
#endif

FString UPCGDeleteAttributesSettings::GetAdditionalTitleInformation() const
{
	// The display name for the operation is way too long when put in a node title, so abbreviate it here.
	FString OperationString;
	if (Operation == EPCGAttributeFilterOperation::KeepSelectedAttributes)
	{
		OperationString = LOCTEXT("OperationKeep", "Keep").ToString();
	}
	else if (Operation == EPCGAttributeFilterOperation::DeleteSelectedAttributes)
	{
		OperationString = LOCTEXT("OperationDelete", "Delete").ToString();
	}
	else
	{
		ensureMsgf(false, TEXT("Unrecognized operation"));
	}

	TArray<FString> AttributesToKeep = PCGHelpers::GetStringArrayFromCommaSeparatedString(SelectedAttributes);
	if (AttributesToKeep.Num() == 1)
	{
		return FString::Printf(TEXT("%s (%s)"), *OperationString, *AttributesToKeep[0]);
	}
	else if (AttributesToKeep.IsEmpty())
	{
		return FString::Printf(TEXT("%s (%s)"), *OperationString, *LOCTEXT("NoAttributes", "None").ToString());
	}
	else
	{
		return FString::Printf(TEXT("%s (%s)"), *OperationString, *LOCTEXT("KeepMultipleAttributes", "Multiple").ToString());
	}
}

TArray<FPCGPinProperties> UPCGDeleteAttributesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGDeleteAttributesSettings::CreateElement() const
{
	return MakeShared<FPCGDeleteAttributesElement>();
}

bool FPCGDeleteAttributesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDeleteAttributesElement::Execute);

	check(Context);

	const UPCGDeleteAttributesSettings* Settings = Context->GetInputSettings<UPCGDeleteAttributesSettings>();

	const bool bAddAttributesFromParent = (Settings->Operation == EPCGAttributeFilterOperation::DeleteSelectedAttributes);
	const EPCGMetadataFilterMode FilterMode = bAddAttributesFromParent ? EPCGMetadataFilterMode::ExcludeAttributes : EPCGMetadataFilterMode::IncludeAttributes;

	TSet<FName> AttributesToFilter; 
	const TArray<FString> FilterAttributes = PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->SelectedAttributes);
	for (const FString& FilterAttribute : FilterAttributes)
	{
		AttributesToFilter.Add(FName(*FilterAttribute));
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	for (const FPCGTaggedData& InputTaggedData : Inputs)
	{
		const UPCGData* InputData = InputTaggedData.Data;
		UPCGData* OutputData = nullptr;

		const UPCGMetadata* ParentMetadata = nullptr;
		UPCGMetadata* Metadata = nullptr;

		if (const UPCGSpatialData* InputSpatialData = Cast<UPCGSpatialData>(InputData))
		{
			ParentMetadata = InputSpatialData->Metadata;

			UPCGSpatialData* NewSpatialData = InputSpatialData->DuplicateData(/*bInitializeFromThisData=*/false);
			Metadata = NewSpatialData->Metadata;
			NewSpatialData->Metadata->InitializeWithAttributeFilter(ParentMetadata, AttributesToFilter, FilterMode);

			// No need to inherit metadata since we already initialized it.
			NewSpatialData->InitializeFromData(InputSpatialData, /*InMetadataParentOverride=*/ nullptr, /*bInheritMetadata=*/ false);

			OutputData = NewSpatialData;
		}
		else if (const UPCGParamData* InputParamData = Cast<UPCGParamData>(InputData))
		{
			ParentMetadata = InputParamData->Metadata;

			UPCGParamData* NewParamData = NewObject<UPCGParamData>();
			Metadata = NewParamData->Metadata;
			Metadata->InitializeAsCopyWithAttributeFilter(ParentMetadata, AttributesToFilter, FilterMode);

			OutputData = NewParamData;
		}
		else
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid data as input. Only Spatial and Attribute Set data are supported."));
			continue;
		}

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Add_GetRef(InputTaggedData);
		Output.Data = OutputData;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
