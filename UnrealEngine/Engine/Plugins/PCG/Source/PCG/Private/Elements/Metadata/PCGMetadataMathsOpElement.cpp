// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataMathsOpElement.h"

#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

#include "Elements/Metadata/PCGMetadataMaths.inl"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataMathsOpElement)

namespace PCGMetadataMathsSettings
{
	inline constexpr bool IsUnaryOp(EPCGMetadataMathsOperation Operation)
	{
		return !!(Operation & EPCGMetadataMathsOperation::UnaryOp);
	}

	inline constexpr bool IsBinaryOp(EPCGMetadataMathsOperation Operation)
	{
		return !!(Operation & EPCGMetadataMathsOperation::BinaryOp);
	}

	inline constexpr bool IsTernaryOp(EPCGMetadataMathsOperation Operation)
	{
		return !!(Operation & EPCGMetadataMathsOperation::TernaryOp);
	}

	inline FName GetFirstPinLabel(EPCGMetadataMathsOperation Operation)
	{
		if (PCGMetadataMathsSettings::IsUnaryOp(Operation)
			|| Operation == EPCGMetadataMathsOperation::Clamp
			|| Operation == EPCGMetadataMathsOperation::ClampMin
			|| Operation == EPCGMetadataMathsOperation::ClampMax)
		{
			return PCGPinConstants::DefaultInputLabel;
		}

		if (PCGMetadataMathsSettings::IsBinaryOp(Operation)
			|| Operation == EPCGMetadataMathsOperation::Lerp)
		{
			return PCGMetadataSettingsBaseConstants::DoubleInputFirstLabel;
		}

		return NAME_None;
	}

	inline FName GetSecondPinLabel(EPCGMetadataMathsOperation Operation)
	{
		if (Operation == EPCGMetadataMathsOperation::ClampMin || Operation == EPCGMetadataMathsOperation::Clamp)
		{
			return PCGMetadataSettingsBaseConstants::ClampMinLabel;
		}

		if (Operation == EPCGMetadataMathsOperation::ClampMax)
		{
			return PCGMetadataSettingsBaseConstants::ClampMaxLabel;
		}

		if (PCGMetadataMathsSettings::IsBinaryOp(Operation) || PCGMetadataMathsSettings::IsTernaryOp(Operation))
		{
			return PCGMetadataSettingsBaseConstants::DoubleInputSecondLabel;
		}

		return NAME_None;
	}

	inline FName GetThirdPinLabel(EPCGMetadataMathsOperation Operation)
	{
		if (Operation == EPCGMetadataMathsOperation::Clamp)
		{
			return PCGMetadataSettingsBaseConstants::ClampMaxLabel;
		}

		if (Operation == EPCGMetadataMathsOperation::Lerp)
		{
			return PCGMetadataSettingsBaseConstants::LerpRatioLabel;
		}

		return NAME_None;
	}

	template <typename T>
	T UnaryOp(const T& Value, EPCGMetadataMathsOperation Op)
	{
		switch (Op)
		{
		case EPCGMetadataMathsOperation::Sign:
			return PCGMetadataMaths::Sign(Value);
		case EPCGMetadataMathsOperation::Frac:
			return PCGMetadataMaths::Frac(Value);
		case EPCGMetadataMathsOperation::Truncate:
			return PCGMetadataMaths::Truncate(Value);
		case EPCGMetadataMathsOperation::Round:
			return PCGMetadataMaths::Round(Value);
		case EPCGMetadataMathsOperation::Sqrt:
			return PCGMetadataMaths::Sqrt(Value);
		case EPCGMetadataMathsOperation::Abs:
			return PCGMetadataMaths::Abs(Value);
		case EPCGMetadataMathsOperation::Floor:
			return PCGMetadataMaths::Floor(Value);
		case EPCGMetadataMathsOperation::Ceil:
			return PCGMetadataMaths::Ceil(Value);
		case EPCGMetadataMathsOperation::OneMinus:
			return PCGMetadataMaths::OneMinus(Value);
		default:
			return T{};
		}
	}

	template <typename T>
	T BinaryOp(const T& Value1, const T& Value2, EPCGMetadataMathsOperation Op)
	{
		switch (Op)
		{
		case EPCGMetadataMathsOperation::Add:
			return Value1 + Value2;
		case EPCGMetadataMathsOperation::Subtract:
			return Value1 - Value2;
		case EPCGMetadataMathsOperation::Multiply:
			return Value1 * Value2;
		case EPCGMetadataMathsOperation::Divide:
			return (Value2 != T{0}) ? (Value1 / Value2) : T{0}; // To mirror FMath
		case EPCGMetadataMathsOperation::Max:
			return PCGMetadataMaths::Max(Value1, Value2);
		case EPCGMetadataMathsOperation::Min:
			return PCGMetadataMaths::Min(Value1, Value2);
		case EPCGMetadataMathsOperation::ClampMin:
			return PCGMetadataMaths::Clamp(Value1, Value2, Value1);
		case EPCGMetadataMathsOperation::ClampMax:
			return PCGMetadataMaths::Clamp(Value1, Value1, Value2);
		case EPCGMetadataMathsOperation::Pow:
			return PCGMetadataMaths::Pow(Value1, Value2);
		case EPCGMetadataMathsOperation::Modulo:
			return PCGMetadataMaths::Modulo(Value1, Value2);
		case EPCGMetadataMathsOperation::Set:
			return Value2;
		default:
			return T{};
		}
	}

