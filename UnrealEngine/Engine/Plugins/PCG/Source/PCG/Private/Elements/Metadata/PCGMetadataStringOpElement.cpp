// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataStringOpElement.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataStringOpElement)

FName UPCGMetadataStringOpSettings::GetInputPinLabel(uint32 Index) const
{
	switch (Index)
	{
	case 0: return PCGMetadataSettingsBaseConstants::DoubleInputFirstLabel;
	case 1: return PCGMetadataSettingsBaseConstants::DoubleInputSecondLabel;
	default: return NAME_None;
	}
}

uint32 UPCGMetadataStringOpSettings::GetOperandNum() const
{
	return 2;
}

bool UPCGMetadataStringOpSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	bHasSpecialRequirement = false;
	return true; // all types support ToString()
}

FPCGAttributePropertyInputSelector UPCGMetadataStringOpSettings::GetInputSource(uint32 Index) const
{
	switch (Index)
	{
	case 0: return InputSource1;
	case 1: return InputSource2;
	default: return FPCGAttributePropertyInputSelector();
	}
}

FString UPCGMetadataStringOpSettings::GetAdditionalTitleInformation() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGMetadataStringOperation>())
	{
		return FString("String: ") + EnumPtr->GetNameStringByValue(static_cast<int>(Operation));
	}
	else
	{
		return FString();
	}
}
 
#if WITH_EDITOR
FName UPCGMetadataStringOpSettings::GetDefaultNodeName() const
{
	return TEXT("AttributeStringOp");
}

FText UPCGMetadataStringOpSettings::GetDefaultNodeTitle() const
{
	return NSLOCTEXT("PCGMetadataStringOpSettings", "NodeTitle", "Attribute String Op");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGMetadataStringOpSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataStringOpElement>();
}

uint16 UPCGMetadataStringOpSettings::GetOutputType(uint16 InputTypeId) const
{
	return (uint16)EPCGMetadataTypes::String;
}

bool FPCGMetadataStringOpElement::DoOperation(PCGMetadataOps::FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataStringOpElement::Execute);

	const UPCGMetadataStringOpSettings* Settings = CastChecked<UPCGMetadataStringOpSettings>(OperationData.Settings);

	if (Settings->Operation == EPCGMetadataStringOperation::Append)
	{
		DoBinaryOp<FString, FString>(OperationData, [](const FString& Value1, const FString& Value2) -> FString { return Value1 + Value2; });
	}

	return true;
}