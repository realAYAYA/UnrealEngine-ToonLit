// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataOpElementBase)

#define LOCTEXT_NAMESPACE "PCGMetadataElementBaseElement"

void UPCGMetadataSettingsBase::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (OutputAttributeName_DEPRECATED != NAME_None)
	{
		OutputTarget.SetAttributeName(OutputAttributeName_DEPRECATED);
		OutputAttributeName_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

EPCGDataType UPCGMetadataSettingsBase::GetInputPinType(uint32 Index) const
{
	const EPCGDataType FirstPinTypeUnion = GetTypeUnionOfIncidentEdges(GetInputPinLabel(Index));

	// If the pin is not connected but support default value, treat it as a param
	if (FirstPinTypeUnion == EPCGDataType::None && DoesInputSupportDefaultValue(Index))
	{
		return EPCGDataType::Param;
	}

	return FirstPinTypeUnion != EPCGDataType::None ? FirstPinTypeUnion : EPCGDataType::Any;
}

TArray<FName> UPCGMetadataSettingsBase::GetOutputDataFromPinOptions() const
{
	const uint32 NumInputs = GetInputPinNum();

	TArray<FName> AllOptions;
	AllOptions.SetNum(NumInputs + 1);
	AllOptions[0] = PCGMetadataSettingsBaseConstants::DefaultOutputDataFromPinName;

	for (uint32 Index = 0; Index < NumInputs; ++Index)
	{
		AllOptions[Index + 1] = GetInputPinLabel(Index);
	}

	return AllOptions;
}

uint32 UPCGMetadataSettingsBase::GetInputPinIndex(FName InPinLabel) const
{
	if (InPinLabel != PCGMetadataSettingsBaseConstants::DefaultOutputDataFromPinName)
	{
		for (uint32 Index = 0; Index < GetInputPinNum(); ++Index)
		{
			if (InPinLabel == GetInputPinLabel(Index))
			{
				return Index;
			}
		}
	}

	return (uint32)INDEX_NONE;
}

uint32 UPCGMetadataSettingsBase::GetInputPinToForward() const
{
	const uint32 NumberOfInputs = GetInputPinNum();
	uint32 InputPinToForward = 0;

	// If there is only one input, it is trivial
	if (NumberOfInputs != 1)
	{
		// Heuristic:
		//	* If OutputDataFromPin is set, use this value
		//	* If there are connected pins, use the first spatial input (not Any)
		//	* Otherwise, take the first pin
		const uint32 OutputDataFromPinIndex = GetInputPinIndex(OutputDataFromPin);
		if (OutputDataFromPinIndex != (uint32)INDEX_NONE)
		{
			InputPinToForward = OutputDataFromPinIndex;
		}
		else
		{
			for (uint32 InputPinIndex = 0; InputPinIndex < NumberOfInputs; ++InputPinIndex)
			{
				const EPCGDataType PinType = GetInputPinType(InputPinIndex);

				if ((PinType != EPCGDataType::Any) && !!(PinType & EPCGDataType::Spatial))
				{
					InputPinToForward = InputPinIndex;
					break;
				}
			}
		}
	}

	return InputPinToForward;
}

EPCGDataType UPCGMetadataSettingsBase::GetCurrentPinTypes(const UPCGPin* InPin) const
{
	check(InPin);
	const uint32 NumberOfInputs = GetInputPinNum();
	if (!InPin->IsOutputPin() || NumberOfInputs == 0)
	{
		// Fall back to default for input pins, or if no input pins present from which to obtain type
		return Super::GetCurrentPinTypes(InPin);
	}

	// Output pin narrows to union of inputs on pin to forward
	return GetInputPinType(GetInputPinToForward());
}

#if WITH_EDITOR
void UPCGMetadataSettingsBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Make sure the output data from pin value is always valid. Reset it otherwise.
	if (GetInputPinIndex(OutputDataFromPin) == (uint32)INDEX_NONE)
	{
		OutputDataFromPin = PCGMetadataSettingsBaseConstants::DefaultOutputDataFromPinName;
	}
}

