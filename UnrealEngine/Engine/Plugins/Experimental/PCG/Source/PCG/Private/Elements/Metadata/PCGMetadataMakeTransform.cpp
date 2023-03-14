// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataMakeTransform.h"

#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"

namespace PCGMetadataMakeTransformSettings
{
	template <typename VectorType = FVector4>
	FTransform MakeTransform(const VectorType& Translation, const FQuat& Rotation, const VectorType& Scale)
	{
		return FTransform(Rotation, FVector(Translation), FVector(Scale));
	}

	template <>
	FTransform MakeTransform<FVector>(const FVector& Translation, const FQuat& Rotation, const FVector& Scale)
	{
		return FTransform(Rotation, Translation, Scale);
	}

	template <>
	FTransform MakeTransform<FVector2D>(const FVector2D& Translation, const FQuat& Rotation, const FVector2D& Scale)
	{
		return FTransform(Rotation, FVector(Translation, 0.0), FVector(Scale, 1.0));
	}
}

FName UPCGMetadataMakeTransformSettings::GetInputPinLabel(uint32 Index) const
{
	switch (Index)
	{
	case 0:
		return PCGMetadataTransformConstants::Translation;
	case 1:
		return PCGMetadataTransformConstants::Rotation;
	case 2:
		return PCGMetadataTransformConstants::Scale;
	default:
		return NAME_None;
	}
}

uint32 UPCGMetadataMakeTransformSettings::GetInputPinNum() const
{
	return 3;
}

bool UPCGMetadataMakeTransformSettings::IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const
{
	if (InputIndex == 1)
	{
		bHasSpecialRequirement = true;
		return PCG::Private::IsOfTypes<FRotator, FQuat>(TypeId);
	}
	else
	{
		bHasSpecialRequirement = false;
		return PCG::Private::IsOfTypes<FVector2D, FVector, FVector4>(TypeId);
	}
}

FName UPCGMetadataMakeTransformSettings::GetInputAttributeNameWithOverride(uint32 Index, UPCGParamData* Params) const
{
	switch (Index)
	{
	case 0:
		return PCG_GET_OVERRIDEN_VALUE(this, Input1AttributeName, Params);
	case 1:
		return PCG_GET_OVERRIDEN_VALUE(this, Input2AttributeName, Params);
	case 2:
		return PCG_GET_OVERRIDEN_VALUE(this, Input3AttributeName, Params);
	default:
		return NAME_None;
	}
}

uint16 UPCGMetadataMakeTransformSettings::GetOutputType(uint16 InputTypeId) const
{
	return (uint16)EPCGMetadataTypes::Transform;
}

#if WITH_EDITOR
FName UPCGMetadataMakeTransformSettings::GetDefaultNodeName() const
{
	return TEXT("Make Transform Attribute");
}
#endif // WITH_EDITOR

FPCGElementPtr UPCGMetadataMakeTransformSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataMakeTransformElement>();
}

bool FPCGMetadataMakeTransformElement::DoOperation(FOperationData& OperationData) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataMakeTransformElement::Execute);

	const UPCGMetadataMakeTransformSettings* Settings = CastChecked<UPCGMetadataMakeTransformSettings>(OperationData.Settings);

	auto TransformFunc = [this, &OperationData](auto DummyValue) -> bool
	{
		using AttributeType = decltype(DummyValue);

		if constexpr (!PCG::Private::IsOfTypes<AttributeType, FVector2D, FVector, FVector4>())
		{
			return false;
		}
		else
		{
			return DoTernaryOp<AttributeType, FQuat, AttributeType>(OperationData, PCGMetadataMakeTransformSettings::MakeTransform<AttributeType>);
		}
	};

	return PCGMetadataAttribute::CallbackWithRightType(OperationData.MostComplexInputType, TransformFunc);
}