// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreateAttribute.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGCreateAttribute)

#define LOCTEXT_NAMESPACE "PCGCreateAttributeElement"

namespace PCGCreateAttributeConstants
{
	const FName NodeNameAddAttribute = TEXT("AddAttribute");
	const FText NodeTitleAddAttribute = LOCTEXT("NodeTitleAddAttribute", "Add Attribute");
	const FName NodeNameCreateAttribute = TEXT("CreateAttribute");
	const FText NodeTitleCreateAttribute = LOCTEXT("NodeTitleCreateAttribute", "Create Attribute");
	const FName AttributesLabel = TEXT("Attributes");
}

void UPCGCreateAttributeSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if ((Type_DEPRECATED != EPCGMetadataTypes::Double) || (DoubleValue_DEPRECATED != 0.0))
	{
		AttributeTypes.Type = Type_DEPRECATED;
		AttributeTypes.DoubleValue = DoubleValue_DEPRECATED;
		AttributeTypes.FloatValue = FloatValue_DEPRECATED;
		AttributeTypes.IntValue = IntValue_DEPRECATED;
		AttributeTypes.Int32Value = Int32Value_DEPRECATED;
		AttributeTypes.Vector2Value = Vector2Value_DEPRECATED;
		AttributeTypes.VectorValue = VectorValue_DEPRECATED;
		AttributeTypes.Vector4Value = Vector4Value_DEPRECATED;
		AttributeTypes.RotatorValue = RotatorValue_DEPRECATED;
		AttributeTypes.QuatValue = QuatValue_DEPRECATED;
		AttributeTypes.TransformValue = TransformValue_DEPRECATED;
		AttributeTypes.BoolValue = BoolValue_DEPRECATED;
		AttributeTypes.StringValue = StringValue_DEPRECATED;
		AttributeTypes.NameValue = NameValue_DEPRECATED;

		Type_DEPRECATED = EPCGMetadataTypes::Double;
		DoubleValue_DEPRECATED = 0.0;
	}
#endif // WITH_EDITOR
}

FName UPCGCreateAttributeSettings::AdditionalTaskName() const
{
	return AdditionalTaskNameInternal(PCGCreateAttributeConstants::NodeNameAddAttribute);
}

#if WITH_EDITOR
FName UPCGCreateAttributeSettings::GetDefaultNodeName() const
{
	return PCGCreateAttributeConstants::NodeNameAddAttribute;
}

FText UPCGCreateAttributeSettings::GetDefaultNodeTitle() const
{
	return PCGCreateAttributeConstants::NodeTitleAddAttribute;
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGCreateAttributeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	EPCGDataType PrimaryInputType = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
	if (PrimaryInputType == EPCGDataType::None)
	{
		// Fall back to Any if no edges, or no overlap in types (which should not normally occur).
		PrimaryInputType = EPCGDataType::Any;
	}

	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, PrimaryInputType);

	if (bFromSourceParam)
	{
		PinProperties.Emplace(PCGCreateAttributeConstants::AttributesLabel, EPCGDataType::Param);
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGCreateAttributeSettings::OutputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultOutputLabel;

	PinProperties.AllowedTypes = GetTypeUnionOfIncidentEdges(PCGPinConstants::DefaultInputLabel);
	if (PinProperties.AllowedTypes == EPCGDataType::None)
	{
		// Nothing connected or incompatible types - output Param.
		PinProperties.AllowedTypes = EPCGDataType::Param;
	}

	return { PinProperties };
}

FPCGElementPtr UPCGCreateAttributeSettings::CreateElement() const
{
	return MakeShared<FPCGCreateAttributeElement>();
}

FName UPCGCreateAttributeSettings::AdditionalTaskNameInternal(FName NodeName) const
{
	if (bFromSourceParam)
	{
		if ((OutputAttributeName == NAME_None) && (SourceParamAttributeName == NAME_None))
		{
			return NodeName;
		}
		else
		{
			const FString AttributeName = ((OutputAttributeName == NAME_None) ? SourceParamAttributeName : OutputAttributeName).ToString();
			return FName(FString::Printf(TEXT("%s %s"), *NodeName.ToString(), *AttributeName));
		}
	}
	else
	{
		return FName(FString::Printf(TEXT("%s: %s"), *OutputAttributeName.ToString(), *AttributeTypes.ToString()));
	}
}