bool UPCGMetadataSettingsBase::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	return InProperty && ((InProperty->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGMetadataSettingsBase, OutputDataFromPin)) || (GetInputPinNum() != 1));
}

bool UPCGMetadataSettingsBase::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const
{
	// Only set the arrow if the output data from pin is forced.
	if (GetInputPinNum() > 1 && InPin && OutputDataFromPin == InPin->Properties.Label)
	{
		OutExtraIcon = TEXT("Icons.ArrowRight");
		return true;
	}
	else
	{
		return false;
	}
}

FText UPCGMetadataSettingsBase::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Metadata operation between Points/Spatial/AttributeSet data.\n"
		"Output data will be taken from the first spatial data by default, or first pin if all are attribute sets.\n"
		"It can be overridden in the settings.");
}

void UPCGMetadataSettingsBase::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (DataVersion < FPCGCustomVersion::UpdateAttributePropertyInputSelector
		&& OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute
		&& OutputTarget.GetAttributeName() == PCGMetadataAttributeConstants::SourceAttributeName)
	{
		// Previous behavior of the output target for this node was:
		// - If the input to forward is an attribute -> SourceName
		// - If the input to forward was not an attribute -> None
		const FPCGAttributePropertyInputSelector& InputSource = GetInputSource(GetInputPinToForward());
		if (InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			OutputTarget.SetAttributeName(PCGMetadataAttributeConstants::SourceNameAttributeName);
		}
		else
		{
			OutputTarget.SetAttributeName(NAME_None);
		}
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGMetadataSettingsBase::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	for (uint32 InputPinIndex = 0; InputPinIndex < GetInputPinNum(); ++InputPinIndex)
	{
		const FName PinLabel = GetInputPinLabel(InputPinIndex);
		if (PinLabel != NAME_None)
		{
			PinProperties.Emplace(PinLabel, EPCGDataType::Any, /*bAllowMultipleConnections=*/false);

#if WITH_EDITOR
			TArray<FText> AllTooltips;
			TArray<FString> SupportedTypes;

			for (uint8 TypeId = 0; TypeId < (uint8)EPCGMetadataTypes::Count; ++TypeId)
			{
				bool DummyBool;
				if (IsSupportedInputType(TypeId, InputPinIndex, DummyBool))
				{
					SupportedTypes.Add(PCG::Private::GetTypeName(TypeId));
				}
			}
			
			if (!SupportedTypes.IsEmpty())
			{
				AllTooltips.Add(FText::Format(LOCTEXT("PinTooltipSupportedTypes", "Supported types: {0}"), FText::FromString(FString::Join(SupportedTypes, TEXT(", ")))));
			}

			if (GetInputPinNum() > 1 && OutputDataFromPin == PinLabel)
			{
				AllTooltips.Add(LOCTEXT("PinTooltipForwardInput", "This input will be forwarded to the output."));
			}

			if (DoesInputSupportDefaultValue(InputPinIndex))
			{
				AllTooltips.Add(FText::Format(LOCTEXT("PinTooltipDefaultValue", "Pin is optional, will use default value if not connected ({0})"), FText::FromString(GetDefaultValueString(InputPinIndex))));
			}

			if (!AllTooltips.IsEmpty())
			{
				PinProperties.Last().Tooltip = FText::Join(FText::FromString(TEXT("\n")), AllTooltips);
			}
#endif // WITH_EDITOR
		}
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMetadataSettingsBase::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	
	for (uint32 OutputPinIndex = 0; OutputPinIndex < GetOutputPinNum(); ++OutputPinIndex)
	{
		const FName PinLabel = GetOutputPinLabel(OutputPinIndex);
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

	uint32 InputPinToForward = Settings->GetInputPinToForward();

	for (uint32 InputPinIndex = 0; InputPinIndex < NumberOfInputs; ++InputPinIndex)
	{
		FName PinLabel = Settings->GetInputPinLabel(InputPinIndex);
		TArray<FPCGTaggedData> InputData = Context->InputData.GetInputsByPin(PinLabel);

		// If input data is empty but we can have default values, add a "fake" param data input.
		if (InputData.IsEmpty() && Settings->DoesInputSupportDefaultValue(InputPinIndex))
		{
			FPCGTaggedData& DummyData = InputData.Emplace_GetRef();
			DummyData.Pin = PinLabel;
			DummyData.Data = Settings->CreateDefaultValueParam(InputPinIndex);
			if (!DummyData.Data)
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CantCreateDefaultValue", "Pin {0} supports default value but we could not create it."), FText::FromName(PinLabel)));
				return true;
			}
		}

		if (InputData.IsEmpty())
		{
			// Visually warn the user, since this is causing execution to be aborted
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("MissingInputDataForPin", "No data provided on pin {0}"), FText::FromName(PinLabel)));
			return true;
		}
		else if (InputData.Num() > 1)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("TooMuchDataForPin", "Too many data items ({0}) provided on pin {1}"), InputData.Num(), FText::FromName(PinLabel)));
			return true;
		}

		// By construction, there can only be one of then(hence the 0 index)
		InputTaggedData[InputPinIndex] = MoveTemp(InputData[0]);

		// Only gather Spacial and Params input. 
		if (const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(InputTaggedData[InputPinIndex].Data))
		{
			SourceMetadata[InputPinIndex] = SpatialInput->Metadata;
		}
		else if (const UPCGParamData* ParamsInput = Cast<const UPCGParamData>(InputTaggedData[InputPinIndex].Data))
		{
			SourceMetadata[InputPinIndex] = ParamsInput->Metadata;
		}
		else
		{
			// Since this aborts execution, and the user can fix it, it should be a node error
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidInputDataTypeForPin", "Invalid data provided on pin {0}, must be of type Spatial or Attribute Set"), FText::FromName(PinLabel)));
			return true;
		}
	}

	FOperationData OperationData;
	OperationData.InputAccessors.SetNum(NumberOfInputs);
	OperationData.InputKeys.SetNum(NumberOfInputs);
	OperationData.InputSources.SetNum(NumberOfInputs);
	TArray<int32> NumberOfElements;
	NumberOfElements.SetNum(NumberOfInputs);

	OperationData.MostComplexInputType = (uint16)EPCGMetadataTypes::Unknown;
	OperationData.NumberOfElementsToProcess = -1;

	bool bNoOperationNeeded = false;

	auto CreateInputAccessorAndValidate = [this, Settings, InputPinToForward, &InputTaggedData, Context, &SourceMetadata, &OperationData, &NumberOfElements, &bNoOperationNeeded, &SourceAttribute](uint32 Index) -> bool
	{
		// First we verify if the input data match the first one. If the pin to forward is not connected, behave like a param data
		const UClass* InputPinToForwardClass = (InputTaggedData[InputPinToForward].Data ? InputTaggedData[InputPinToForward].Data->GetClass() : UPCGParamData::StaticClass());

		if (Index != InputPinToForward && 
			InputPinToForwardClass != InputTaggedData[Index].Data->GetClass() &&
			!InputTaggedData[Index].Data->IsA<UPCGParamData>())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InputTypeMismatch", "Data on pin {0} is not of the same type than on pin {1} and is not an Attribute Set. This is not supported."), FText::FromName(InputTaggedData[Index].Pin), FText::FromName(InputTaggedData[InputPinToForward].Pin)));
			return false;
		}

		OperationData.InputSources[Index] = Settings->GetInputSource(Index).CopyAndFixLast(InputTaggedData[Index].Data);
		const FPCGAttributePropertyInputSelector& InputSource = OperationData.InputSources[Index];

		OperationData.InputAccessors[Index] = PCGAttributeAccessorHelpers::CreateConstAccessor(InputTaggedData[Index].Data, InputSource);
		OperationData.InputKeys[Index] = PCGAttributeAccessorHelpers::CreateConstKeys(InputTaggedData[Index].Data, InputSource);

		if (!OperationData.InputAccessors[Index].IsValid() || !OperationData.InputKeys[Index].IsValid())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeDoesNotExist", "Attribute/Property '{0}' from pin {1} does not exist"), FText::FromName(InputSource.GetName()), FText::FromName(InputTaggedData[Index].Pin)));
			return false;
		}

		uint16 AttributeTypeId = OperationData.InputAccessors[Index]->GetUnderlyingType();

		// Then verify that the type is OK
		bool bHasSpecialRequirement = false;
		if (!Settings->IsSupportedInputType(AttributeTypeId, Index, bHasSpecialRequirement))
		{
			FText AttributeTypeName = FText::FromString(PCG::Private::GetTypeName(AttributeTypeId));
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("UnsupportedAttributeType", "Attribute/Property '{0}' from pin {1} is not a supported type ('{2}')"), 
				FText::FromName(InputSource.GetName()), 
				FText::FromName(InputTaggedData[Index].Pin),
				AttributeTypeName
			));
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
				FText AttributeTypeName = FText::FromString(PCG::Private::GetTypeName(AttributeTypeId));
				FText MostComplexTypeName = FText::FromString(PCG::Private::GetTypeName(OperationData.MostComplexInputType));
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeCannotBeBroadcasted", "Attribute '{0}' (from pin {1}) of type '{2}' cannot be used for operation with type '{3}'"), 
					FText::FromName(InputSource.GetName()), 
					FText::FromName(InputTaggedData[Index].Pin),
					AttributeTypeName,
					MostComplexTypeName));
				return true;
			}
		}

		NumberOfElements[Index] = OperationData.InputKeys[Index]->GetNum();

		if (Index == InputPinToForward)
		{
			OperationData.NumberOfElementsToProcess = NumberOfElements[Index];
		}

		// There is nothing to do if one input doesn't have any element to process.
		// Therefore mark that we have nothing to do and early out.
		if (NumberOfElements[Index] == 0)
		{
			PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("NoElementsInInput", "No elements in data from pin {0}"), FText::FromName(InputTaggedData[Index].Pin)));
			bNoOperationNeeded = true;
			return true;
		}

		// Verify that the number of elements makes sense
		if (OperationData.NumberOfElementsToProcess % NumberOfElements[Index] != 0)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("MismatchInNumberOfElements", "Mismatch between the number of elements from pin {0} ({1}) and from pin {2} ({3})"), FText::FromName(InputTaggedData[InputPinToForward].Pin), OperationData.NumberOfElementsToProcess, FText::FromName(InputTaggedData[Index].Pin), NumberOfElements[Index]));
			return false;
		}

		if (InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			SourceAttribute[Index] = SourceMetadata[Index]->GetConstAttribute(InputSource.GetName());
		}
		else
		{
			SourceAttribute[Index] = nullptr;
		}

		return true;
	};

	// First do it for the input to forward (it's our control data)
	if (!CreateInputAccessorAndValidate(InputPinToForward))
	{
		return true;
	}

	for (uint32 i = 0; i < NumberOfInputs && !bNoOperationNeeded; ++i)
	{
		if (i != InputPinToForward && !CreateInputAccessorAndValidate(i))
		{
			return true;
		}
	}

	// If no operation is needed, just forward input 
	if (bNoOperationNeeded)
	{
		for (uint32 OutputIndex = 0; OutputIndex < Settings->GetOutputPinNum(); ++OutputIndex)
		{
			FPCGTaggedData& OutputData = Outputs.Add_GetRef(InputTaggedData[InputPinToForward]);
			OutputData.Pin = Settings->GetOutputPinLabel(OutputIndex);
		}

		return true;
	}


	// At this point, we verified everything, so we can go forward with the computation, depending on the most complex type
	// So first forward outputs and create the attribute
	OperationData.OutputAccessors.SetNum(Settings->GetOutputPinNum());
	OperationData.OutputKeys.SetNum(Settings->GetOutputPinNum());

	FPCGAttributePropertyOutputSelector OutputTarget = Settings->OutputTarget.CopyAndFixSource(&OperationData.InputSources[InputPinToForward]);

	// Use implicit capture, since we capture a lot
	auto CreateAttribute = [&](uint32 OutputIndex, auto DummyOutValue) -> bool
	{
		using AttributeType = decltype(DummyOutValue);

		FPCGTaggedData& OutputData = Outputs.Add_GetRef(InputTaggedData[InputPinToForward]);
		OutputData.Pin = Settings->GetOutputPinLabel(OutputIndex);

		UPCGMetadata* OutMetadata = nullptr;

		FName OutputName = OutputTarget.GetName();

		if (OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute && OutputTarget.GetExtraNames().IsEmpty())
		{
			// In case of an attribute, we check if we have extra selectors. If not, we can just delete the attribute
			// and create a new one of the right type.
			// But if we have extra selectors, we need to handle it the same way as properties.
			// There is no point of failure before duplicating. So duplicate, create the attribute and then the accessor.
			PCGMetadataElementCommon::DuplicateTaggedData(InputTaggedData[InputPinToForward], OutputData, OutMetadata);
			FPCGMetadataAttributeBase* OutputAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute(OutMetadata, OutputName, AttributeType{});
			if (!OutputAttribute)
			{
				return false;
			}

			// And copy the mapping from the original attribute, if it is not points
			if (!InputTaggedData[InputPinToForward].Data->IsA<UPCGPointData>() && SourceMetadata[InputPinToForward] && SourceAttribute[InputPinToForward])
			{
				PCGMetadataElementCommon::CopyEntryToValueKeyMap(SourceMetadata[InputPinToForward], SourceAttribute[InputPinToForward], OutputAttribute);
			}

			OperationData.OutputAccessors[OutputIndex] = PCGAttributeAccessorHelpers::CreateAccessor(Cast<UPCGData>(OutputData.Data), OutputTarget);
		}
		else
		{
			// In case of property or attribute with extra accessor, we need to validate that the property/attribute can accept the output type.
			// Verify this before duplicating, because an extra allocation is certainly less costly than duplicating the data.
			// Do it with a const accessor, since OutputData.Data is still pointing on the const input data.
			TUniquePtr<const IPCGAttributeAccessor> TempConstAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(Cast<const UPCGData>(OutputData.Data), OutputTarget);

			if (TempConstAccessor.IsValid())
			{
				// We matched an attribute/property, check if the output type is valid.
				if (!PCG::Private::IsBroadcastable(PCG::Private::MetadataTypes<AttributeType>::Id, TempConstAccessor->GetUnderlyingType()))
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeTypeBroadcastFailed", "Attribute/Property '{0}' cannot be broadcasted to match types for input"), FText::FromName(OutputName)));
					return false;
				}

				PCGMetadataElementCommon::DuplicateTaggedData(InputTaggedData[InputPinToForward], OutputData, OutMetadata);

				// Re-create the accessor to point to the right data (since we just duplicated the data)
				OperationData.OutputAccessors[OutputIndex] = PCGAttributeAccessorHelpers::CreateAccessor(Cast<UPCGData>(OutputData.Data), OutputTarget);
			}
		}

		if (!OperationData.OutputAccessors[OutputIndex].IsValid())
		{
			return false;
		}

		if (OperationData.OutputAccessors[OutputIndex]->IsReadOnly())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("OutputAccessorIsReadOnly", "Attribute/Property '{0}' is read only."), OutputTarget.GetDisplayText()));
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