	template <typename T>
	T TernaryOp(const T& Value1, const T& Value2, const T& Value3, EPCGMetadataMathsOperation Op)
	{
		switch (Op)
		{
		case EPCGMetadataMathsOperation::Clamp:
			return PCGMetadataMaths::Clamp(Value1, Value2, Value3);
		case EPCGMetadataMathsOperation::Lerp:
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

uint32 UPCGMetadataMathsSettings::GetOperandNum() const
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

bool UPCGMetadataMathsSettings::ShouldForceOutputToInt(uint16 InputTypeId) const
{
	return PCG::Private::IsOfTypes<float, double>(InputTypeId) && bForceRoundingOpToInt &&
		(Operation == EPCGMetadataMathsOperation::Round ||
		Operation == EPCGMetadataMathsOperation::Truncate ||
		Operation == EPCGMetadataMathsOperation::Floor ||
		Operation == EPCGMetadataMathsOperation::Ceil);
}

bool UPCGMetadataMathsSettings::ShouldForceOutputToDouble(uint16 InputTypeId) const
{
	return PCG::Private::IsOfTypes<int32, int64>(InputTypeId) && bForceOpToDouble &&
		(Operation == EPCGMetadataMathsOperation::Divide ||
		Operation == EPCGMetadataMathsOperation::Sqrt ||
		Operation == EPCGMetadataMathsOperation::Pow ||
		Operation == EPCGMetadataMathsOperation::Lerp);
}

uint16 UPCGMetadataMathsSettings::GetOutputType(uint16 InputTypeId) const
{
	// If attribute Type is a float or double, can convert to int if it is a rounding op.
	if (ShouldForceOutputToInt(InputTypeId))
	{
		return PCG::Private::MetadataTypes<int64>::Id;
	}
	// If attribute Type is an integer, can convert to double if it is an operation that can yield a floating point value.
	else if (ShouldForceOutputToDouble(InputTypeId))
	{
		return PCG::Private::MetadataTypes<double>::Id;
	}
	else
	{
		return InputTypeId;
	}
}

FPCGAttributePropertyInputSelector UPCGMetadataMathsSettings::GetInputSource(uint32 Index) const
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
		return FPCGAttributePropertyInputSelector();
	}
}

FString UPCGMetadataMathsSettings::GetAdditionalTitleInformation() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGMetadataMathsOperation>())
	{
		return EnumPtr->GetNameStringByValue(static_cast<int>(Operation));
	}
	else
	{
		return FString();
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

TArray<FPCGPreConfiguredSettingsInfo> UPCGMetadataMathsSettings::GetPreconfiguredInfo() const
{
	return PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum<EPCGMetadataMathsOperation>({ EPCGMetadataMathsOperation::UnaryOp, EPCGMetadataMathsOperation::BinaryOp, EPCGMetadataMathsOperation::TernaryOp });
}
#endif // WITH_EDITOR

void UPCGMetadataMathsSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfiguredInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGMetadataMathsOperation>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfiguredInfo.PreconfiguredIndex))
		{
			Operation = EPCGMetadataMathsOperation(PreconfiguredInfo.PreconfiguredIndex);
		}
	}
}

FPCGElementPtr UPCGMetadataMathsSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataMathsElement>();
}

bool FPCGMetadataMathsElement::DoOperation(PCGMetadataOps::FOperationData& OperationData) const
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
				// For int64 as output of the lambda if the output type is int64, as AttributeType might be different (cf GetOutputType)
				using OverriddenOutputType = typename std::conditional_t<PCG::Private::IsOfTypes<AttributeType, float, double>(), int64, AttributeType>;

				if (OperationData.OutputType == PCG::Private::MetadataTypes<int64>::Id)
				{
					DoUnaryOp<AttributeType>(OperationData, [Operation](const AttributeType& Value) -> OverriddenOutputType { return static_cast<OverriddenOutputType>(PCGMetadataMathsSettings::UnaryOp<AttributeType>(Value, Operation)); });
				}
				else
				{
					DoUnaryOp<AttributeType>(OperationData, [Operation](const AttributeType& Value) -> AttributeType { return PCGMetadataMathsSettings::UnaryOp(Value, Operation); });
				}
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

	// If the output is double, force all to double.
	if (OperationData.OutputType == PCG::Private::MetadataTypes<double>::Id)
	{
		MathFunc(double{});
	}
	else
	{
		PCGMetadataAttribute::CallbackWithRightType(OperationData.MostComplexInputType, MathFunc);
	}

	return true;
}
