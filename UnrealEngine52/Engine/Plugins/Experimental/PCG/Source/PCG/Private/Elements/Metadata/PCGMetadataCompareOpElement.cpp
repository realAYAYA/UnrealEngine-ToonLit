// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataCompareOpElement.h"

#include "Metadata/PCGMetadataAttributeTpl.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataCompareOpElement)

namespace PCGMetadataCompareSettings
{
	template <typename T>
	bool ApplyCompare(const T& Input1, const T& Input2, EPCGMedadataCompareOperation Operation, double Tolerance)
	{
		if (Operation == EPCGMedadataCompareOperation::Equal)
		{
			return PCG::Private::MetadataTraits<T>::Equal(Input1, Input2);
		}
		else if (Operation == EPCGMedadataCompareOperation::NotEqual)
		{
			return !PCG::Private::MetadataTraits<T>::Equal(Input1, Input2);
		}

		if constexpr (PCG::Private::MetadataTraits<T>::CanCompare)
		{
			switch (Operation)
			{
			case EPCGMedadataCompareOperation::Greater:
				return PCG::Private::MetadataTraits<T>::Greater(Input1, Input2);
			case EPCGMedadataCompareOperation::GreaterOrEqual:
				return PCG::Private::MetadataTraits<T>::GreaterOrEqual(Input1, Input2);
			case EPCGMedadataCompareOperation::Less:
				return PCG::Private::MetadataTraits<T>::Less(Input1, Input2);
			case EPCGMedadataCompareOperation::LessOrEqual:
				return PCG::Private::MetadataTraits<T>::LessOrEqual(Input1, Input2);
			default:
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}

void UPCGMetadataCompareSettings::PostLoad()
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

FName UPCGMetadataCompareSettings::GetInputPinLabel(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return PCGMetadataSettingsBaseConstants::DoubleInputFirstLabel;
	case 1:
		return PCGMetadataSettingsBaseConstants::DoubleInputSecondLabel;
	default:
		return NAME_None;
	}
}

uint32 UPCGMetadataCompareSettings::GetInputPinNum() const
{
	return 2;
}

bool UPCGMetadataCompareSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	bHasSpecialRequirement = false;
	if (Operation == EPCGMedadataCompareOperation::Equal || Operation == EPCGMedadataCompareOperation::NotEqual)
	{
		return PCG::Private::IsPCGType(TypeId);
	}
	else
	{
		auto IsValid = [](auto Dummy) -> bool
		{
			return PCG::Private::MetadataTraits<decltype(Dummy)>::CanCompare;
		};

		return PCGMetadataAttribute::CallbackWithRightType(TypeId, IsValid);
	}
}

FPCGAttributePropertySelector UPCGMetadataCompareSettings::GetInputSource(uint32 Index) const
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

uint16 UPCGMetadataCompareSettings::GetOutputType(uint16 InputTypeId) const
{
	return (uint16)EPCGMetadataTypes::Boolean;
}

FName UPCGMetadataCompareSettings::AdditionalTaskName() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGMedadataCompareOperation>())
	{
		return FName(FString("Compare: ") + EnumPtr->GetNameStringByValue(static_cast<int>(Operation)));
	}
	else
	{
		return NAME_None;
	}
}

#if WITH_EDITOR
FName UPCGMetadataCompareSettings::GetDefaultNodeName() const
{
	return TEXT("AttributeCompareOp");
}

FText UPCGMetadataCompareSettings::GetDefaultNodeTitle() const
{
	return NSLOCTEXT("PCGMetadataCompareSettings", "NodeTitle", "Attribute Compare Op");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGMetadataCompareSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataCompareElement>();
}

bool FPCGMetadataCompareElement::DoOperation(FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataTrigElement::Execute);

	const UPCGMetadataCompareSettings* Settings = CastChecked<UPCGMetadataCompareSettings>(OperationData.Settings);

	auto CompareFunc = [this, Operation = Settings->Operation, Tolerance = Settings->Tolerance, &OperationData](auto DummyValue) -> bool
	{
		using AttributeType = decltype(DummyValue);

		return DoBinaryOp<AttributeType, AttributeType>(OperationData, 
			[Operation, Tolerance](const AttributeType& Value1, const AttributeType& Value2) -> bool { 
				return PCGMetadataCompareSettings::ApplyCompare(Value1, Value2, Operation, Tolerance); 
			});
	};

	return PCGMetadataAttribute::CallbackWithRightType(OperationData.MostComplexInputType, CompareFunc);
}
