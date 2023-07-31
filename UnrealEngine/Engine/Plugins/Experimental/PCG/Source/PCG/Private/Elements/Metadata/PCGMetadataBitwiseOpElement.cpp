// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataBitwiseOpElement.h"

#include "PCGParamData.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/PCGMetadataEntryKeyIterator.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

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

FName UPCGMetadataBitwiseSettings::GetInputAttributeNameWithOverride(uint32 Index, UPCGParamData* Params) const
{
	switch (Index)
	{
	case 0:
		return PCG_GET_OVERRIDEN_VALUE(this, Input1AttributeName, Params);
	case 1:
		return PCG_GET_OVERRIDEN_VALUE(this, Input2AttributeName, Params);
	default:
		return NAME_None;
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
	return TEXT("Attribute Bitwise Op");
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