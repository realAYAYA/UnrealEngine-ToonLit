// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "Elements/Metadata/PCGMetadataOpElementBase.h"

#include "PCGMetadataBreakVector.generated.h"

namespace PCGMetadataBreakVectorConstants
{
	const FName XLabel = TEXT("X");
	const FName YLabel = TEXT("Y");
	const FName ZLabel = TEXT("Z");
	const FName WLabel = TEXT("W");
}

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataBreakVectorSettings : public UPCGMetadataSettingsBase
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
#endif

	virtual FName GetInputAttributeNameWithOverride(uint32 Index, UPCGParamData* Params) const override;

	virtual FName GetOutputPinLabel(uint32 Index) const override;
	virtual uint32 GetOutputPinNum() const override;

	virtual bool IsSupportedInputType(uint16 TypeId, uint32 InputIndex, bool& bHasSpecialRequirement) const override;
	virtual uint16 GetOutputType(uint16 InputTypeId) const override;
	virtual FName GetOutputAttributeName(FName BaseName, uint32 Index) const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName InputAttributeName = NAME_None;
};

class FPCGMetadataBreakVectorElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
