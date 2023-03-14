// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreateParamData.h"

#include "PCGData.h"
#include "PCGParamData.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataCommon.h"

#if WITH_EDITOR
FName UPCGCreateParamDataSettings::GetDefaultNodeName() const
{
	return TEXT("CreateParamData");
}
#endif

TArray<FPCGPinProperties> UPCGCreateParamDataSettings::InputPinProperties() const
{
	return TArray<FPCGPinProperties>();
}

TArray<FPCGPinProperties> UPCGCreateParamDataSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FPCGElementPtr UPCGCreateParamDataSettings::CreateElement() const
{
	return MakeShared<FPCGCreateParamDataElement>();
}

bool FPCGCreateParamDataElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreateParamDataElement::Execute);

	check(Context);

	const UPCGCreateParamDataSettings* Settings = Context->GetInputSettings<UPCGCreateParamDataSettings>();
	check(Settings);

	// From there, we should be able to create the data.
	UPCGParamData* ParamData = NewObject<UPCGParamData>();
	UPCGMetadata* Metadata = ParamData->MutableMetadata();
	check(Metadata);
	PCGMetadataEntryKey EntryKey = Metadata->AddEntry();

	FPCGMetadataAttributeBase* Attribute = Settings->ClearOrCreateAttribute(Metadata);

	if (!Attribute)
	{
		PCGE_LOG(Error, "Error while creating attribute %s", *Settings->OutputAttributeName.ToString());
		return true;
	}

	Settings->SetAttribute(Attribute, Metadata, EntryKey);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = ParamData;

	return true;
}