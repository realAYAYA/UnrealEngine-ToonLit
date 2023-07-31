// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"
#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGMetadataMathsOpElement.generated.h"

UENUM(Meta=(Bitflags))
enum class EPCGMedadataMathsOperation : uint16
{
	// Unary op
	UnaryOp = 1 << 10 UMETA(Hidden),
	Sign,
	Frac,
	Truncate,
	Round,
	Sqrt,
	Abs,

	// Binary op
	BinaryOp = 1 << 11 UMETA(Hidden),
	Add,
	Subtract,
	Multiply,
	Divide,
	Max,
	Min,
	Pow,
	ClampMin,
	ClampMax,

	// Ternary op
	TernaryOp = 1 << 12 UMETA(Hidden),
	Clamp,
	Lerp,
};
ENUM_CLASS_FLAGS(EPCGMedadataMathsOperation);

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataMathsSettings : public UPCGMetadataSettingsBase
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
#endif
	virtual FName AdditionalTaskName() const override;
	//~End UPCGSettings interface

	//~Begin UPCGMetadataSettingsBase interface
	virtual FName GetInputAttributeNameWithOverride(uint32 Index, UPCGParamData* Params) const override;

	virtual FName GetInputPinLabel(uint32 Index) const override;
	virtual uint32 GetInputPinNum() const override;

	virtual bool IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const override;
	//~End UPCGMetadataSettingsBase interface

protected:
	//~Begin UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGMedadataMathsOperation Operation = EPCGMedadataMathsOperation::Add;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input1AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input, meta = (EditCondition="(Operation & '/Script/PCG.EPCGMedadataMathsOperation::Binary') || (Operation & '/Script/PCG.EPCGMedadataMathsOperation::TernaryOp')", EditConditionHides))
	FName Input2AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input, meta = (EditCondition = "Operation & '/Script/PCG.EPCGMedadataMathsOperation::TernaryOp'", EditConditionHides))
	FName Input3AttributeName = NAME_None;
};

class FPCGMetadataMathsElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