UPCGCreateAttributeSetSettings::UPCGCreateAttributeSetSettings()
{
	// No input pin to grab source param from
	bDisplayFromSourceParamSetting = false;
}

#if WITH_EDITOR
FName UPCGCreateAttributeSetSettings::GetDefaultNodeName() const
{
	return PCGCreateAttributeConstants::NodeNameCreateAttribute;
}

FText UPCGCreateAttributeSetSettings::GetDefaultNodeTitle() const
{
	return PCGCreateAttributeConstants::NodeTitleCreateAttribute;
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGCreateAttributeSetSettings::OutputPinProperties() const
{
	FPCGPinProperties PinProperties;
	PinProperties.Label = PCGPinConstants::DefaultOutputLabel;
	PinProperties.AllowedTypes = EPCGDataType::Param;

	return { PinProperties };
}

FName UPCGCreateAttributeSetSettings::AdditionalTaskName() const
{
	return AdditionalTaskNameInternal(PCGCreateAttributeConstants::NodeNameCreateAttribute);
}

bool FPCGCreateAttributeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGCreateAttributeElement::Execute);

	check(Context);

	const UPCGCreateAttributeSettings* Settings = Context->GetInputSettings<UPCGCreateAttributeSettings>();
	check(Settings);

	TArray<FPCGTaggedData> SourceParams = Context->InputData.GetInputsByPin(PCGCreateAttributeConstants::AttributesLabel);
	UPCGParamData* SourceParamData = nullptr;
	FName SourceParamAttributeName = NAME_None;

	if (Settings->bFromSourceParam)
	{
		if (SourceParams.IsEmpty())
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("ParamNotProvided", "Source attribute was not provided"));
			return true;
		}

		SourceParamData = CastChecked<UPCGParamData>(SourceParams[0].Data);

		if (!SourceParamData->Metadata)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("ParamMissingMetadata", "Source Attribute Set data does not have metadata"));
			return true;
		}

		SourceParamAttributeName = (Settings->SourceParamAttributeName == NAME_None) ? SourceParamData->Metadata->GetLatestAttributeNameOrNone() : Settings->SourceParamAttributeName;

		if (!SourceParamData->Metadata->HasAttribute(SourceParamAttributeName))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ParamMissingAttribute", "Source Attribute Set data does not have an attribute '{0}'"), FText::FromName(SourceParamAttributeName)));
			return true;
		}
	}

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	// Only if we have no input connections we'll switch from "adding attribute" to "creating param data"
	const UPCGPin* InputPin = Context->Node ? Context->Node->GetInputPin(PCGPinConstants::DefaultInputLabel) : nullptr;
	const bool bHasInputConnections = InputPin && InputPin->EdgeCount() > 0;

	// If the input is empty, we will create a new ParamData.
	// We can re-use this newly object as the output
	bool bCanReuseInputData = false;
	if (!bHasInputConnections)
	{
		ensure(Inputs.IsEmpty());

		FPCGTaggedData& NewData = Inputs.Emplace_GetRef();
		NewData.Data = NewObject<UPCGParamData>();
		NewData.Pin = PCGPinConstants::DefaultInputLabel;
		bCanReuseInputData = true;
	}

	for (const FPCGTaggedData& InputTaggedData : Inputs)
	{
		const UPCGData* InputData = InputTaggedData.Data;
		UPCGData* OutputData = nullptr;

		UPCGMetadata* Metadata = nullptr;

		bool bShouldAddNewEntry = false;

		if (const UPCGSpatialData* InputSpatialData = Cast<UPCGSpatialData>(InputData))
		{
			UPCGSpatialData* NewSpatialData = InputSpatialData->DuplicateData(/*bInitializeFromData=*/false);
			NewSpatialData->InitializeFromData(InputSpatialData, /*InMetadataParentOverride=*/ nullptr, /*bInheritMetadata=*/true);

			OutputData = NewSpatialData;
			Metadata = NewSpatialData->Metadata;
		}
		else if (const UPCGParamData* InputParamData = Cast<UPCGParamData>(InputData))
		{
			// If we can reuse input data, it is safe to const_cast, as it was created by ourselves above.
			UPCGParamData* NewParamData = bCanReuseInputData ? const_cast<UPCGParamData*>(InputParamData) : NewObject<UPCGParamData>();
			NewParamData->Metadata->InitializeAsCopy(bCanReuseInputData ? nullptr : InputParamData->Metadata);

			OutputData = NewParamData;
			Metadata = NewParamData->Metadata;

			// In case of param data, we want to add a new entry too, if needed
			bShouldAddNewEntry = true;
		}
		else
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid data as input. Only Spatial and Attribute Set data supported."));
			continue;
		}

		const FName OutputAttributeName = (Settings->bFromSourceParam && Settings->OutputAttributeName == NAME_None) ? SourceParamAttributeName : Settings->OutputAttributeName;

		FPCGMetadataAttributeBase* Attribute = nullptr;

		if (Settings->bFromSourceParam)
		{
			const FPCGMetadataAttributeBase* SourceAttribute = SourceParamData->Metadata->GetConstAttribute(SourceParamAttributeName);
			Attribute = Metadata->CopyAttribute(SourceAttribute, OutputAttributeName, /*bKeepParent=*/false, /*bCopyEntries=*/bShouldAddNewEntry, /*bCopyValues=*/bShouldAddNewEntry);
		}
		else
		{
			Attribute = ClearOrCreateAttribute(Settings, Metadata, &OutputAttributeName);
		}

		if (!Attribute)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingAttribute", "Error while creating attribute '{0}'"), FText::FromName(OutputAttributeName)));
			continue;
		}

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		FPCGTaggedData& Output = Outputs.Emplace_GetRef();
		Output.Data = OutputData;

		// Add a new entry if it is a param data and not from source (because entries are already copied)
		if (bShouldAddNewEntry && !Settings->bFromSourceParam)
		{
			// If the metadata is empty, we need to add a new entry, so set it to PCGInvalidEntryKey.
			// Otherwise, use the entry key 0.
			PCGMetadataEntryKey EntryKey = Metadata->GetItemCountForChild() == 0 ? PCGInvalidEntryKey : 0;
			SetAttribute(Settings, Attribute, Metadata, EntryKey);
		}
	}

	return true;
}

