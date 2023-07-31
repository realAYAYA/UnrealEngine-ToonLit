// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreateAttribute.h"

#include "PCGData.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"

#if WITH_EDITOR
FName UPCGCreateAttributeSettings::GetDefaultNodeName() const
{
	return TEXT("CreateAttribute");
}
#endif

TArray<FPCGPinProperties> UPCGCreateAttributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any, /*bInAllowMultipleConnections=*/ true);
	PinProperties.Emplace(PCGPinConstants::DefaultParamsLabel, EPCGDataType::Param, /*bInAllowMultipleConnections=*/ false);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGCreateAttributeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGCreateAttributeSettings::CreateElement() const
{
	return MakeShared<FPCGCreateAttributeElement>();
}

bool FPCGCreateAttributeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreateAttributeElement::Execute);

	check(Context);

	const UPCGCreateAttributeSettings* Settings = Context->GetInputSettings<UPCGCreateAttributeSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Params = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultParamsLabel);
	UPCGParamData* ParamData = nullptr;

	if (!Params.IsEmpty())
	{
		ParamData = CastChecked<UPCGParamData>(Params[0].Data);
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// If the input is empty, we will create a new ParamData.
	// We can re-use this newly object as the output
	bool bCanReuseInputData = false;
	if (Inputs.IsEmpty())
	{
		FPCGTaggedData& NewData = Inputs.Emplace_GetRef();
		NewData.Data = NewObject<UPCGParamData>();
		NewData.Pin = PCGPinConstants::DefaultInputLabel;
		bCanReuseInputData = true;
	}

	for (const FPCGTaggedData& InputTaggedData : Inputs)
	{
		const UPCGData* InputData = InputTaggedData.Data;
		UPCGData* OutputData = nullptr;

		UPCGMetadata* Metadata = NewObject<UPCGMetadata>();

		bool bShouldAddNewEntry = false;

		if (const UPCGSpatialData* InputSpatialData = Cast<UPCGSpatialData>(InputData))
		{
			UPCGSpatialData* NewSpatialData = DuplicateObject<UPCGSpatialData>(const_cast<UPCGSpatialData*>(InputSpatialData), nullptr);
			NewSpatialData->Metadata = Metadata;
			NewSpatialData->InitializeFromData(InputSpatialData, /*InMetadataParentOverride=*/ nullptr, /*bInheritMetadata=*/ Settings->bKeepExistingAttributes);
			
			OutputData = NewSpatialData;
		}
		else if (const UPCGParamData* InputParamData = Cast<UPCGParamData>(InputData))
		{
			// If we can reuse input data, it is safe to const_cast, as it was created by ourselves above.
			UPCGParamData* NewParamData = bCanReuseInputData ? const_cast<UPCGParamData*>(InputParamData) : NewObject<UPCGParamData>();
			NewParamData->Metadata = Metadata;

			Metadata->Initialize(Settings->bKeepExistingAttributes ? InputParamData->Metadata : nullptr);
			OutputData = NewParamData;

			// In case of param data, we want to add a new entry too
			bShouldAddNewEntry = true;
		}
		else
		{
			PCGE_LOG(Error, "Invalid data as input. Only support spatial and params");
			continue;
		}

		FPCGMetadataAttributeBase* Attribute = Settings->ClearOrCreateAttribute(Metadata, ParamData);

		if (!Attribute)
		{
			PCGE_LOG(Error, "Error while creating attribute %s", *Settings->OutputAttributeName.ToString());
			continue;
		}

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = OutputData;

		// Add a new entry if it is a param data
		if (bShouldAddNewEntry)
		{
			PCGMetadataEntryKey EntryKey = Metadata->AddEntry();
			Settings->SetAttribute(Attribute, Metadata, EntryKey, ParamData);
		}
	}

	return true;
}
