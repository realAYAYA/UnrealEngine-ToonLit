// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"

#include "Elements/Metadata/PCGMetadataOpElementBase.h"
#include "Elements/Metadata/PCGMetadataMakeTransform.h"

#include "PCGMetadataBreakTransform.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGMetadataBreakTransformSettings : public UPCGMetadataSettingsBase
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
	virtual FName GetOutputAttributeName(FName BaseName, uint32 Index) const override;

	virtual bool HasDifferentOutputTypes() const override;
	virtual TArray<uint16> GetAllOutputTypes() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName InputAttributeName = NAME_None;
};

class FPCGMetadataBreakTransformElement : public FPCGMetadataElementBase
{
protected:
	virtual bool DoOperation(FOperationData& OperationData) const override;
};
