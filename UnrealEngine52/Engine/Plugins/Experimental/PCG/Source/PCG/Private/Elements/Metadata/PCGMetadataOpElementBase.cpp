// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataOpElementBase)

#define LOCTEXT_NAMESPACE "PCGMetadataElementBaseElement"

void UPCGMetadataSettingsBase::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (OutputAttributeName_DEPRECATED != NAME_None)
	{
		OutputTarget.Selection = EPCGAttributePropertySelection::Attribute;
		OutputTarget.AttributeName = OutputAttributeName_DEPRECATED;
		OutputAttributeName_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

TArray<FPCGPinProperties> UPCGMetadataSettingsBase::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	for (uint32 i = 0; i < GetInputPinNum(); ++i)
	{
		const FName PinLabel = GetInputPinLabel(i);
		if (PinLabel != NAME_None)
		{
			PinProperties.Emplace(PinLabel, EPCGDataType::Any, /*bAllowMultipleConnections=*/false);
		}
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMetadataSettingsBase::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	
	for (uint32 i = 0; i < GetOutputPinNum(); ++i)
	{
		const FName PinLabel = GetOutputPinLabel(i);
		if (PinLabel != NAME_None)
		{
			PinProperties.Emplace(PinLabel, EPCGDataType::Any);
		}
	}

	return PinProperties;
}

bool FPCGMetadataElementBase::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataElementBase::Execute);

	const UPCGMetadataSettingsBase* Settings = Context->GetInputSettings<UPCGMetadataSettingsBase>();
	check(Settings);

	const uint32 NumberOfInputs = Settings->GetInputPinNum();
	const uint32 NumberOfOutputs = Settings->GetOutputPinNum();

	check(NumberOfInputs > 0);
	check(NumberOfOutputs <= UPCGMetadataSettingsBase::MaxNumberOfOutputs);

	const TArray<FPCGTaggedData>& Inputs = Context->InputData.TaggedData;
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Gathering all the inputs metadata
	TArray<const UPCGMetadata*> SourceMetadata;
	TArray<const FPCGMetadataAttributeBase*> SourceAttribute;
	TArray<FPCGTaggedData> InputTaggedData;
	SourceMetadata.SetNum(NumberOfInputs);
	SourceAttribute.SetNum(NumberOfInputs);
	InputTaggedData.SetNum(NumberOfInputs);

	for (uint32 i = 0; i < NumberOfInputs; ++i)
	{
		TArray<FPCGTaggedData> InputData = Context->InputData.GetInputsByPin(Settings->GetInputPinLabel(i));
		if (InputData.IsEmpty())
		{
			// Absence of data not worth broadcasting to user as visual warning
			PCGE_LOG(Warning, LogOnly, FText::Format(LOCTEXT("MissingInputDataForPin", "No data provided on pin {0}"), i));
			return true;
		}
		else if (InputData.Num() > 1)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("TooMuchDataForPin", "Too many data items ({0}) provided on pin {1}"), InputData.Num(), i));
			return true;
		}

		// By construction, there can only be one of then(hence the 0 index)
		InputTaggedData[i] = MoveTemp(InputData[0]);

		// Only gather Spacial and Params input. 
		if (const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(InputTaggedData[i].Data))
		{
			SourceMetadata[i] = SpatialInput->Metadata;
		}
		else if (const UPCGParamData* ParamsInput = Cast<const UPCGParamData>(InputTaggedData[i].Data))
		{
			SourceMetadata[i] = ParamsInput->Metadata;
		}
		else
		{
			PCGE_LOG(Warning, LogOnly, FText::Format(LOCTEXT("InvalidInputDataTypeForPin", "Invalid data provided on pin {0}, must be of type Spatial or Attribute Set"), i));
			return true;
		}
	}

	FOperationData OperationData;
	OperationData.InputAccessors.SetNum(NumberOfInputs);
	OperationData.InputKeys.SetNum(NumberOfInputs);
	TArray<int32> NumberOfElements;
	NumberOfElements.SetNum(NumberOfInputs);

	OperationData.MostComplexInputType = (uint16)EPCGMetadataTypes::Unknown;
	OperationData.NumberOfElementsToProcess = -1;

	bool bNoOperationNeeded = false;

	// Use this name to forward it to the output if needed.
	// Only set it if the first input is an attribute.
	FName InputName = NAME_None;

	for (uint32 i = 0; i < NumberOfInputs; ++i)
	{
		// First we verify if the input data match the first one.
		if (i != 0 &&
			InputTaggedData[0].Data->GetClass() != InputTaggedData[i].Data->GetClass() &&
			!InputTaggedData[i].Data->IsA<UPCGParamData>())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InputTypeMismatch", "Input {0} is not of the same type than input 0 and is not an Attribute Set. This is not supported."), i));
			return true;
		}

		FPCGAttributePropertySelector InputSource = Settings->GetInputSource(i);

		if (InputSource.Selection == EPCGAttributePropertySelection::Attribute && InputSource.AttributeName == NAME_None)
		{
			InputSource.AttributeName = SourceMetadata[i]->GetLatestAttributeNameOrNone();
		}

		if (i == 0 && InputSource.Selection == EPCGAttributePropertySelection::Attribute)
		{
			InputName = InputSource.AttributeName;
		}

		OperationData.InputAccessors[i] = PCGAttributeAccessorHelpers::CreateConstAccessor(InputTaggedData[i].Data, InputSource);
		OperationData.InputKeys[i] = PCGAttributeAccessorHelpers::CreateConstKeys(InputTaggedData[i].Data, InputSource);

		if (!OperationData.InputAccessors[i].IsValid() || !OperationData.InputKeys[i].IsValid())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeDoesNotExist", "Attribute/Property '{0}' does not exist for input {1}"), FText::FromName(InputSource.GetName()), i));
			return true;
		}

		uint16 AttributeTypeId = OperationData.InputAccessors[i]->GetUnderlyingType();

		// Then verify that the type is OK
		bool bHasSpecialRequirement = false;
		if (!Settings->IsSupportedInputType(AttributeTypeId, i, bHasSpecialRequirement))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("UnsupportedAttributeType", "Attribute/Property '{0}' is not a supported type for input {1}"), FText::FromName(InputSource.GetName()), i));
			return true;
		}

		if (!bHasSpecialRequirement)
		{
			// In this case, check if we have a more complex type, or if we can broadcast to the most complex type.
			if (OperationData.MostComplexInputType == (uint16)EPCGMetadataTypes::Unknown || PCG::Private::IsMoreComplexType(AttributeTypeId, OperationData.MostComplexInputType))
			{
				OperationData.MostComplexInputType = AttributeTypeId;
			}
			else if (OperationData.MostComplexInputType != AttributeTypeId && !PCG::Private::IsBroadcastable(AttributeTypeId, OperationData.MostComplexInputType))
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeCannotBeBroadcasted", "Attribute '{0}' cannot be broadcasted to match types for input {1}"), FText::FromName(InputSource.GetName()), i));
				return true;
			}
		}

		NumberOfElements[i] = OperationData.InputKeys[i]->GetNum();

		if (OperationData.NumberOfElementsToProcess == -1)
		{
			OperationData.NumberOfElementsToProcess = NumberOfElements[i];
		}

		// There is nothing to do if one input doesn't have any element to process.
		// Therefore mark that we have nothing to do and early out.
		if (NumberOfElements[i] == 0)
		{
			PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("NoElementsInInput", "No elements in input {0}"), i));
			bNoOperationNeeded = true;
			break;
		}

		// Verify that the number of elements makes sense
		if (OperationData.NumberOfElementsToProcess % NumberOfElements[i] != 0)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("MismatchInNumberOfElements", "Mismatch between the number of elements in input 0 ({0}) and in input {1} ({2})"), OperationData.NumberOfElementsToProcess, i, NumberOfElements[i]));
			return true;
		}

		if (InputSource.Selection == EPCGAttributePropertySelection::Attribute)
		{
			SourceAttribute[i] = SourceMetadata[i]->GetConstAttribute(InputSource.GetName());
		}
		else
		{
			SourceAttribute[i] = nullptr;
		}
	}

	// If no operation is needed, just forward input 0
	if (bNoOperationNeeded)
	{
		for (uint32 OutputIndex = 0; OutputIndex < Settings->GetOutputPinNum(); ++OutputIndex)
		{
			FPCGTaggedData& OutputData = Outputs.Add_GetRef(InputTaggedData[0]);
			OutputData.Pin = Settings->GetOutputPinLabel(OutputIndex);
		}

		return true;
	}


	// At this point, we verified everything, so we can go forward with the computation, depending on the most complex type
	// So first forward outputs and create the attribute
	OperationData.OutputAccessors.SetNum(Settings->GetOutputPinNum());
	OperationData.OutputKeys.SetNum(Settings->GetOutputPinNum());

	FPCGAttributePropertySelector OutputTarget = Settings->OutputTarget;

	if (OutputTarget.Selection == EPCGAttributePropertySelection::Attribute && OutputTarget.AttributeName == NAME_None)
	{
		OutputTarget.AttributeName = InputName;
	}
	
	// TODO: Make it an option
	const int32 InputToForward = 0;

	// Use implicit capture, since we capture a lot
	auto CreateAttribute = [&](uint32 OutputIndex, auto DummyOutValue) -> bool
	{
		using AttributeType = decltype(DummyOutValue);

		FPCGTaggedData& OutputData = Outputs.Add_GetRef(InputTaggedData[InputToForward]);
		OutputData.Pin = Settings->GetOutputPinLabel(OutputIndex);

		UPCGMetadata* OutMetadata = nullptr;

		FName OutputName = OutputTarget.GetName();

		if (OutputTarget.Selection == EPCGAttributePropertySelection::Attribute && OutputTarget.ExtraNames.IsEmpty())
		{
			// In case of an attribute, we check if we have extra selectors. If not, we can just delete the attribute
			// and create a new one of the right type.
			// But if we have extra selectors, we need to handle it the same way as properties.
			// There is no point of failure before duplicating. So duplicate, create the attribute and then the accessor.
			PCGMetadataElementCommon::DuplicateTaggedData(InputTaggedData[InputToForward], OutputData, OutMetadata);
			FPCGMetadataAttributeBase* OutputAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute(OutMetadata, OutputName, AttributeType{});
			if (!OutputAttribute)
			{
				return false;
			}

			// And copy the mapping from the original attribute, if it is not points
			if (!InputTaggedData[InputToForward].Data->IsA<UPCGPointData>() && SourceMetadata[InputToForward] && SourceAttribute[InputToForward])
			{
				PCGMetadataElementCommon::CopyEntryToValueKeyMap(SourceMetadata[InputToForward], SourceAttribute[InputToForward], OutputAttribute);
			}

			OperationData.OutputAccessors[OutputIndex] = PCGAttributeAccessorHelpers::CreateAccessor(Cast<UPCGData>(OutputData.Data), OutputTarget);
		}
		else
		{
			// In case of property or attribute with extra accessor, we need to validate that the property/attribute can accept the output type.
			// Verify this before duplicating.
			OperationData.OutputAccessors[OutputIndex] = PCGAttributeAccessorHelpers::CreateAccessor(Cast<UPCGData>(OutputData.Data), OutputTarget);

			if (OperationData.OutputAccessors[OutputIndex].IsValid())
			{
				// We matched an attribute/property, check if the output type is valid.
				if (!PCG::Private::IsBroadcastable(PCG::Private::MetadataTypes<AttributeType>::Id, OperationData.OutputAccessors[OutputIndex]->GetUnderlyingType()))
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeTypeBroadcastFailed", "Attribute/Property '{0}' cannot be broadcasted to match types for input"), FText::FromName(OutputName)));
					return false;
				}

				PCGMetadataElementCommon::DuplicateTaggedData(InputTaggedData[InputToForward], OutputData, OutMetadata);

				// Re-create the accessor to point to the right data (since we just duplicated the data)
				OperationData.OutputAccessors[OutputIndex] = PCGAttributeAccessorHelpers::CreateAccessor(Cast<UPCGData>(OutputData.Data), OutputTarget);
			}
		}

		if (!OperationData.OutputAccessors[OutputIndex].IsValid())
		{
			return false;
		}

		OperationData.OutputKeys[OutputIndex] = PCGAttributeAccessorHelpers::CreateKeys(Cast<UPCGData>(OutputData.Data), OutputTarget);

		return OperationData.OutputKeys[OutputIndex].IsValid();
	};

	auto CreateAllSameAttributes = [NumberOfOutputs, &CreateAttribute](auto DummyOutValue) -> bool
	{
		for (uint32 i = 0; i < NumberOfOutputs; ++i)
		{
			if (!CreateAttribute(i, DummyOutValue))
			{
				return false;
			}
		}

		return true;
	};

	OperationData.OutputType = Settings->GetOutputType(OperationData.MostComplexInputType);

	bool bCreateAttributeSucceeded = true;

	if (!Settings->HasDifferentOutputTypes())
	{
		bCreateAttributeSucceeded = PCGMetadataAttribute::CallbackWithRightType(OperationData.OutputType, CreateAllSameAttributes);
	}
	else
	{
		TArray<uint16> OutputTypes = Settings->GetAllOutputTypes();
		check(OutputTypes.Num() == NumberOfOutputs);

		for (uint32 i = 0; i < NumberOfOutputs && bCreateAttributeSucceeded; ++i)
		{
			bCreateAttributeSucceeded &= PCGMetadataAttribute::CallbackWithRightType(OutputTypes[i],
				[&CreateAttribute, i](auto DummyOutValue) -> bool 
				{
					return CreateAttribute(i, DummyOutValue);
				});
		}
	}

	if (!bCreateAttributeSucceeded)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("ErrorCreatingOutputAttributes", "Error while creating output attributes"));
		Outputs.Empty();
		return true;
	}

	OperationData.Settings = Settings;

	if (!DoOperation(OperationData))
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("ErrorOccurred", "Error while performing the metadata operation, check logs for more information"));
		Outputs.Empty();
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
