// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
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
	const uint32 OperandNum = GetOperandNum();

	TArray<FName> AllOptions;
	AllOptions.SetNum(OperandNum + 1);
	AllOptions[0] = PCGMetadataSettingsBaseConstants::DefaultOutputDataFromPinName;

	for (uint32 Index = 0; Index < OperandNum; ++Index)
	{
		AllOptions[Index + 1] = GetInputPinLabel(Index);
	}

	return AllOptions;
}

uint32 UPCGMetadataSettingsBase::GetInputPinIndex(FName InPinLabel) const
{
	if (InPinLabel != PCGMetadataSettingsBaseConstants::DefaultOutputDataFromPinName)
	{
		for (uint32 Index = 0; Index < GetOperandNum(); ++Index)
		{
			if (InPinLabel == GetInputPinLabel(Index))
			{
				return Index;
			}
		}
	}

	return static_cast<uint32>(INDEX_NONE);
}

uint32 UPCGMetadataSettingsBase::GetInputPinToForward() const
{
	const uint32 OperandNum = GetOperandNum();
	uint32 InputPinToForward = 0;

	// If there is only one input, it is trivial
	if (OperandNum != 1)
	{
		// Heuristic:
		//	* If OutputDataFromPin is set, use this value
		//	* If there are connected pins, use the first spatial input (not Any)
		//	* Otherwise, take the first pin
		const uint32 OutputDataFromPinIndex = GetInputPinIndex(OutputDataFromPin);
		if (OutputDataFromPinIndex != static_cast<uint32>(INDEX_NONE))
		{
			InputPinToForward = OutputDataFromPinIndex;
		}
		else
		{
			for (uint32 InputPinIndex = 0; InputPinIndex < OperandNum; ++InputPinIndex)
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
	const uint32 OperandNum = GetOperandNum();
	if (!InPin->IsOutputPin() || OperandNum == 0)
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
	if (GetInputPinIndex(OutputDataFromPin) == static_cast<uint32>(INDEX_NONE))
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

	return InProperty && ((InProperty->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGMetadataSettingsBase, OutputDataFromPin)) || (GetOperandNum() != 1));
}

bool UPCGMetadataSettingsBase::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const
{
	// Only set the arrow if the output data from pin is forced.
	if (GetOperandNum() > 1 && InPin && OutputDataFromPin == InPin->Properties.Label)
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

bool UPCGMetadataSettingsBase::DoesPinSupportPassThrough(UPCGPin* InPin) const
{
	return InPin && !InPin->IsOutputPin() && GetInputPinIndex(InPin->Properties.Label) == GetInputPinToForward();
}

TArray<FPCGPinProperties> UPCGMetadataSettingsBase::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	for (uint32 InputPinIndex = 0; InputPinIndex < GetOperandNum(); ++InputPinIndex)
	{
		const FName PinLabel = GetInputPinLabel(InputPinIndex);
		if (PinLabel != NAME_None)
		{
			FPCGPinProperties& PinProperty = PinProperties.Emplace_GetRef(PinLabel, EPCGDataType::Any);

			const bool bSupportDefaultValue = DoesInputSupportDefaultValue(InputPinIndex);
			if (!bSupportDefaultValue)
			{
				PinProperty.SetRequiredPin();
			}

#if WITH_EDITOR
			TArray<FText> AllTooltips;
			TArray<FString> SupportedTypes;

			for (uint8 TypeId = 0; TypeId < static_cast<uint8>(EPCGMetadataTypes::Count); ++TypeId)
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

			if (GetOperandNum() > 1 && OutputDataFromPin == PinLabel)
			{
				AllTooltips.Add(LOCTEXT("PinTooltipForwardInput", "This input will be forwarded to the output."));
			}

			if (bSupportDefaultValue)
			{
				AllTooltips.Add(FText::Format(LOCTEXT("PinTooltipDefaultValue", "Pin is optional, will use default value if not connected ({0})"), FText::FromString(GetDefaultValueString(InputPinIndex))));
			}

			if (!AllTooltips.IsEmpty())
			{
				PinProperty.Tooltip = FText::Join(FText::FromString(TEXT("\n")), AllTooltips);
			}
#endif // WITH_EDITOR
		}
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMetadataSettingsBase::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	for (uint32 OutputPinIndex = 0; OutputPinIndex < GetResultNum(); ++OutputPinIndex)
	{
		const FName PinLabel = GetOutputPinLabel(OutputPinIndex);
		if (PinLabel != NAME_None)
		{
			PinProperties.Emplace(PinLabel, EPCGDataType::Any);
		}
	}

	return PinProperties;
}

void FPCGMetadataElementBase::PassthroughInput(FPCGContext* Context, TArray<FPCGTaggedData>& Outputs, const int32 Index) const
{
	check(Context);

	const UPCGMetadataSettingsBase* Settings = Context->GetInputSettings<UPCGMetadataSettingsBase>();
	check(Settings);

	const uint32 NumberOfOutputs = Settings->GetResultNum();
	const uint32 PrimaryPinIndex = Settings->GetInputPinToForward();
	TArray<FPCGTaggedData> InputsToForward = Context->InputData.GetInputsByPin(Settings->GetInputPinLabel(PrimaryPinIndex));

	if (InputsToForward.IsEmpty())
	{
		return;
	}

	// Take the index of the iteration, except for the 1:N case, where we just grab the first index
	const int32 AdjustedIndex = (Index <= InputsToForward.Num()) ? Index : 0;

	// Passthrough this single input to all of the outputs
	for (uint32 I = 0; I < NumberOfOutputs; ++I)
	{
		Outputs.Emplace_GetRef(InputsToForward[AdjustedIndex]).Pin = Settings->GetOutputPinLabel(I);
	}
}

namespace PCGMetadataOpPrivate
{
	using ContextType = FPCGMetadataElementBase::ContextType;
	using ExecStateType = FPCGMetadataElementBase::ExecStateType;

	void CreateAccessor(const UPCGMetadataSettingsBase* Settings, const FPCGTaggedData& InputData, PCGMetadataOps::FOperationData& OperationData, const int32 Index)
	{
		OperationData.InputSources[Index] = Settings->GetInputSource(Index).CopyAndFixLast(InputData.Data);
		const FPCGAttributePropertyInputSelector& InputSource = OperationData.InputSources[Index];

		OperationData.InputAccessors[Index] = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData.Data, InputSource);
		OperationData.InputKeys[Index] = PCGAttributeAccessorHelpers::CreateConstKeys(InputData.Data, InputSource);
	}

	bool ValidateAccessor(const FPCGContext* Context, const UPCGMetadataSettingsBase* Settings, const FPCGTaggedData& InputData, PCGMetadataOps::FOperationData& OperationData, int32 Index)
	{
		const FPCGAttributePropertyInputSelector& InputSource = OperationData.InputSources[Index];
		const FText InputSourceText = InputSource.GetDisplayText();

		if (!OperationData.InputAccessors[Index].IsValid() || !OperationData.InputKeys[Index].IsValid())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("AttributeDoesNotExist", "Attribute/Property '{0}' from pin {1} does not exist"), InputSourceText, FText::FromName(InputData.Pin)));
			return false;
		}

		const uint16 AttributeTypeId = OperationData.InputAccessors[Index]->GetUnderlyingType();

		// Then verify that the type is OK
		bool bHasSpecialRequirement = false;
		if (!Settings->IsSupportedInputType(AttributeTypeId, Index, bHasSpecialRequirement))
		{
			const FText AttributeTypeName = PCG::Private::GetTypeNameText(AttributeTypeId);
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("UnsupportedAttributeType", "Attribute/Property '{0}' from pin {1} is not a supported type ('{2}')"),
				InputSourceText,
				FText::FromName(InputData.Pin),
				AttributeTypeName));
			return false;
		}

		if (!bHasSpecialRequirement)
		{
			// In this case, check if we have a more complex type, or if we can broadcast to the most complex type.
			if (OperationData.MostComplexInputType == static_cast<uint16>(EPCGMetadataTypes::Unknown) || PCG::Private::IsMoreComplexType(AttributeTypeId, OperationData.MostComplexInputType))
			{
				OperationData.MostComplexInputType = AttributeTypeId;
			}
			else if (OperationData.MostComplexInputType != AttributeTypeId && !PCG::Private::IsBroadcastable(AttributeTypeId, OperationData.MostComplexInputType))
			{
				const FText AttributeTypeName = PCG::Private::GetTypeNameText(AttributeTypeId);
				const FText MostComplexTypeName = PCG::Private::GetTypeNameText(OperationData.MostComplexInputType);
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("AttributeCannotBeBroadcasted", "Attribute '{0}' (from pin {1}) of type '{2}' cannot be used for operation with type '{3}'"),
					InputSourceText,
					FText::FromName(InputData.Pin),
					AttributeTypeName,
					MostComplexTypeName));
				return false;
			}
		}

		return true;
	}

	bool ValidateSecondaryInputClassMatches(const FPCGTaggedData& PrimaryInputData, const FPCGTaggedData& SecondaryInputData)
	{
		// First, verify the input data matches the primary. If the pin to forward is not connected, behave like a param data
		const UClass* InputPinToForwardClass = (PrimaryInputData.Data ? PrimaryInputData.Data->GetClass() : UPCGParamData::StaticClass());

		// TODO: Consider updating this to check if its a child class instead to be more future proof. For now this is good.
		// Check for data mismatch between primary pin and current pin
		if (InputPinToForwardClass != SecondaryInputData.Data->GetClass() && !SecondaryInputData.Data->IsA<UPCGParamData>())
		{
			return false;
		}

		return true;
	}
}

