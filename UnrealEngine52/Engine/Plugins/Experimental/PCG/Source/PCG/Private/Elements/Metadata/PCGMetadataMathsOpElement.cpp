// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataMathsOpElement.h"

#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Elements/Metadata/PCGMetadataMaths.inl"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataMathsOpElement)

namespace PCGMetadataMathsSettings
{
	inline constexpr bool IsUnaryOp(EPCGMedadataMathsOperation Operation)
	{
		return !!(Operation & EPCGMedadataMathsOperation::UnaryOp);
	}

	inline constexpr bool IsBinaryOp(EPCGMedadataMathsOperation Operation)
	{
		return !!(Operation & EPCGMedadataMathsOperation::BinaryOp);
	}

	inline constexpr bool IsTernaryOp(EPCGMedadataMathsOperation Operation)
	{
		return !!(Operation & EPCGMedadataMathsOperation::TernaryOp);
	}

	inline FName GetFirstPinLabel(EPCGMedadataMathsOperation Operation)
	{
		if (PCGMetadataMathsSettings::IsUnaryOp(Operation)
			|| Operation == EPCGMedadataMathsOperation::Clamp
			|| Operation == EPCGMedadataMathsOperation::ClampMin
			|| Operation == EPCGMedadataMathsOperation::ClampMax)
		{
			return PCGPinConstants::DefaultInputLabel;
		}

		if (PCGMetadataMathsSettings::IsBinaryOp(Operation)
			|| Operation == EPCGMedadataMathsOperation::Lerp)
		{
			return PCGMetadataSettingsBaseConstants::DoubleInputFirstLabel;
		}

		return NAME_None;
	}

	inline FName GetSecondPinLabel(EPCGMedadataMathsOperation Operation)
	{
		if (Operation == EPCGMedadataMathsOperation::ClampMin || Operation == EPCGMedadataMathsOperation::Clamp)
		{
			return PCGMetadataSettingsBaseConstants::ClampMinLabel;
		}

		if (Operation == EPCGMedadataMathsOperation::ClampMax)
		{
			return PCGMetadataSettingsBaseConstants::ClampMaxLabel;
		}

		if (PCGMetadataMathsSettings::IsBinaryOp(Operation) || PCGMetadataMathsSettings::IsTernaryOp(Operation))
		{
			return PCGMetadataSettingsBaseConstants::DoubleInputSecondLabel;
		}

		return NAME_None;
	}

	inline FName GetThirdPinLabel(EPCGMedadataMathsOperation Operation)
	{
		if (Operation == EPCGMedadataMathsOperation::Clamp)
		{
			return PCGMetadataSettingsBaseConstants::ClampMaxLabel;
		}

		if (Operation == EPCGMedadataMathsOperation::Lerp)
		{
			return PCGMetadataSettingsBaseConstants::LerpRatioLabel;
		}

		return NAME_None;
	}

	template <typename T>
	T UnaryOp(const T& Value, EPCGMedadataMathsOperation Op)
	{
		switch (Op)
		{
		case EPCGMedadataMathsOperation::Sign:
			return PCGMetadataMaths::Sign(Value);
		case EPCGMedadataMathsOperation::Frac:
			return PCGMetadataMaths::Frac(Value);
		case EPCGMedadataMathsOperation::Truncate:
			return PCGMetadataMaths::Truncate(Value);
		case EPCGMedadataMathsOperation::Round:
			return PCGMetadataMaths::Round(Value);
		case EPCGMedadataMathsOperation::Sqrt:
			return PCGMetadataMaths::Sqrt(Value);
		case EPCGMedadataMathsOperation::Abs:
			return PCGMetadataMaths::Abs(Value);
		case EPCGMedadataMathsOperation::Floor:
			return PCGMetadataMaths::Floor(Value);
		case EPCGMedadataMathsOperation::Ceil:
			return PCGMetadataMaths::Ceil(Value);
		default:
			return T{};
		}
	}

	template <typename T>
	T BinaryOp(const T& Value1, const T& Value2, EPCGMedadataMathsOperation Op)
	{
		switch (Op)
		{
		case EPCGMedadataMathsOperation::Add:
			return Value1 + Value2;
		case EPCGMedadataMathsOperation::Subtract:
			return Value1 - Value2;
		case EPCGMedadataMathsOperation::Multiply:
			return Value1 * Value2;
		case EPCGMedadataMathsOperation::Divide:
			return Value1 / Value2;
		case EPCGMedadataMathsOperation::Max:
			return PCGMetadataMaths::Max(Value1, Value2);
		case EPCGMedadataMathsOperation::Min:
			return PCGMetadataMaths::Min(Value1, Value2);
		case EPCGMedadataMathsOperation::ClampMin:
			return PCGMetadataMaths::Clamp(Value1, Value2, Value1);
		case EPCGMedadataMathsOperation::ClampMax:
			return PCGMetadataMaths::Clamp(Value1, Value1, Value2);
		case EPCGMedadataMathsOperation::Pow:
			return PCGMetadataMaths::Pow(Value1, Value2);
		case EPCGMedadataMathsOperation::Modulo:
			return PCGMetadataMaths::Modulo(Value1, Value2);
		default:
			return T{};
		}
	}

