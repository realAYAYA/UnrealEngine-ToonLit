// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataRenameElement.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

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

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData> AllParams = Context->InputData.GetAllParams();
	UPCGParamData* Params = Context->InputData.GetParams();

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	
	const FName AttributeToRename = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGMetadataRenameSettings, AttributeToRename), Settings->AttributeToRename, Params);
	const FName NewAttributeName = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGMetadataRenameSettings, NewAttributeName), Settings->NewAttributeName, Params);

	if (NewAttributeName == NAME_None)
	{
		UE_LOG(LogPCG, Warning, TEXT("Metadata rename operation cannot rename from %s to %s"), *AttributeToRename.ToString(), *NewAttributeName.ToString());
		// Bypass
		Context->OutputData = Context->InputData;
		return true;
	}

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data);
		if (!SpatialInput)
		{
			continue;
		}

		// If the data has a metadata & the attribute to rename, then duplicate the data
		// otherwise, keep it as is.
		const UPCGMetadata* Metadata = SpatialInput->Metadata;

		if (!Metadata)
		{
			continue;
		}

		const FName LocalAttributeToRename = ((AttributeToRename != NAME_None) ? AttributeToRename : Metadata->GetLatestAttributeNameOrNone());

		if (!Metadata->HasAttribute(LocalAttributeToRename))
		{
			continue;
		}

		//TODO: this might require to execute on the main thread
		UPCGSpatialData* NewSpatialData = Cast<UPCGSpatialData>(StaticDuplicateObject(SpatialInput, const_cast<UPCGSpatialData*>(SpatialInput), FName()));
		NewSpatialData->InitializeFromData(SpatialInput);
		NewSpatialData->Metadata->RenameAttribute(LocalAttributeToRename, NewAttributeName);

		Output.Data = NewSpatialData;
	}

	for (const FPCGTaggedData& ParamTaggedData : AllParams)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(ParamTaggedData);

		const UPCGParamData* InputParams = Cast<const UPCGParamData>(ParamTaggedData.Data);
		if (!InputParams)
		{
			continue;
		}
		
		const UPCGMetadata* Metadata = InputParams->Metadata;

		if (!Metadata)
		{
			continue;
		}

		const FName LocalAttributeToRename = ((AttributeToRename != NAME_None) ? AttributeToRename : Metadata->GetLatestAttributeNameOrNone());

		if (!Metadata->HasAttribute(LocalAttributeToRename))
		{
			continue;
		}

		//TODO: this might require to execute on the main thread
		UPCGParamData* NewParams = Cast<UPCGParamData>(StaticDuplicateObject(InputParams, const_cast<UPCGParamData*>(InputParams), FName()));
		// Note: we will not parent the metadata here to ensure that the 0th entry is present on this metadata
		// We will instead do a copy (and keep its parent, etc.)
		NewParams->Metadata->InitializeAsCopy(Metadata);
		NewParams->Metadata->RenameAttribute(LocalAttributeToRename, NewAttributeName);

		Output.Data = NewParams;
	}

	// Pass-through settings
	Outputs.Append(Context->InputData.GetAllSettings());

	return true;
}