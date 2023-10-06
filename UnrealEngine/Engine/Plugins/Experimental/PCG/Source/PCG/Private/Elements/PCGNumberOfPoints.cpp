// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGNumberOfPoints.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGPin.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGNumberOfPointsSettings"


TArray<FPCGPinProperties> UPCGNumberOfPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, /*bInAllowMultipleConnections=*/ false, /*bAllowMultipleData=*/false);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGNumberOfPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FPCGElementPtr UPCGNumberOfPointsSettings::CreateElement() const
{
	return MakeShared<FPCGNumberOfPointsElement>();
}

bool FPCGNumberOfPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGNumberOfPointsElement::Execute);
	
	check(Context);

	const UPCGNumberOfPointsSettings* Settings = Context->GetInputSettings<UPCGNumberOfPointsSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	if (Inputs.Num() != 1)
	{
		PCGE_LOG(Warning, LogOnly, FText::Format(LOCTEXT("WrongNumberOfInputs", "Input pin expected to have one input data element, encountered {0}"), Inputs.Num()));
		return true;
	}

	const UPCGPointData* PointData = Cast<UPCGPointData>(Inputs[0].Data);

	if (!PointData)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InputNotPointData", "Input is not a point data"));
		return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGParamData* OutputParamData = NewObject<UPCGParamData>();
	
	const int32 NumPoints = PointData->GetPoints().Num();

	FPCGMetadataAttribute<int32>* NewAttribute =
		OutputParamData->Metadata->CreateAttribute<int32>(Settings->OutputAttributeName, NumPoints, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false);

	if (NewAttribute) 
	{
		NewAttribute->SetValue(OutputParamData->Metadata->AddEntry(), NumPoints);

		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = OutputParamData;
		Output.Pin = PCGPinConstants::DefaultOutputLabel;
	}
	else 
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("AttributeNotFound", "New Attribute failed to create."));
	}


	return true;
}

#undef LOCTEXT_NAMESPACE
