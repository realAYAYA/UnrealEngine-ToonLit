// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataBooleanOpElement.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataBooleanOpElement)

namespace PCGMetadataBooleanSettings
{
	inline bool UnaryOp(const bool& Value)
	{
		// EPCGMedadataBooleanOperation::Not
		return !Value;
	}

	inline bool BinaryOp(const bool& Value1, const bool& Value2, EPCGMedadataBooleanOperation Operation)
	{
		switch (Operation)
		{
		case EPCGMedadataBooleanOperation::And:
			return (Value1 && Value2);
		case EPCGMedadataBooleanOperation::Or:
			return (Value1 || Value2);
		case EPCGMedadataBooleanOperation::Xor:
			return (Value1 != Value2);
		default:
			return false;
		}
	}
}

void UPCGMetadataBooleanSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (Input1AttributeName_DEPRECATED != NAME_None)
	{
		InputSource1.SetAttributeName(Input1AttributeName_DEPRECATED);
		Input1AttributeName_DEPRECATED = NAME_None;
	}

	if (Input2AttributeName_DEPRECATED != NAME_None)
	{
		InputSource2.SetAttributeName(Input2AttributeName_DEPRECATED);
		Input2AttributeName_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

FName UPCGMetadataBooleanSettings::GetInputPinLabel(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return (Operation == EPCGMedadataBooleanOperation::Not) ? PCGPinConstants::DefaultInputLabel : PCGMetadataSettingsBaseConstants::DoubleInputFirstLabel;
	case 1:
		return PCGMetadataSettingsBaseConstants::DoubleInputSecondLabel;
	default:
		return NAME_None;
	}
}

uint32 UPCGMetadataBooleanSettings::GetInputPinNum() const
{
	return (Operation == EPCGMedadataBooleanOperation::Not) ? 1 : 2;
}

bool UPCGMetadataBooleanSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	bHasSpecialRequirement = false;
	return PCG::Private::IsOfTypes<bool>(TypeId);
}

FPCGAttributePropertySelector UPCGMetadataBooleanSettings::GetInputSource(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return InputSource1;
	case 1:
		return InputSource2;
	default:
		return FPCGAttributePropertySelector();
	}
}

uint16 UPCGMetadataBooleanSettings::GetOutputType(uint16 InputTypeId) const
{
	return (uint16)EPCGMetadataTypes::Boolean;
}

FName UPCGMetadataBooleanSettings::AdditionalTaskName() const
{
	if (const UEnum* EnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/PCG.EPCGMedadataBooleanOperation"), true))
	{
		return FName(FString("Boolean: ") + EnumPtr->GetNameStringByValue(static_cast<int>(Operation)));
	}
	else
	{
		return NAME_None;
	}
}

#if WITH_EDITOR
FName UPCGMetadataBooleanSettings::GetDefaultNodeName() const
{
	return TEXT("AttributeBooleanOp");
}

FText UPCGMetadataBooleanSettings::GetDefaultNodeTitle() const
{
	return NSLOCTEXT("PCGMetadataBooleanSettings", "NodeTitle", "Attribute Boolean Op");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGMetadataBooleanSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataBooleanElement>();
}

bool FPCGMetadataBooleanElement::DoOperation(FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataBooleanElement::Execute);

	const UPCGMetadataBooleanSettings* Settings = CastChecked<UPCGMetadataBooleanSettings>(OperationData.Settings);

	if (Settings->Operation == EPCGMedadataBooleanOperation::Not)
	{
		DoUnaryOp<bool>(OperationData, [](const bool& Value) -> bool { return PCGMetadataBooleanSettings::UnaryOp(Value); });
	}
	else
	{
		DoBinaryOp<bool, bool>(OperationData, [Operation = Settings->Operation](const bool& Value1, const bool& Value2) -> bool { return PCGMetadataBooleanSettings::BinaryOp(Value1, Value2, Operation); });
	}

	return true;
}