	template <typename T>
	T TernaryOp(const T& Value1, const T& Value2, const T& Value3, EPCGMedadataMathsOperation Op)
	{
		switch (Op)
		{
		case EPCGMedadataMathsOperation::Clamp:
			return PCGMetadataMaths::Clamp(Value1, Value2, Value3);
		case EPCGMedadataMathsOperation::Lerp:
			return PCGMetadataMaths::Lerp(Value1, Value2, Value3);
		default:
			return T{};
		}
	}
}

void UPCGMetadataMathsSettings::PostLoad()
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

	if (Input3AttributeName_DEPRECATED != NAME_None)
	{
		InputSource3.SetAttributeName(Input3AttributeName_DEPRECATED);
		Input3AttributeName_DEPRECATED = NAME_None;
	}
#endif // WITH_EDITOR
}

FName UPCGMetadataMathsSettings::GetInputPinLabel(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return PCGMetadataMathsSettings::GetFirstPinLabel(Operation);
	case 1:
		return PCGMetadataMathsSettings::GetSecondPinLabel(Operation);
	case 2:
		return PCGMetadataMathsSettings::GetThirdPinLabel(Operation);
	default:
		return NAME_None;
	}
}

uint32 UPCGMetadataMathsSettings::GetInputPinNum() const
{
	if (PCGMetadataMathsSettings::IsUnaryOp(Operation))
	{
		return 1;
	}

	if (PCGMetadataMathsSettings::IsBinaryOp(Operation))
	{
		return 2;
	}

	if (PCGMetadataMathsSettings::IsTernaryOp(Operation))
	{
		return 3;
	}

	return 0;
}

// By default: Float/Double, Int32/Int64, Vector2, Vector, Vector4
bool UPCGMetadataMathsSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	bHasSpecialRequirement = false;
	return PCG::Private::IsOfTypes<float, double, int32, int64, FVector2D, FVector, FVector4>(TypeId);
}

FPCGAttributePropertySelector UPCGMetadataMathsSettings::GetInputSource(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return InputSource1;
	case 1:
		return InputSource2;
	case 2:
		return InputSource3;
	default:
		return FPCGAttributePropertySelector();
	}
}

FName UPCGMetadataMathsSettings::AdditionalTaskName() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGMedadataMathsOperation>())
	{
		return FName(FString("Maths: ") + EnumPtr->GetNameStringByValue(static_cast<int>(Operation)));
	}
	else
	{
		return NAME_None;
	}
}

#if WITH_EDITOR
FName UPCGMetadataMathsSettings::GetDefaultNodeName() const
{
	return TEXT("AttributeMathsOp");
}

FText UPCGMetadataMathsSettings::GetDefaultNodeTitle() const
{
	return NSLOCTEXT("PCGMetadataMathsSettings", "NodeTitle", "Attribute Maths Op");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGMetadataMathsSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataMathsElement>();
}

bool FPCGMetadataMathsElement::DoOperation(FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataMathsElement::Execute);

	const UPCGMetadataMathsSettings* Settings = CastChecked<UPCGMetadataMathsSettings>(OperationData.Settings);

	auto MathFunc = [this, Operation = Settings->Operation, &OperationData](auto DummyOutValue) -> void
	{
		using AttributeType = decltype(DummyOutValue);

		// Need to remove types that would not compile
		if constexpr (!PCG::Private::IsOfTypes<AttributeType, float, double, int32, int64, FVector2D, FVector, FVector4>())
		{
			return;
		}
		else
		{
			if (PCGMetadataMathsSettings::IsUnaryOp(Operation))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataMathsElement::ExecuteInternal::UnaryOp);
				DoUnaryOp<AttributeType>(OperationData, [Operation](const AttributeType& Value) -> AttributeType { return PCGMetadataMathsSettings::UnaryOp(Value, Operation); });
			}
			else if (PCGMetadataMathsSettings::IsBinaryOp(Operation))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataMathsElement::ExecuteInternal::BinaryOp);
				DoBinaryOp<AttributeType, AttributeType>(OperationData, [Operation](const AttributeType& Value1, const AttributeType& Value2) -> AttributeType { return PCGMetadataMathsSettings::BinaryOp(Value1, Value2, Operation); });
			}
			else if (PCGMetadataMathsSettings::IsTernaryOp(Operation))
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataMathsElement::ExecuteInternal::TernaryOp);
				DoTernaryOp<AttributeType, AttributeType, AttributeType>(OperationData, [Operation](const AttributeType& Value1, const AttributeType& Value2, const AttributeType& Value3) -> AttributeType { return PCGMetadataMathsSettings::TernaryOp(Value1, Value2, Value3, Operation); });
			}
		}
	};

	PCGMetadataAttribute::CallbackWithRightType(OperationData.OutputType, MathFunc);

	return true;
}