bool FPCGMetadataElementBase::PrepareDataInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataElementBase::PrepareDataInternal);

	FPCGMetadataElementBase::ContextType* TimeSlicedContext = static_cast<FPCGMetadataElementBase::ContextType*>(Context);
	check(TimeSlicedContext);

	const UPCGMetadataSettingsBase* Settings = Context->GetInputSettings<UPCGMetadataSettingsBase>();
	check(Settings);

	const uint32 OperandNum = Settings->GetOperandNum();
	const uint32 ResultNum = Settings->GetResultNum();

	check(OperandNum > 0);
	check(ResultNum <= UPCGMetadataSettingsBase::MaxNumberOfOutputs);

	const FName PrimaryPinLabel = Settings->GetInputPinLabel(Settings->GetInputPinToForward());
	const TArray<FPCGTaggedData> PrimaryInputs = Context->InputData.GetInputsByPin(PrimaryPinLabel);

	// There are no inputs on the primary pin, so pass-through inputs if the primary pin is required
	if (Settings->IsPrimaryInputPinRequired() && PrimaryInputs.IsEmpty())
	{
		return true;
	}

	int32 OperandInputNumMax = 0;

	// There's no execution state, so just flag that it is ready to continue
	TimeSlicedContext->InitializePerExecutionState([this, Settings, &OperandInputNumMax, OperandNum](ContextType* Context, PCGTimeSlice::FEmptyStruct& OutState) -> EPCGTimeSliceInitResult
	{
		for (uint32 i = 0; i < OperandNum; ++i)
		{
			const int32 CurrentInputNum = Context->InputData.GetInputsByPin(Settings->GetInputPinLabel(i)).Num();

			OperandInputNumMax = FMath::Max(OperandInputNumMax, CurrentInputNum);

			// For the current input, no input (0) could be default value and we support N:1 and 1:N
			if (CurrentInputNum > 1 && CurrentInputNum != OperandInputNumMax)
			{
				PCGE_LOG(Error, GraphAndLog, LOCTEXT("MismatchedOperandDataCount", "Number of data elements provided on inputs must be 1:N, N:1, or N:N."));
				return EPCGTimeSliceInitResult::AbortExecution;
			}
		}

		return EPCGTimeSliceInitResult::Success;
	});

	// Set up the iterations on the multiple inputs of the primary pin
	TimeSlicedContext->InitializePerIterationStates(OperandInputNumMax, [this, Context, OperandNum, Settings, OperandInputNumMax](IterStateType& OutState, const ExecStateType& ExecState, const uint32 IterationIndex)
	{
		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		const uint32 NumberOfResults = Settings->GetResultNum();

		// Gathering all the inputs metadata
		TArray<const UPCGMetadata*> SourceMetadata;
		TArray<const FPCGMetadataAttributeBase*> SourceAttribute;
		TArray<FPCGTaggedData> InputTaggedData;
		SourceMetadata.SetNum(OperandNum);
		SourceAttribute.SetNum(OperandNum);
		InputTaggedData.SetNum(OperandNum);

		const uint32 PrimaryPinIndex = Settings->GetInputPinToForward();

		// Iterate over the inputs and validate
		for (uint32 OperandPinIndex = 0; OperandPinIndex < OperandNum; ++OperandPinIndex)
		{
			const FName CurrentPinLabel = Settings->GetInputPinLabel(OperandPinIndex);
			const bool bIsInputConnected = Context->Node ? Context->Node->IsInputPinConnected(CurrentPinLabel) : false;
			TArray<FPCGTaggedData> CurrentPinInputData = Context->InputData.GetInputsByPin(CurrentPinLabel);

			// This only needs to be checked once
			if (!bIsInputConnected && CurrentPinInputData.IsEmpty())
			{
				FPCGTaggedData& DefaultData = CurrentPinInputData.Emplace_GetRef();
				DefaultData.Pin = CurrentPinLabel;
				DefaultData.Data = Settings->CreateDefaultValueParam(OperandPinIndex);

				if (!DefaultData.Data)
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CantCreateDefaultValue", "Pin '{0}' supports default value but we could not create it."), FText::FromName(CurrentPinLabel)));
					return EPCGTimeSliceInitResult::AbortExecution;
				}
				else
				{
					// Need to make sure the param data has at least one entry
					UPCGMetadata* DefaultParamMetadata = CastChecked<UPCGParamData>(DefaultData.Data)->Metadata;
					if (DefaultParamMetadata->GetLocalItemCount() == 0)
					{
						DefaultParamMetadata->AddEntry();
					}
				}
			}

			if (CurrentPinInputData.IsEmpty())
			{
				// If we have no data, there is no operation
				PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("MissingInputDataForPin", "No data provided on pin '{0}'."), FText::FromName(CurrentPinLabel)));
				return EPCGTimeSliceInitResult::NoOperation;
			}
			else if (CurrentPinInputData.Num() != 1 && CurrentPinInputData.Num() != OperandInputNumMax)
			{
				PCGE_LOG(Error, GraphAndLog,
					FText::Format(LOCTEXT("MismatchedDataCountForPin", "Number of data elements ({0}) provided on pin '{1}' doesn't match number of expected elements ({2}). Only 1 input or {2} are supported."),
						CurrentPinInputData.Num(),
						FText::FromName(CurrentPinLabel),
						OperandInputNumMax));
				return EPCGTimeSliceInitResult::AbortExecution;
			}

			// The operand inputs must either be N:1 or N:N or 1:N
			InputTaggedData[OperandPinIndex] = CurrentPinInputData.Num() == 1 ? CurrentPinInputData[0] : MoveTemp(CurrentPinInputData[IterationIndex]);

			// Check if we have any points
			if (const UPCGPointData* PointInput = Cast<const UPCGPointData>(InputTaggedData[OperandPinIndex].Data))
			{
				if (PointInput->GetPoints().IsEmpty())
				{
					// If we have no points, there is no operation
					PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("NoPointsForPin", "No points in point data provided on pin {0}"), FText::FromName(CurrentPinLabel)));
					return EPCGTimeSliceInitResult::NoOperation;
				}
			}

			SourceMetadata[OperandPinIndex] = InputTaggedData[OperandPinIndex].Data->ConstMetadata();
			if (!SourceMetadata[OperandPinIndex])
			{
				// Since this aborts execution, and the user can fix it, it should be a node error
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidInputDataTypeForPin", "Invalid data provided on pin '{0}', must be of type Spatial or Attribute Set."), FText::FromName(CurrentPinLabel)));
				return EPCGTimeSliceInitResult::AbortExecution;
			}
		}

		PCGMetadataOps::FOperationData& OperationData = OutState;
		OperationData.Settings = Settings;
		OperationData.InputAccessors.SetNum(OperandNum);
		OperationData.InputKeys.SetNum(OperandNum);
		OperationData.InputSources.SetNum(OperandNum);
		OperationData.MostComplexInputType = static_cast<uint16>(EPCGMetadataTypes::Unknown);

		const FPCGTaggedData& PrimaryPinData = InputTaggedData[PrimaryPinIndex];

		// First create an accessor for the input to forward (it's our control data)
		PCGMetadataOpPrivate::CreateAccessor(Settings, PrimaryPinData, OperationData, PrimaryPinIndex);
		if (!PCGMetadataOpPrivate::ValidateAccessor(Context, Settings, PrimaryPinData, OperationData, PrimaryPinIndex))
		{
			return EPCGTimeSliceInitResult::AbortExecution;
		}

		// Update the number of elements to process
		OperationData.NumberOfElementsToProcess = OperationData.InputKeys[PrimaryPinIndex]->GetNum();
		if (OperationData.NumberOfElementsToProcess == 0)
		{
			PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("NoElementsInForwardedInput", "No elements in data from forwarded pin '{0}'."), FText::FromName(PrimaryPinData.Pin)));
			return EPCGTimeSliceInitResult::NoOperation;
		}

		// Create the accessors and validate them for each of the other operands
		for (uint32 Index = 0; Index < OperandNum; ++Index)
		{
			if (Index != PrimaryPinIndex)
			{
				// Secondary input class should match the forwarded one
				if (!PCGMetadataOpPrivate::ValidateSecondaryInputClassMatches(PrimaryPinData, InputTaggedData[Index]))
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InputTypeMismatch", "Data on pin '{0}' is not of the same type than on pin '{1}' and is not an Attribute Set. This is not supported."), FText::FromName(InputTaggedData[Index].Pin), FText::FromName(PrimaryPinData.Pin)));
					return EPCGTimeSliceInitResult::AbortExecution;
				}

				PCGMetadataOpPrivate::CreateAccessor(Settings, InputTaggedData[Index], OperationData, Index);

				if (!PCGMetadataOpPrivate::ValidateAccessor(Context, Settings, InputTaggedData[Index], OperationData, Index))
				{
					return EPCGTimeSliceInitResult::AbortExecution;
				}

				const int32 ElementNum = OperationData.InputKeys[Index]->GetNum();

				// No elements on secondary pin, early out for no operation
				if (ElementNum == 0)
				{
					PCGE_LOG(Verbose, LogOnly, FText::Format(LOCTEXT("NoElementsInInput", "No elements in data from secondary pin '{0}'."), FText::FromName(PrimaryPinData.Pin)));
					return EPCGTimeSliceInitResult::NoOperation;
				}

				// Verify that the number of elements makes sense
				if (OperationData.NumberOfElementsToProcess % ElementNum != 0)
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("MismatchInNumberOfElements", "Mismatch between the number of elements from pin '{0}' ({1}) and from pin '{2}' ({3})."), FText::FromName(PrimaryPinData.Pin), OperationData.NumberOfElementsToProcess, FText::FromName(InputTaggedData[Index].Pin), ElementNum));
					return EPCGTimeSliceInitResult::AbortExecution;
				}

				// If selection is an attribute, get it from the metadata
				FPCGAttributePropertyInputSelector& InputSource = OperationData.InputSources[Index];
				if (InputSource.GetSelection() == EPCGAttributePropertySelection::Attribute)
				{
					SourceAttribute[Index] = SourceMetadata[Index]->GetConstAttribute(InputSource.GetName());
				}
				else
				{
					SourceAttribute[Index] = nullptr;
				}
			}
		}

		// At this point, we verified everything, so we can go forward with the computation, depending on the most complex type
		// So first forward outputs and create the attribute
		OperationData.OutputAccessors.SetNum(Settings->GetResultNum());
		OperationData.OutputKeys.SetNum(Settings->GetResultNum());

		const FPCGAttributePropertyOutputSelector OutputTarget = Settings->OutputTarget.CopyAndFixSource(&OperationData.InputSources[PrimaryPinIndex]);

		// Use implicit capture, since we capture a lot
		auto CreateAttribute = [&]<typename AttributeType>(uint32 OutputIndex, AttributeType DummyOutValue) -> bool
		{
			FPCGTaggedData& OutputData = Outputs.Add_GetRef(InputTaggedData[PrimaryPinIndex]);
			OutputData.Pin = Settings->GetOutputPinLabel(OutputIndex);

			UPCGMetadata* OutMetadata = nullptr;

			const FName OutputName = OutputTarget.GetName();
			const FText OutputTargetText = OutputTarget.GetDisplayText();

			if (OutputTarget.GetSelection() == EPCGAttributePropertySelection::Attribute && OutputTarget.GetExtraNames().IsEmpty())
			{
				// In case of an attribute, we check if we have extra selectors. If not, we can just delete the attribute
				// and create a new one of the right type.
				// But if we have extra selectors, we need to handle it the same way as properties.
				// There is no point of failure before duplicating. So duplicate, create the attribute and then the accessor.
				PCGMetadataElementCommon::DuplicateTaggedData(InputTaggedData[PrimaryPinIndex], OutputData, OutMetadata);
				FPCGMetadataAttributeBase* OutputAttribute = PCGMetadataElementCommon::ClearOrCreateAttribute<AttributeType>(OutMetadata, OutputName);
				if (!OutputAttribute)
				{
					return false;
				}

				// And copy the mapping from the original attribute, if it is not points
				if (!InputTaggedData[PrimaryPinIndex].Data->IsA<UPCGPointData>() && SourceMetadata[PrimaryPinIndex] && SourceAttribute[PrimaryPinIndex])
				{
					PCGMetadataElementCommon::CopyEntryToValueKeyMap(SourceMetadata[PrimaryPinIndex], SourceAttribute[PrimaryPinIndex], OutputAttribute);
				}

				OperationData.OutputAccessors[OutputIndex] = PCGAttributeAccessorHelpers::CreateAccessor(const_cast<UPCGData*>(OutputData.Data.Get()), OutputTarget);
			}
			else
			{
				// In case of property or attribute with extra accessor, we need to validate that the property/attribute can accept the output type.
				// Verify this before duplicating, because an extra allocation is certainly less costly than duplicating the data.
				// Do it with a const accessor, since OutputData.Data is still pointing on the const input data.
				const TUniquePtr<const IPCGAttributeAccessor> TempConstAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(OutputData.Data.Get(), OutputTarget);

				if (TempConstAccessor.IsValid())
				{
					// We matched an attribute/property, check if the output type is valid.
					if (!PCG::Private::IsBroadcastable(PCG::Private::MetadataTypes<AttributeType>::Id, TempConstAccessor->GetUnderlyingType()))
					{
						PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeTypeBroadcastFailed_Updated", "Attribute/Property '{0}' ({1}) is not compatible with operation output type ({2})."),
							OutputTargetText,
							PCG::Private::GetTypeNameText(TempConstAccessor->GetUnderlyingType()),
							PCG::Private::GetTypeNameText<AttributeType>()));
						return false;
					}

					PCGMetadataElementCommon::DuplicateTaggedData(InputTaggedData[PrimaryPinIndex], OutputData, OutMetadata);

					// Re-create the accessor to point to the right data (since we just duplicated the data)
					OperationData.OutputAccessors[OutputIndex] = PCGAttributeAccessorHelpers::CreateAccessor(const_cast<UPCGData*>(OutputData.Data.Get()), OutputTarget);
				}
			}

			if (!OperationData.OutputAccessors[OutputIndex].IsValid())
			{
				return false;
			}

			if (OperationData.OutputAccessors[OutputIndex]->IsReadOnly())
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("OutputAccessorIsReadOnly", "Attribute/Property '{0}' is read only."), OutputTargetText));
				return false;
			}

			OperationData.OutputKeys[OutputIndex] = PCGAttributeAccessorHelpers::CreateKeys(const_cast<UPCGData*>(OutputData.Data.Get()), OutputTarget);

			return OperationData.OutputKeys[OutputIndex].IsValid();
		};

		auto CreateAllSameAttributes = [NumberOfResults, &CreateAttribute](auto DummyOutValue) -> bool
		{
			for (uint32 i = 0; i < NumberOfResults; ++i)
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
			check(OutputTypes.Num() == NumberOfResults);

			for (uint32 i = 0; i < NumberOfResults && bCreateAttributeSucceeded; ++i)
			{
				bCreateAttributeSucceeded &= PCGMetadataAttribute::CallbackWithRightType(OutputTypes[i], [&CreateAttribute, i](auto DummyOutValue) -> bool
				{
					return CreateAttribute(i, DummyOutValue);
				});
			}
		}

		if (!bCreateAttributeSucceeded)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("ErrorCreatingOutputAttributes", "Error while creating output attributes"));
			return EPCGTimeSliceInitResult::AbortExecution;
		}

		OperationData.Settings = Settings;

		return EPCGTimeSliceInitResult::Success;
	});

	return true;
}

bool FPCGMetadataElementBase::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataElementBase::Execute);
	ContextType* TimeSlicedContext = static_cast<ContextType*>(Context);
	check(TimeSlicedContext);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	// Prepare data failed, no need to execute. Return an empty output
	if (!TimeSlicedContext->DataIsPreparedForExecution())
	{
		return true;
	}

	return ExecuteSlice(TimeSlicedContext, [this, &Outputs](ContextType* Context, const ExecStateType& ExecState, IterStateType& IterState, const uint32 IterationIndex) -> bool
	{
		// No operation, so skip the iteration.
		if (Context->GetIterationStateResult(IterationIndex) == EPCGTimeSliceInitResult::NoOperation)
		{
			PassthroughInput(Context, Outputs, IterationIndex);
			return true;
		}

		// TODO: Add range-based async evaluation to the DoOperation function in the future
		if (!DoOperation(IterState))
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("ErrorOccurred", "Error while performing the metadata operation, check logs for more information"));
			PassthroughInput(Context, Outputs, IterationIndex);
		}

		return true;
	});
}

#undef LOCTEXT_NAMESPACE
