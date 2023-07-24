// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAttributeGetFromPointIndexElement.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGAttributeGetFromPointIndexElement)

#define LOCTEXT_NAMESPACE "PCGAttributeGetFromPointIndexElement"

#if WITH_EDITOR
FName UPCGAttributeGetFromPointIndexSettings::GetDefaultNodeName() const
{
	return TEXT("GetAttributeFromPointIndex");
}

FText UPCGAttributeGetFromPointIndexSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Get Attribute From Point Index");
}
#endif

void UPCGAttributeGetFromPointIndexSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (InputAttributeName_DEPRECATED != NAME_None)
	{
		InputSource.SetAttributeName(InputAttributeName_DEPRECATED);
		InputAttributeName_DEPRECATED = NAME_None;
	}
#endif
}

TArray<FPCGPinProperties> UPCGAttributeGetFromPointIndexSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point, /*bInAllowMultipleConnections=*/ false);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGAttributeGetFromPointIndexSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGAttributeGetFromPointIndexConstants::OutputAttributeLabel, EPCGDataType::Param);
	PinProperties.Emplace(PCGAttributeGetFromPointIndexConstants::OutputPointLabel, EPCGDataType::Point);

	return PinProperties;
}

FPCGElementPtr UPCGAttributeGetFromPointIndexSettings::CreateElement() const
{
	return MakeShared<FPCGAttributeGetFromPointIndexElement>();
}

bool FPCGAttributeGetFromPointIndexElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAttributeGetFromPointIndexElement::Execute);

	check(Context);

	const UPCGAttributeGetFromPointIndexSettings* Settings = Context->GetInputSettings<UPCGAttributeGetFromPointIndexSettings>();
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

	const int32 Index = Settings->Index;

	if (Index < 0 || Index >= PointData->GetPoints().Num())
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("IndexOutOfBounds", "Index is out of bounds. Index: {0}; Number of Points: {1}"), Index, PointData->GetPoints().Num()));
		return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	const FPCGPoint& Point = PointData->GetPoints()[Index];

#if !WITH_EDITOR
	// Eschew output creation only in non-editor builds
	if(Context->Node && Context->Node->IsOutputPinConnected(PCGAttributeGetFromPointIndexConstants::OutputPointLabel))
#endif
	{
		UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
		OutputPointData->InitializeFromData(PointData);
		OutputPointData->GetMutablePoints().Add(Point);

		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = OutputPointData;
		Output.Pin = PCGAttributeGetFromPointIndexConstants::OutputPointLabel;
	}

	FPCGAttributePropertySelector InputSource = Settings->InputSource;
	if (InputSource.Selection == EPCGAttributePropertySelection::Attribute && InputSource.AttributeName == NAME_None)
	{
		InputSource.SetAttributeName(PointData->Metadata->GetLatestAttributeNameOrNone());
	}

	FName OutputAttributeName = (Settings->OutputAttributeName == NAME_None) ? InputSource.GetName() : Settings->OutputAttributeName;

	TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, InputSource);
	FPCGAttributeAccessorKeysPoints PointKey(Point);

	if (Accessor.IsValid())
	{
		UPCGParamData* OutputParamData = NewObject<UPCGParamData>();

		auto ExtractAttribute = [this, Context, OutputAttributeName, &OutputParamData, &Accessor, &PointKey](auto DummyValue) -> bool
		{
			using AttributeType = decltype(DummyValue);

			AttributeType Value{};

			// Should never fail, as OutputType == Accessor->UnderlyingType
			if (!ensure(Accessor->Get<AttributeType>(Value, PointKey)))
			{
				return false;
			}

			FPCGMetadataAttribute<AttributeType>* NewAttribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(
				OutputParamData->Metadata->CreateAttribute<AttributeType>(OutputAttributeName, Value, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false));

			if (!NewAttribute)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingTargetAttribute", "Error while creating target attribute '{0}'"), FText::FromName(OutputAttributeName)));
				return false;
			}

			NewAttribute->SetValue(OutputParamData->Metadata->AddEntry(), Value);

			return true;
		};

		if (PCGMetadataAttribute::CallbackWithRightType(Accessor->GetUnderlyingType(), ExtractAttribute))
		{
			FPCGTaggedData& Output = Outputs.Emplace_GetRef();
			Output.Data = OutputParamData;
			Output.Pin = PCGAttributeGetFromPointIndexConstants::OutputAttributeLabel;
		}
	}
	else
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeNotFound", "Cannot find attribute/property '{0}' in input"), FText::FromName(InputSource.GetName())));
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
