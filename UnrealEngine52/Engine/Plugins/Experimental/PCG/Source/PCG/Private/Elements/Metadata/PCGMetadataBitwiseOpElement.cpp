// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataBitwiseOpElement.h"


#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataBitwiseOpElement)

namespace PCGMetadataBitwiseSettings
{
	inline int64 UnaryOp(const int64& Value)
	{
		// EPCGMedadataBitwiseOperation::Not
		return ~Value;
	}

	inline int64 BinaryOp(const int64& Value1, const int64& Value2, EPCGMedadataBitwiseOperation Operation)
	{
		switch (Operation)
		{
		case EPCGMedadataBitwiseOperation::And:
			return (Value1 & Value2);
		case EPCGMedadataBitwiseOperation::Or:
			return (Value1 | Value2);
		case EPCGMedadataBitwiseOperation::Xor:
			return (Value1 ^ Value2);
		default:
			return 0;
		}
	}
}

void UPCGMetadataBitwiseSettings::PostLoad()
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

FName UPCGMetadataBitwiseSettings::GetInputPinLabel(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return (Operation == EPCGMedadataBitwiseOperation::Not) ? PCGPinConstants::DefaultInputLabel : PCGMetadataSettingsBaseConstants::DoubleInputFirstLabel;
	case 1:
		return PCGMetadataSettingsBaseConstants::DoubleInputSecondLabel;
	default:
		return NAME_None;
	}
}

uint32 UPCGMetadataBitwiseSettings::GetInputPinNum() const
{
	return (Operation == EPCGMedadataBitwiseOperation::Not) ? 1 : 2;
}

bool UPCGMetadataBitwiseSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	bHasSpecialRequirement = false;
	return PCG::Private::IsOfTypes<int32, int64>(TypeId);
}

FPCGAttributePropertySelector UPCGMetadataBitwiseSettings::GetInputSource(uint32 Index) const
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

FName UPCGMetadataBitwiseSettings::AdditionalTaskName() const
{
	if (const UEnum* EnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/PCG.EPCGMedadataBitwiseOperation"), true))
	{
		return FName(FString("Bitwise: ") + EnumPtr->GetNameStringByValue(static_cast<int>(Operation)));
	}
	else
	{
		return NAME_None;
	}
}

#if WITH_EDITOR
FName UPCGMetadataBitwiseSettings::GetDefaultNodeName() const
{
	return TEXT("AttributeBitwiseOp");
}

FText UPCGMetadataBitwiseSettings::GetDefaultNodeTitle() const
{
	return NSLOCTEXT("PCGMetadataBitwiseSettings", "NodeTitle", "Attribute Bitwise Op");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGMetadataBitwiseSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataBitwiseElement>();
}

uint16 UPCGMetadataBitwiseSettings::GetOutputType(uint16 InputTypeId) const
{
	return (uint16)EPCGMetadataTypes::Integer64;
}

bool FPCGMetadataBitwiseElement::DoOperation(FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataBitwiseElement::Execute);

	const UPCGMetadataBitwiseSettings* Settings = CastChecked<UPCGMetadataBitwiseSettings>(OperationData.Settings);

	if (Settings->Operation == EPCGMedadataBitwiseOperation::Not)
	{
		DoUnaryOp<int64>(OperationData, [](const int64& Value) -> int64 { return PCGMetadataBitwiseSettings::UnaryOp(Value); });
	}
	else
	{
		DoBinaryOp<int64, int64>(OperationData, [Operation = Settings->Operation](const int64& Value1, const int64& Value2) -> int64 { return PCGMetadataBitwiseSettings::BinaryOp(Value1, Value2, Operation); });
	}

	return true;
}
