// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGNumberOfElements.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGNumberOfElementsSettings"

////////////////////////////////////
// UPCGNumberOfElementsBaseSettings
////////////////////////////////////

TArray<FPCGPinProperties> UPCGNumberOfElementsBaseSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}


////////////////////////////////////
// UPCGNumberOfPointsSettings
////////////////////////////////////

TArray<FPCGPinProperties> UPCGNumberOfPointsSettings::InputPinProperties() const
{
	return Super::DefaultPointInputPinProperties();
}

FPCGElementPtr UPCGNumberOfPointsSettings::CreateElement() const
{
	return MakeShared<FPCGNumberOfPointsElement>();
}


////////////////////////////////////
// UPCGNumberOfEntriesSettings
////////////////////////////////////

TArray<FPCGPinProperties> UPCGNumberOfEntriesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Param, /*bInAllowMultipleConnections=*/true, /*bAllowMultipleData=*/true);
	InputPinProperty.SetRequiredPin();

	return PinProperties;
}

FPCGElementPtr UPCGNumberOfEntriesSettings::CreateElement() const
{
	return MakeShared<FPCGNumberOfEntriesElement>();
}


////////////////////////////////////
// FPCGNumberOfElementsBaseElement
////////////////////////////////////

template <typename DataType>
bool FPCGNumberOfElementsBaseElement<DataType>::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGNumberOfElementsBaseElement::Execute);

	check(Context);

	const UPCGNumberOfElementsBaseSettings* Settings = Context->GetInputSettings<UPCGNumberOfElementsBaseSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	UPCGParamData* OutputParamData = nullptr;
	FPCGMetadataAttribute<int32>* NewAttribute = nullptr;

	for (int32 i = 0; i < Inputs.Num(); ++i)
	{
		const DataType* InputData = Cast<DataType>(Inputs[i].Data);

		if (!InputData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InputNotRightType", "Input {0} is not of the right input type, discarded"), FText::AsNumber(i)));
			continue;
		}

		if (!OutputParamData)
		{
			OutputParamData = NewObject<UPCGParamData>();
			NewAttribute = OutputParamData->Metadata->CreateAttribute<int32>(Settings->OutputAttributeName, 0, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false);

			if (!NewAttribute)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeFailedToBeCreated", "New Attribute {0} failed to be created."), FText::FromName(Settings->OutputAttributeName)));
				return true;
			}
		}

		check(OutputParamData && NewAttribute);
		// Implementation note: since we might have multiple entries, we must set the actual value to a new entry
		NewAttribute->SetValue(OutputParamData->Metadata->AddEntry(), GetNum(InputData));
	}

	if (OutputParamData)
	{
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = OutputParamData;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}

	return true;
}


////////////////////////////////////
// FPCGNumberOfPointsElement
////////////////////////////////////

int32 FPCGNumberOfPointsElement::GetNum(const UPCGPointData* InData) const
{
	return InData ? InData->GetPoints().Num() : 0;
}


////////////////////////////////////
// FPCGNumberOfEntriesElement
////////////////////////////////////

int32 FPCGNumberOfEntriesElement::GetNum(const UPCGParamData* InData) const
{
	return (InData && InData->Metadata) ? InData->Metadata->GetLocalItemCount() : 0;
}

#undef LOCTEXT_NAMESPACE