FPCGMetadataAttributeBase* FPCGCreateAttributeElement::ClearOrCreateAttribute(const UPCGCreateAttributeSettings* Settings, UPCGMetadata* Metadata, const FName* OutputAttributeNameOverride) const
{
	check(Metadata);

	auto CreateAttribute = [Settings, Metadata, OutputAttributeNameOverride](auto&& Value) -> FPCGMetadataAttributeBase*
	{
		return PCGMetadataElementCommon::ClearOrCreateAttribute(Metadata, OutputAttributeNameOverride ? *OutputAttributeNameOverride : Settings->OutputAttributeName, std::forward<decltype(Value)>(Value));
	};

	return Settings->AttributeTypes.Dispatcher(CreateAttribute);
}

PCGMetadataEntryKey FPCGCreateAttributeElement::SetAttribute(const UPCGCreateAttributeSettings* Settings, FPCGMetadataAttributeBase* Attribute, UPCGMetadata* Metadata, PCGMetadataEntryKey EntryKey) const
{
	check(Attribute && Metadata);

	auto SetAttribute = [Attribute, EntryKey, Metadata](auto&& Value) -> PCGMetadataEntryKey
	{
		using AttributeType = std::decay_t<decltype(Value)>;

		check(Attribute->GetTypeId() == PCG::Private::MetadataTypes<AttributeType>::Id);

		const PCGMetadataEntryKey FinalKey = (EntryKey == PCGInvalidEntryKey) ? Metadata->AddEntry() : EntryKey;

		static_cast<FPCGMetadataAttribute<AttributeType>*>(Attribute)->SetValue(FinalKey, std::forward<decltype(Value)>(Value));

		return FinalKey;
	};

	return Settings->AttributeTypes.Dispatcher(SetAttribute);
}
#undef LOCTEXT_NAMESPACE
