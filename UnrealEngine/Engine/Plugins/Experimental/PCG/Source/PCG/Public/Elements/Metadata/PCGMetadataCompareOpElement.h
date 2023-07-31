// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"
#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGMetadataCompareOpElement.generated.h"

UENUM()
enum class EPCGMedadataCompareOperation : uint16
{
	Equal,
	NotEqual,
	Greater,
	GreaterOrEqual,
	Less,
	LessOrEqual
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataCompareSettings : public UPCGMetadataSettingsBase
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
	virtual uint16 GetOutputType(uint16 InputTypeId) const override;
	//~End UPCGMetadataSettingsBase interface

protected:
	//~Begin UPCGSettings interface
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGMedadataCompareOperation Operation = EPCGMedadataCompareOperation::Equal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input1AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Input)
	FName Input2AttributeName = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "Operation == EPCGMedadataCompareOperation::Equal || Operation == EPCGMedadataCompareOperation::NotEqual", EditConditionHides))
	double Tolerance = UE_DOUBLE_SMALL_NUMBER;
};

class FPCGMetadataCompareElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
