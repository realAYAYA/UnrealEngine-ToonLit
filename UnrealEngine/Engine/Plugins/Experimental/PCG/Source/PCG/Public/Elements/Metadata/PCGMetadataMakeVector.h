// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGMetadataMakeVector.generated.h"

namespace PCGMetadataMakeVectorConstants
{
	const FName XLabel = TEXT("X");
	const FName YLabel = TEXT("Y");
	const FName ZLabel = TEXT("Z");
	const FName WLabel = TEXT("W");

	const FName XYLabel = TEXT("XY");
	const FName ZWLabel = TEXT("ZW");

	const FName XYZLabel = TEXT("XYZ");
}

UENUM()
enum class EPCGMetadataMakeVector3 : uint8
{
	ThreeValues,
	Vector2AndValue
};

UENUM()
enum class EPCGMetadataMakeVector4 : uint8
{
	FourValues,
	Vector2AndTwoValues,
	TwoVector2,
	Vector3AndValue
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataMakeVectorSettings : public UPCGMetadataSettingsBase
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	//~Begin UPCGSettings interface
	virtual FName GetDefaultNodeName() const override;
	//~End UPCGSettings interface
#endif

	virtual FName GetInputAttributeNameWithOverride(uint32 Index, UPCGParamData* Params) const override;

	virtual FName GetInputPinLabel(uint32 Index) const override;
	virtual uint32 GetInputPinNum() const override;

	virtual bool IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const override;
	virtual uint16 GetOutputType(uint16 InputTypeId) const override;

protected:
	//~Begin UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input1AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input2AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input3AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input4AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (ValidEnumValues = "Vector2, Vector, Vector4"))
	EPCGMetadataTypes OutputType = EPCGMetadataTypes::Vector2;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "OutputType == EPCGMetadataTypes::Vector", EditConditionHides))
	EPCGMetadataMakeVector3 MakeVector3Op = EPCGMetadataMakeVector3::ThreeValues;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "OutputType == EPCGMetadataTypes::Vector4", EditConditionHides))
	EPCGMetadataMakeVector4 MakeVector4Op = EPCGMetadataMakeVector4::FourValues;
};

class FPCGMetadataMakeVectorElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
