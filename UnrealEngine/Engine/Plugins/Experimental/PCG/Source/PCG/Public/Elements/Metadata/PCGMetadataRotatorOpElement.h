// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"
#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGMetadataRotatorOpElement.generated.h"

UENUM()
enum class EPCGMedadataRotatorOperation : uint16
{
	RotatorOp = 0 UMETA(Hidden),
	Combine,
	Invert,
	Lerp,

	TransformOp = 100 UMETA(Hidden),
	TransformRotation,
	InverseTransformRotation
};

namespace PCGMetadataRotatorHelpers
{
	// Taken from Kismet Math Library
	FRotator RLerp(const FRotator& A, const FRotator& B, double Alpha, bool bShortestPath);
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataRotatorSettings : public UPCGMetadataSettingsBase
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
	EPCGMedadataRotatorOperation Operation = EPCGMedadataRotatorOperation::Combine;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input1AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input, meta = (EditCondition = "Operation != EPCGMedadataTrigOperation::Invert", EditConditionHides))
	FName Input2AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input, meta = (EditCondition = "Operation == EPCGMedadataTrigOperation::Lerp", EditConditionHides))
	FName Input3AttributeName = NAME_None;
};

class FPCGMetadataRotatorElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
