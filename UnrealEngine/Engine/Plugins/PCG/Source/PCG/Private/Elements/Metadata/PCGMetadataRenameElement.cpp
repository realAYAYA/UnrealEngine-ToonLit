// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataRenameElement.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataRenameElement)

#define LOCTEXT_NAMESPACE "PCGMetadataRenameElement"

FString UPCGMetadataRenameSettings::GetAdditionalTitleInformation() const
{
#if WITH_EDITOR
	if (IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGMetadataRenameSettings, AttributeToRename)) || IsPropertyOverriddenByPin(GET_MEMBER_NAME_CHECKED(UPCGMetadataRenameSettings, NewAttributeName)))
	{
		return FString();
	}
	else
#endif
	{
		return FString::Printf(TEXT("%s -> %s"), *AttributeToRename.ToString(), *NewAttributeName.ToString());
	}
}

TArray<FPCGPinProperties> UPCGMetadataRenameSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGMetadataRenameSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataRenameElement>();
}

bool FPCGMetadataRenameElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataOperationElement::Execute);

	const UPCGMetadataRenameSettings* Settings = Context->GetInputSettings<UPCGMetadataRenameSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	
	const FName AttributeToRename = Settings->AttributeToRename;
	const FName NewAttributeName = Settings->NewAttributeName;

	if (NewAttributeName == NAME_None)
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InvalidTargetAttributeName", "Invalid target attribute name '{0}'"), FText::FromName(NewAttributeName)));
		// Bypass
		Context->OutputData = Context->InputData;
		return true;
	}

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		const UPCGMetadata* Metadata = nullptr;

		if (const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data))
		{
			Metadata = SpatialInput->Metadata;
			Output.Data = SpatialInput;
		}
		else if (const UPCGParamData* ParamInput = Cast<UPCGParamData>(Input.Data))
		{
			Metadata = ParamInput->Metadata;
			Output.Data = ParamInput;
		}

		if (!Metadata)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidInputType", "Input data type is not supported, only supports Spatial and Attribute Set"));
			Output.Data = nullptr;
			continue;
		}

		const FName LocalAttributeToRename = ((AttributeToRename != NAME_None) ? AttributeToRename : Metadata->GetLatestAttributeNameOrNone());

		if (!Metadata->HasAttribute(LocalAttributeToRename))
		{
			continue;
		}

		UPCGMetadata* NewMetadata = nullptr;
		PCGMetadataElementCommon::DuplicateTaggedData(Input, Output, NewMetadata);

		if (!NewMetadata || !NewMetadata->RenameAttribute(LocalAttributeToRename, NewAttributeName))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeRenamedFailed", "Failed to rename attribute from '{0}' to '{1}'"),
				FText::FromName(LocalAttributeToRename), FText::FromName(NewAttributeName)));
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
